#pragma once

#include <atomic>
#include <cstdarg>

namespace akira::settings {
class DeveloperSettings;
}

namespace akira::log {

class FfmpegLog {
public:
    explicit FfmpegLog(const settings::DeveloperSettings& developer);
    ~FfmpegLog();

    FfmpegLog(const FfmpegLog&) = delete;
    FfmpegLog& operator=(const FfmpegLog&) = delete;

private:
    static void forward(void* context, int level, const char* format, va_list arguments);

    static inline std::atomic<const settings::DeveloperSettings*> installed{nullptr};
};

}
