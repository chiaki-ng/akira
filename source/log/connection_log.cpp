#include "log/connection_log.hpp"

#include <utility>

namespace akira {

auto ConnectionLog::open(const std::string& path) -> std::optional<ConnectionLog>
{
    auto redirect = log::Log::process().redirect(path);
    if (!redirect)
        return std::nullopt;
    return ConnectionLog{std::move(*redirect)};
}

ConnectionLog::ConnectionLog(log::Log::Redirect redirect) noexcept
    : redirect(std::move(redirect))
{
}

}
