#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

#include "app/screenshot.hpp"

#include <format>
#include <fstream>
#include <utility>

namespace akira::app {

Screenshot::Screenshot(std::uint32_t width, std::uint32_t height, std::vector<std::byte> rgba)
    : wide(width)
    , tall(height)
    , pixels(std::move(rgba))
{
}

auto Screenshot::create(std::uint32_t width, std::uint32_t height, std::vector<std::byte> rgba) -> std::optional<Screenshot>
{
    if (width == 0 || height == 0 || rgba.size() != std::size_t{ width } * height * channels)
        return std::nullopt;
    return Screenshot(width, height, std::move(rgba));
}

auto Screenshot::fileName(const std::tm& local) -> std::string
{
    return std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}.png", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
        local.tm_hour, local.tm_min, local.tm_sec);
}

void Screenshot::append(void* context, void* data, int size)
{
    auto* encoded = static_cast<std::vector<std::byte>*>(context);
    const auto* bytes = static_cast<const std::byte*>(data);
    encoded->insert(encoded->end(), bytes, bytes + size);
}

auto Screenshot::png() const -> std::optional<std::vector<std::byte>>
{
    std::vector<std::byte> encoded;
    const int written = stbi_write_png_to_func(&Screenshot::append, &encoded, static_cast<int>(wide), static_cast<int>(tall),
        static_cast<int>(channels), pixels.data(), static_cast<int>(wide * channels));
    if (written == 0 || encoded.empty())
        return std::nullopt;
    return encoded;
}

auto Screenshot::save(const std::filesystem::path& folder, const std::string& name) const
    -> std::expected<std::filesystem::path, std::error_code>
{
    std::error_code failure;
    std::filesystem::create_directories(folder, failure);
    if (failure)
        return std::unexpected(failure);
    const std::optional<std::vector<std::byte>> encoded = png();
    if (!encoded)
        return std::unexpected(std::make_error_code(std::errc::not_enough_memory));
    const std::filesystem::path target = folder / name;
    std::ofstream file(target, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(encoded->data()), static_cast<std::streamsize>(encoded->size()));
    if (!file)
        return std::unexpected(std::make_error_code(std::errc::io_error));
    return target;
}

}
