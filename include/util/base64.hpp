#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <simdutf.h>

namespace akira::base64 {

inline auto encode(std::span<const std::byte> bytes) -> std::string
{
    std::string text(simdutf::base64_length_from_binary(bytes.size()), '\0');
    const std::size_t written = simdutf::binary_to_base64(
        reinterpret_cast<const char*>(bytes.data()), bytes.size(), text.data());
    text.resize(written);
    return text;
}

[[nodiscard]] inline auto decodeExactly(std::string_view text, std::span<std::byte> out) noexcept -> bool
{
    std::size_t written = out.size();
    const simdutf::result decoded = simdutf::base64_to_binary_safe(
        text.data(), text.size(), reinterpret_cast<char*>(out.data()), written);
    return decoded.error == simdutf::error_code::SUCCESS && written == out.size();
}

}
