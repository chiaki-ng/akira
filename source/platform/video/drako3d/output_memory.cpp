#include "platform/video/drako3d/output_memory.hpp"

#include <algorithm>
#include <cstdlib>
#include <span>

namespace akira::platform::drako3d
{

class HostOutputMemory::Block final : public viddec::OutputBlock
{
public:
    Block(void* storage, std::uint64_t size, viddec::OutputHandle handle)
        : m_storage(storage), m_size(size), m_handle(handle)
    {
    }

    ~Block() override { std::free(m_storage); }

    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;

    std::span<std::byte> bytes() const override { return {static_cast<std::byte*>(m_storage), m_size}; }
    viddec::OutputHandle handle() const override { return m_handle; }

private:
    void* m_storage;
    std::uint64_t m_size;
    viddec::OutputHandle m_handle;
};

std::expected<std::unique_ptr<viddec::OutputBlock>, viddec::OutputFailure> HostOutputMemory::allocate(std::uint64_t size,
    std::uint64_t alignment)
{
    const std::uint64_t align = std::max(alignment, PageSize);
    const std::uint64_t rounded = (size + align - 1) & ~(align - 1);
    void* storage = std::aligned_alloc(align, rounded);
    if (!storage)
        return std::unexpected(viddec::OutputFailure{"aligned_alloc", static_cast<std::int64_t>(rounded)});
    return std::make_unique<Block>(storage, rounded, viddec::OutputHandle{m_next_handle++});
}

}
