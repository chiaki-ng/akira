#pragma once

#include <cstdint>
#include <expected>
#include <memory>

#include "platform/video/viddec/output_memory.hpp"

namespace akira::platform::drako3d
{

class HostOutputMemory final : public viddec::OutputMemory
{
public:
    std::expected<std::unique_ptr<viddec::OutputBlock>, viddec::OutputFailure> allocate(std::uint64_t size,
        std::uint64_t alignment) override;

    static constexpr std::uint64_t PageSize = 0x1000;

private:
    class Block;

    std::uint64_t m_next_handle = 1;
};

}
