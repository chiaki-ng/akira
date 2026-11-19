#pragma once

#include <arisa/provider/horizon/console.hpp>
#include <arisa/provider/surface/provider.hpp>

#if AKIRA_VULKAN
#include "app/arisa/vulkan_host.hpp"
#else
#include "app/arisa/deko3d_host.hpp"
#endif
#include "platform/video/gpu_access.hpp"

namespace akira::app::arisa {

class Gpu final : public platform::video::GpuAccess {
public:
#if AKIRA_VULKAN
    using Host = VulkanHost;
    static constexpr play::RendererKind Kind = play::RendererKind::Vulkan;
#else
    using Host = Deko3dHost;
    static constexpr play::RendererKind Kind = play::RendererKind::Drako3d;
#endif

    Gpu(::arisa::provider::horizon::Console::Gpu& gpu, const ::arisa::provider::SurfaceProvider& surface);

    void prefer(play::RendererKind kind) override;
    auto deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>> override;
    auto vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>> override;

private:
    Host host;
};

}
