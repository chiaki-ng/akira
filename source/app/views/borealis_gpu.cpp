#include "app/views/borealis_gpu.hpp"

#include <borealis.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>

namespace akira::app::views {

#ifdef BOREALIS_USE_VULKAN
void BorealisGpu::prefer(play::RendererKind kind)
{
    auto& platform = *static_cast<brls::SwitchPlatform*>(brls::Application::getPlatform());
    platform.setRenderer(kind == play::RendererKind::Drako3d ? brls::SwitchRenderer::Deko3d : brls::SwitchRenderer::Vulkan);
}

auto BorealisGpu::deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>>
{
    const auto& platform = *static_cast<brls::SwitchPlatform*>(brls::Application::getPlatform());
    if (platform.getRenderer() == brls::SwitchRenderer::Deko3d)
        return deko3dHost;
    return std::nullopt;
}

auto BorealisGpu::vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>>
{
    const auto& platform = *static_cast<brls::SwitchPlatform*>(brls::Application::getPlatform());
    if (platform.getRenderer() == brls::SwitchRenderer::Deko3d)
        return std::nullopt;
    return vulkanHost;
}
#else
void BorealisGpu::prefer(play::RendererKind)
{
}

auto BorealisGpu::deko3d() -> std::optional<std::reference_wrapper<platform::deko3d::GpuHost>>
{
    return deko3dHost;
}

auto BorealisGpu::vulkan() -> std::optional<std::reference_wrapper<platform::vulkan::GpuHost>>
{
    return std::nullopt;
}
#endif

}
