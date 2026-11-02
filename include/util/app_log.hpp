#pragma once

#include <cstdio>
#include <string>

namespace akira {

class AppLog {
public:
    static void open(const std::string& path);
    static void close();
    static auto output() noexcept -> std::FILE*;

private:
    static inline std::FILE* file = nullptr;
};

}
