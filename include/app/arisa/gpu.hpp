#pragma once

#include <arisa/provider/horizon/console.hpp>
#include <arisa/provider/surface/provider.hpp>

#if defined(__SWITCH2__)
#include "app/arisa/vulkan_host.hpp"
#else
#include "app/arisa/deko3d_host.hpp"
#endif
#include "platform/video/gpu_access.hpp"

namespace akira::app::arisa {

class Gpu final : public platform::video::GpuAccess {
public:
#if defined(__SWITCH2__)
    using Host = VulkanHost;
#else
    using Host = Deko3dHost;
#endif

    Gpu(::arisa::provider::horizon::Console::Gpu& gpu, const ::arisa::provider::SurfaceProvider& surface);

    void prefer(play::RendererKind kind) override;
    auto deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>> override;
    auto vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>> override;

private:
    Host host;
};

}
