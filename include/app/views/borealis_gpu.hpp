#pragma once

#include "app/views/borealis_deko3d_host.hpp"
#include "app/views/borealis_vulkan_host.hpp"
#include "platform/video/gpu_access.hpp"

namespace akira::app::views {

class BorealisGpu : public platform::video::GpuAccess {
public:
    void prefer(play::RendererKind kind) override;
    auto deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>> override;
    auto vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>> override;

private:
#ifdef BOREALIS_USE_DEKO3D
    BorealisDeko3dHost deko3dHost;
#endif
#ifdef BOREALIS_USE_VULKAN
    BorealisVulkanHost vulkanHost;
#endif
};

}
