#include "log/connection_log.hpp"

#include <utility>

namespace akira::log {

auto ConnectionLog::open(const std::string& path) -> std::optional<ConnectionLog>
{
    auto redirect = Log::process().redirect(path);
    if (!redirect)
        return std::nullopt;
    return ConnectionLog{std::move(*redirect)};
}

ConnectionLog::ConnectionLog(Log::Redirect redirect) noexcept
    : redirect(std::move(redirect))
{
}

}
