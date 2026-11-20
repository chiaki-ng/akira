#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace akira::log {

class LogFolder {
public:
    struct Entry {
        std::string name;
        std::filesystem::path path;
        bool directory = false;
        std::uintmax_t size = 0;
        std::time_t modified = 0;

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    static auto list(const std::filesystem::path& folder) -> std::optional<std::vector<Entry>>;
    static void sort(std::vector<Entry>& entries);
    static auto sizeText(std::uintmax_t bytes) -> std::string;
    static auto modifiedText(std::time_t modified) -> std::string;
};

}
