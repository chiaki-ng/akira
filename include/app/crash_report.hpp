#ifndef AKIRA_CRASH_REPORT_HPP
#define AKIRA_CRASH_REPORT_HPP

#include <switch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>

namespace akira::app {

class CrashReport {
public:
    static void install(const char* appName, const char* version, const char* directory);
    static void stage(const char* name);
    static void onException(const ThreadExceptionDump& dump);
    static void onAbort(std::string_view kind, std::string_view detail, std::uint64_t returnAddress, std::uint64_t frame);
    static void onResult(std::string_view kind, Result result, std::uint64_t returnAddress, std::uint64_t frame);

private:
    static constexpr std::size_t MaxFrames = 32;
    static constexpr std::size_t BufferSize = 16 * 1024;

    class Writer {
    public:
        template <class... Args>
        void line(std::format_string<Args...> format, Args&&... args)
        {
            const std::size_t room = m_buffer.size() - m_length;
            if (room < 2)
                return;
            const auto result = std::format_to_n(m_buffer.data() + m_length, room - 1, format, std::forward<Args>(args)...);
            m_length += std::min<std::size_t>(static_cast<std::size_t>(result.size), room - 1);
            m_buffer[m_length++] = '\n';
        }

        std::string_view text() const { return {m_buffer.data(), m_length}; }

    private:
        std::array<char, BufferSize> m_buffer{};
        std::size_t m_length = 0;
    };

    [[noreturn]] static void onTerminate();
    static void header(Writer& writer, std::string_view kind);
    static void address(Writer& writer, std::string_view label, std::uint64_t value);
    static void backtrace(Writer& writer, std::uint64_t pc, std::uint64_t lr, std::uint64_t fp);
    static bool readable(std::uint64_t address, std::size_t size);
    static std::string_view describe(std::uint32_t errorDesc);
    static void save(std::string_view text);
    static void echo(std::string_view text);
    static void createDirectories();
    static void write(const char* path, std::string_view text, bool append);

    static inline const char* s_appName = "app";
    static inline const char* s_version = "";
    static inline const char* s_directory = "/switch";
    static inline std::array<char, FS_MAX_PATH> s_bootPath{};
    static inline std::atomic<const char*> s_stage{"start"};
    static inline std::atomic_flag s_writing = ATOMIC_FLAG_INIT;
    static Writer s_writer;
};

}

#endif
