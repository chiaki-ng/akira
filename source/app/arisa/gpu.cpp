#include "app/arisa/gpu.hpp"

#include "log/log.hpp"

namespace akira::app::arisa {

Gpu::Gpu(::arisa::provider::horizon::Console::Gpu& gpu, const ::arisa::provider::SurfaceProvider& surface)
    : host(gpu, surface)
{
}

void Gpu::prefer(play::RendererKind kind)
{
    if (kind == play::RendererKind::Drako3d)
        log::warning("[ARISA] drako3d is chosen at build time in arisa, the stream uses this build's GPU");
}

#if defined(__SWITCH2__)
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
