#include "app/crash_report.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <typeinfo>

extern "C" char __start__[];
extern "C" char __rodata_start[];

namespace akira::app {

CrashReport::Writer CrashReport::s_writer;

void CrashReport::install(const char* appName, const char* version, const char* directory)
{
    s_appName = appName;
    s_version = version;
    s_directory = directory;
    std::set_terminate(&CrashReport::onTerminate);
    createDirectories();
    s_bootPath.fill('\0');
    std::format_to_n(s_bootPath.data(), s_bootPath.size() - 1, "{}/{}-boot.txt", s_directory, s_appName);
    std::array<char, 160> line{};
    const auto out = std::format_to_n(line.data(), line.size() - 1, "{} {} boot trace\n", s_appName, s_version);
    write(s_bootPath.data(), {line.data(), static_cast<std::size_t>(out.size)}, false);
    stage("install");
}

void CrashReport::stage(const char* name)
{
    s_stage.store(name, std::memory_order_release);
    if (!s_bootPath[0])
        return;
    std::array<char, 128> line{};
    const auto out = std::format_to_n(line.data(), line.size() - 1, "{} {}\n", armGetSystemTick(), name);
    write(s_bootPath.data(), {line.data(), std::min<std::size_t>(static_cast<std::size_t>(out.size), line.size() - 1)}, true);
}

void CrashReport::onAbort(std::string_view kind, std::string_view detail, std::uint64_t returnAddress, std::uint64_t frame)
{
    if (s_writing.test_and_set())
        return;
    Writer& writer = s_writer;
    header(writer, kind);
    if (!detail.empty())
        writer.line("{}", detail);
    backtrace(writer, returnAddress, 0, frame);
    save(writer.text());
    echo(writer.text());
}

void CrashReport::onResult(std::string_view kind, Result result, std::uint64_t returnAddress, std::uint64_t frame)
{
    std::array<char, 96> detail{};
    const auto out = std::format_to_n(detail.data(), detail.size() - 1, "result 0x{:x} error {:04}-{:04}", result,
        2000 + R_MODULE(result), R_DESCRIPTION(result));
    onAbort(kind, {detail.data(), std::min<std::size_t>(static_cast<std::size_t>(out.size), detail.size() - 1)}, returnAddress, frame);
}

void CrashReport::onException(const ThreadExceptionDump& dump)
{
    if (s_writing.test_and_set())
        return;
    Writer& writer = s_writer;
    header(writer, "exception");
    writer.line("desc 0x{:x} {}", dump.error_desc, describe(dump.error_desc));
    address(writer, "pc", dump.pc.x);
    address(writer, "lr", dump.lr.x);
    address(writer, "sp", dump.sp.x);
    address(writer, "fp", dump.fp.x);
    writer.line("far 0x{:016x}", dump.far.x);
    writer.line("esr 0x{:08x} pstate 0x{:08x} afsr0 0x{:08x} afsr1 0x{:08x}", dump.esr, dump.pstate, dump.afsr0, dump.afsr1);
    for (std::size_t i = 0; i < 29; i += 2) {
        if (i + 1 < 29)
            writer.line("x{:<2} 0x{:016x}  x{:<2} 0x{:016x}", i, dump.cpu_gprs[i].x, i + 1, dump.cpu_gprs[i + 1].x);
        else
            writer.line("x{:<2} 0x{:016x}", i, dump.cpu_gprs[i].x);
    }
    backtrace(writer, dump.pc.x, dump.lr.x, dump.fp.x);
    save(writer.text());
    echo(writer.text());
}

void CrashReport::onTerminate()
{
    if (!s_writing.test_and_set()) {
        Writer& writer = s_writer;
        header(writer, "terminate");
        if (const std::exception_ptr current = std::current_exception()) {
            try {
                std::rethrow_exception(current);
            } catch (const std::exception& error) {
                writer.line("exception {} what=\"{}\"", typeid(error).name(), error.what());
            } catch (...) {
                writer.line("exception of a non-std type");
            }
        } else {
            writer.line("std::terminate without an active exception");
        }
        const auto fp = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0));
        const auto pc = reinterpret_cast<std::uint64_t>(__builtin_return_address(0));
        backtrace(writer, pc, 0, fp);
        save(writer.text());
        echo(writer.text());
    }
    std::abort();
}

void CrashReport::header(Writer& writer, std::string_view kind)
{
    u64 threadId = 0;
    svcGetThreadId(&threadId, CUR_THREAD_HANDLE);
    writer.line("{} crash report ({})", s_appName, kind);
    writer.line("version {}", s_version);
    writer.line("stage {}", s_stage.load(std::memory_order_acquire));
    writer.line("thread 0x{:x}", threadId);
    writer.line("tick {}", armGetSystemTick());
    writer.line("module 0x{:016x} text 0x{:x}", reinterpret_cast<std::uint64_t>(__start__),
        reinterpret_cast<std::uint64_t>(__rodata_start) - reinterpret_cast<std::uint64_t>(__start__));
}

void CrashReport::address(Writer& writer, std::string_view label, std::uint64_t value)
{
    const auto base = reinterpret_cast<std::uint64_t>(__start__);
    const auto end = reinterpret_cast<std::uint64_t>(__rodata_start);
    if (value >= base && value < end)
        writer.line("{} 0x{:016x} elf+0x{:x}", label, value, value - base);
    else
        writer.line("{} 0x{:016x}", label, value);
}

void CrashReport::backtrace(Writer& writer, std::uint64_t pc, std::uint64_t lr, std::uint64_t fp)
{
    writer.line("backtrace");
    address(writer, "  #0", pc);
    std::size_t frame = 1;
    if (lr != 0) {
        address(writer, "  #1", lr);
        frame = 2;
    }
    while (frame < MaxFrames && fp != 0 && (fp & 0xF) == 0 && readable(fp, 16)) {
        const auto* record = reinterpret_cast<const std::uint64_t*>(fp);
        const std::uint64_t next = record[0];
        const std::uint64_t ret = record[1];
        if (ret == 0)
            break;
        std::array<char, 8> label{};
        std::format_to_n(label.data(), label.size() - 1, "  #{}", frame++);
        address(writer, label.data(), ret);
        if (next <= fp)
            break;
        fp = next;
    }
}

bool CrashReport::readable(std::uint64_t address, std::size_t size)
{
    MemoryInfo info{};
    u32 pageInfo = 0;
    if (R_FAILED(svcQueryMemory(&info, &pageInfo, address)))
        return false;
    return (info.perm & Perm_R) && info.type != MemType_Unmapped && address + size <= info.addr + info.size;
}

std::string_view CrashReport::describe(std::uint32_t errorDesc)
{
    switch (errorDesc) {
    case ThreadExceptionDesc_InstructionAbort:
        return "instruction abort";
    case ThreadExceptionDesc_MisalignedPC:
        return "misaligned pc";
    case ThreadExceptionDesc_MisalignedSP:
        return "misaligned sp";
    case ThreadExceptionDesc_SError:
        return "serror";
    case ThreadExceptionDesc_BadSVC:
        return "bad svc";
    case ThreadExceptionDesc_Trap:
        return "trap";
    case ThreadExceptionDesc_Other:
        return "data abort or other";
    default:
        return "unknown";
    }
}

void CrashReport::save(std::string_view text)
{
    std::array<char, FS_MAX_PATH> path{};
    std::format_to_n(path.data(), path.size() - 1, "{}/{}-crash-{:016x}.txt", s_directory, s_appName, armGetSystemTick());
    write(path.data(), text, false);
}

void CrashReport::createDirectories()
{
    FsFileSystem* sdmc = fsdevGetDeviceFileSystem("sdmc");
    if (!sdmc)
        return;
    const std::string_view directory{s_directory};
    std::array<char, FS_MAX_PATH> path{};
    for (std::size_t end = directory.find('/', 1); ; end = directory.find('/', end + 1)) {
        const std::string_view prefix = directory.substr(0, end);
        path.fill('\0');
        std::ranges::copy(prefix.substr(0, path.size() - 1), path.begin());
        fsFsCreateDirectory(sdmc, path.data());
        if (end == std::string_view::npos)
            break;
    }
}

void CrashReport::write(const char* path, std::string_view text, bool append)
{
    FsFileSystem* sdmc = fsdevGetDeviceFileSystem("sdmc");
    if (!sdmc)
        return;
    if (!append)
        fsFsDeleteFile(sdmc, path);
    fsFsCreateFile(sdmc, path, 0, 0);
    FsFile file{};
    if (R_FAILED(fsFsOpenFile(sdmc, path, FsOpenMode_Write | FsOpenMode_Append, &file)))
        return;
    s64 size = 0;
    if (append)
        fsFileGetSize(&file, &size);
    fsFileWrite(&file, size, text.data(), text.size(), FsWriteOption_Flush);
    fsFileClose(&file);
}

void CrashReport::echo(std::string_view text)
{
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fflush(stderr);
}

}
