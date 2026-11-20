#include "app/arisa/gpu.hpp"

#include "log/log.hpp"

namespace akira::app::arisa {

Gpu::Gpu(Stock& gpu, const ::arisa::provider::SurfaceProvider& surface)
    : host(gpu, surface)
{
}

void Gpu::prefer(play::RendererKind kind)
{
    if (kind != Kind)
        log::warning("[ARISA] akira-arisa picks its GPU API at build time, the stream keeps this build's renderer");
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
