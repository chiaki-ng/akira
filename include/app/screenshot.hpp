#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace akira::app {

class Screenshot {
public:
    static constexpr std::size_t channels = 4;

    static auto create(std::uint32_t width, std::uint32_t height, std::vector<std::byte> rgba) -> std::optional<Screenshot>;
    static auto fileName(const std::tm& local) -> std::string;

    auto width() const -> std::uint32_t { return wide; }
    auto height() const -> std::uint32_t { return tall; }
    [[nodiscard]] auto png() const -> std::optional<std::vector<std::byte>>;
    [[nodiscard]] auto save(const std::filesystem::path& folder, const std::string& name) const
        -> std::expected<std::filesystem::path, std::error_code>;

private:
    Screenshot(std::uint32_t width, std::uint32_t height, std::vector<std::byte> rgba);

    static void append(void* context, void* data, int size);

    std::uint32_t wide;
    std::uint32_t tall;
    std::vector<std::byte> pixels;
};

}
