#include "util/crash_report.hpp"

#include <format>

extern "C" {

alignas(16) u8 __nx_exception_stack[0x10000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __real_abort();
void __real___assert_func(const char* file, int line, const char* function, const char* expression);
void __real_fatalThrow(Result result);
void __real_fatalThrowWithPolicy(Result result, FatalPolicy policy);

void __libnx_exception_handler(ThreadExceptionDump* dump)
{
    akira::CrashReport::onException(*dump);
}

void diagAbortWithResult(Result result)
{
    akira::CrashReport::onResult("diag abort", result, reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    svcBreak(BreakReason_Panic, reinterpret_cast<uintptr_t>(&result), sizeof(result));
    __builtin_unreachable();
}

void __wrap_fatalThrow(Result result)
{
    akira::CrashReport::onResult("fatal", result, reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real_fatalThrow(result);
    __builtin_unreachable();
}

void __wrap_fatalThrowWithPolicy(Result result, FatalPolicy policy)
{
    akira::CrashReport::onResult("fatal", result, reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real_fatalThrowWithPolicy(result, policy);
    __builtin_unreachable();
}

void __wrap_abort()
{
    akira::CrashReport::onAbort("abort", "", reinterpret_cast<std::uint64_t>(__builtin_return_address(0)),
        reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real_abort();
    __builtin_unreachable();
}

void __wrap___assert_func(const char* file, int line, const char* function, const char* expression)
{
    std::array<char, 384> detail{};
    const auto out = std::format_to_n(detail.data(), detail.size() - 1, "assert {} at {}:{} in {}", expression ? expression : "?",
        file ? file : "?", line, function ? function : "?");
    akira::CrashReport::onAbort("assert", {detail.data(), std::min<std::size_t>(static_cast<std::size_t>(out.size), detail.size() - 1)},
        reinterpret_cast<std::uint64_t>(__builtin_return_address(0)), reinterpret_cast<std::uint64_t>(__builtin_frame_address(0)));
    __real___assert_func(file, line, function, expression);
    __builtin_unreachable();
}

}
