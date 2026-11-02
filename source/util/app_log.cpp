#include "util/app_log.hpp"

#include <borealis.hpp>

namespace akira {

void AppLog::open(const std::string& path)
{
    if (file)
        return;
    file = std::fopen(path.c_str(), "w");
    if (!file)
        return;
    std::setvbuf(file, nullptr, _IOLBF, 0);
    brls::Logger::setLogOutput(file);
    brls::Logger::info("File logging enabled: {}", path);
}

void AppLog::close()
{
    if (!file)
        return;
    brls::Logger::flushAsyncLogs();
    if (brls::Logger::getLogOutput() == file)
        brls::Logger::setLogOutput(stdout);
    brls::Logger::flushAsyncLogs();
    std::fclose(file);
    file = nullptr;
}

auto AppLog::output() noexcept -> std::FILE*
{
    return file ? file : stdout;
}

}
