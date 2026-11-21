#pragma once

#include <arisa/provider/horizon/gpu_options.hpp>
#include <arisa/provider/surface/provider.hpp>

#include "app/arisa/overlay_handoff.hpp"
#include "app/arisa/stream_capture.hpp"
#if AKIRA_VULKAN
#include "app/arisa/vulkan_host.hpp"
#else
#include "app/arisa/deko3d_host.hpp"
#endif
#include "platform/video/gpu_access.hpp"

namespace akira::app::arisa {

class Gpu final : public platform::video::GpuAccess, public StreamCapture {
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

    static auto options() -> ::arisa::provider::horizon::GpuOptions;

    void prefer(play::RendererKind kind) override;
    auto deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>> override;
    auto vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>> override;
    [[nodiscard]] auto captureStream(Delivery& delivery) -> bool override;
    auto handoff() -> OverlayHandoff& { return overlays; }

private:
    OverlayHandoff overlays;
    Host host;
};

}
