#pragma once

#include <cstdio>
#include <optional>
#include <string>

namespace akira {

class ConnectionLog {
public:
    static auto open(const std::string& path) -> std::optional<ConnectionLog>;

    ConnectionLog(ConnectionLog&& other) noexcept;
    ConnectionLog& operator=(ConnectionLog&& other) noexcept;
    ConnectionLog(const ConnectionLog&) = delete;
    ConnectionLog& operator=(const ConnectionLog&) = delete;
    ~ConnectionLog();

private:
    explicit ConnectionLog(FILE* file) noexcept;
    void restore() noexcept;

    FILE* file;
};

}
