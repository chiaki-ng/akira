#pragma once

#include <utility>

#include <vulkan/vulkan.h>

namespace akira::platform::vulkan {

template <typename Handle, auto Destroy>
class DeviceHandle {
public:
    DeviceHandle() = default;
    DeviceHandle(VkDevice device, Handle handle)
        : device(device)
        , handle(handle)
    {
    }
    ~DeviceHandle() { reset(); }
    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;
    DeviceHandle(DeviceHandle&& other) noexcept
        : device(other.device)
        , handle(std::exchange(other.handle, VK_NULL_HANDLE))
    {
    }
    DeviceHandle& operator=(DeviceHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            device = other.device;
            handle = std::exchange(other.handle, VK_NULL_HANDLE);
        }
        return *this;
    }

    Handle get() const { return handle; }
    explicit operator bool() const { return handle != VK_NULL_HANDLE; }

    void reset()
    {
        if (handle != VK_NULL_HANDLE)
            Destroy(device, std::exchange(handle, VK_NULL_HANDLE), nullptr);
    }

private:
    VkDevice device = VK_NULL_HANDLE;
    Handle handle = VK_NULL_HANDLE;
};

}
