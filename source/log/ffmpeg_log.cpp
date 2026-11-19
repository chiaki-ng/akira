#include "log/ffmpeg_log.hpp"

#include <array>
#include <cstdio>
#include <string_view>

extern "C" {
#include <libavutil/log.h>
}

#include "log/log.hpp"
#include "settings/developer_settings.hpp"

namespace akira::log {

FfmpegLog::FfmpegLog(const settings::DeveloperSettings& developer)
{
    installed.store(&developer, std::memory_order_release);
    av_log_set_callback(forward);
}

FfmpegLog::~FfmpegLog()
{
    installed.store(nullptr, std::memory_order_release);
}

void FfmpegLog::forward(void*, int level, const char* format, va_list arguments)
{
    const settings::DeveloperSettings* developer = installed.load(std::memory_order_acquire);
    if (!developer || developer->silenceAllLogs() || !developer->ffmpegLog())
        return;
    if (level > av_log_get_level())
        return;
    std::array<char, 512> buf{};
    vsnprintf(buf.data(), buf.size(), format, arguments);
    std::string_view msg(buf.data());
    if (msg.ends_with('\n'))
        msg.remove_suffix(1);
    if (msg.empty())
        return;
    if (level <= AV_LOG_ERROR)
        error("ffmpeg: {}", msg);
    else if (level <= AV_LOG_WARNING)
        warning("ffmpeg: {}", msg);
    else
        info("ffmpeg: {}", msg);
}

}
