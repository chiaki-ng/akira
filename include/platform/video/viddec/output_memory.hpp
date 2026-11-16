#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace akira::platform::viddec
{

enum class OutputHandle : std::uint64_t
{
};

struct OutputFailure
{
    std::string_view step;
    std::int64_t code;
};

class OutputBlock
{
public:
    virtual ~OutputBlock() = default;

    virtual std::span<std::byte> bytes() const = 0;
    virtual OutputHandle handle() const = 0;
};

class OutputMemory
{
public:
    virtual ~OutputMemory() = default;

    virtual std::expected<std::unique_ptr<OutputBlock>, OutputFailure> allocate(std::uint64_t size,
        std::uint64_t alignment) = 0;
};

}
