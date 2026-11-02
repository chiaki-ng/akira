#include "util/connection_log.hpp"

#include "util/app_log.hpp"

#include <borealis.hpp>

#include <utility>

namespace akira {

auto ConnectionLog::open(const std::string& path) -> std::optional<ConnectionLog>
{
    FILE* file = std::fopen(path.c_str(), "w");
    if (!file)
        return std::nullopt;
    brls::Logger::setLogOutput(file);
    return ConnectionLog{file};
}

ConnectionLog::ConnectionLog(FILE* file) noexcept
    : file(file)
{
}

ConnectionLog::ConnectionLog(ConnectionLog&& other) noexcept
    : file(std::exchange(other.file, nullptr))
{
}

ConnectionLog& ConnectionLog::operator=(ConnectionLog&& other) noexcept
{
    if (this != &other)
    {
        restore();
        file = std::exchange(other.file, nullptr);
    }
    return *this;
}

ConnectionLog::~ConnectionLog()
{
    restore();
}

void ConnectionLog::restore() noexcept
{
    if (!file)
        return;
    brls::Logger::flushAsyncLogs();
    brls::Logger::setLogOutput(AppLog::output());
    brls::Logger::flushAsyncLogs();
    std::fclose(file);
    file = nullptr;
}

}
