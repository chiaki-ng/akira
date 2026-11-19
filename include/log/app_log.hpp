#pragma once

#include "log/log.hpp"

#include <optional>
#include <string>

namespace akira {

class AppLog {
public:
    static void open(const std::string& path);
    static void close();

private:
    static auto redirect() -> std::optional<log::Log::Redirect>&;
};

}
