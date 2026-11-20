#pragma once

#include <arisa/provider/horizon/gpu_options.hpp>
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
    using Stock = ::arisa::provider::vulkan::VulkanGpu;
    static constexpr ::arisa::provider::horizon::GpuApi Api = ::arisa::provider::horizon::GpuApi::vulkan;
    static constexpr play::RendererKind Kind = play::RendererKind::Vulkan;
#else
    using Host = Deko3dHost;
    using Stock = ::arisa::provider::horizon::Deko3dGpu;
    static constexpr ::arisa::provider::horizon::GpuApi Api
        = AKIRA_DRAKO3D ? ::arisa::provider::horizon::GpuApi::drako3d : ::arisa::provider::horizon::GpuApi::deko3d;
    static constexpr play::RendererKind Kind = play::RendererKind::Drako3d;
#endif

    Gpu(Stock& gpu, const ::arisa::provider::SurfaceProvider& surface);

    void prefer(play::RendererKind kind) override;
    auto deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>> override;
    auto vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>> override;

private:
    Host host;
};

}
