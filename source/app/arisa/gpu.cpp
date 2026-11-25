#include "app/arisa/gpu.hpp"

#include "log/log.hpp"

namespace akira::app::arisa {

namespace horizon = ::arisa::provider::horizon;

Gpu::Gpu(Stock& gpu, const ::arisa::provider::SurfaceProvider& surface)
    : host(gpu, surface, overlays)
{
}

auto Gpu::options() -> horizon::GpuOptions
{
    horizon::GpuOptions made{ .api = Api };
#if AKIRA_VULKAN
    made.vulkan = horizon::VulkanDeviceOptions{
        .extensions = {},
        .optional_extensions = { "VK_EXT_external_memory_host", "VK_EXT_image_drm_format_modifier" },
        .features = { horizon::VulkanFeature::dynamic_rendering, horizon::VulkanFeature::synchronization2 },
    };
#endif
    return made;
}

void Gpu::prefer(play::RendererKind kind)
{
    if (kind != Kind)
        log::warning("[ARISA] akira-arisa picks its GPU API at build time, the stream keeps this build's renderer");
}

auto Gpu::captureStream(Delivery& delivery) -> bool
{
    return host.captureStream(delivery);
}

#if AKIRA_VULKAN
auto Gpu::deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>>
{
    return std::nullopt;
}

auto Gpu::vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>>
{
    return host;
}
#else
auto Gpu::deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>>
{
    return host;
}

auto Gpu::vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>>
{
    return std::nullopt;
}
#endif

}
