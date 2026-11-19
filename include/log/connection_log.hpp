#pragma once

#include "log/log.hpp"

#include <optional>
#include <string>

namespace akira::log {

class ConnectionLog {
public:
    static auto open(const std::string& path) -> std::optional<ConnectionLog>;

private:
    explicit ConnectionLog(Log::Redirect redirect) noexcept;

    Log::Redirect redirect;
};

}
