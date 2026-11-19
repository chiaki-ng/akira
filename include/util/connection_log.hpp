#pragma once

#include "log/log.hpp"

#include <optional>
#include <string>

namespace akira {

class ConnectionLog {
public:
    static auto open(const std::string& path) -> std::optional<ConnectionLog>;

private:
    explicit ConnectionLog(log::Log::Redirect redirect) noexcept;

    log::Log::Redirect redirect;
};

}
