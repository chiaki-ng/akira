#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace akira::log {

class LogExcerpt {
public:
    enum class Level { Info, Warning, Error, Debug, Verbose };

    struct Block {
        Level level = Level::Info;
        std::string text;

        friend bool operator==(const Block&, const Block&) = default;
    };

    static constexpr std::size_t maxBytes = 256 * 1024;
    static constexpr std::size_t maxLines = 2000;

    static auto read(const std::filesystem::path& path) -> std::optional<LogExcerpt>;
    static auto of(std::string raw, std::uintmax_t totalBytes, bool startedMidway) -> LogExcerpt;
    static auto sanitize(std::string_view input) -> std::string;
    static auto levelOf(std::string_view line) -> std::optional<Level>;
    static auto blocksOf(std::string_view text) -> std::vector<Block>;

    auto blocks() const -> const std::vector<Block>& { return shown; }
    auto totalBytes() const -> std::uintmax_t { return total; }
    auto lines() const -> std::size_t { return lineCount; }
    auto truncated() const -> bool { return cut; }
    auto empty() const -> bool { return shown.empty(); }

    friend bool operator==(const LogExcerpt&, const LogExcerpt&) = default;

private:
    std::vector<Block> shown;
    std::uintmax_t total = 0;
    std::size_t lineCount = 0;
    bool cut = false;
};

}
