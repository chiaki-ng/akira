#pragma once

#include <string>

namespace akira::log {

class LogFiles {
public:
    static constexpr const char* directory = "sdmc:/switch/akira/logs";

    static std::string appLogPath();
    static std::string connectionLogPath(const std::string& connType);

private:
    static void pruneOldLogs();
};

}
