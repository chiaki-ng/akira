#include "platform/factories/ounce/factory.hpp"

#include <array>

#include "play/stream_settings.hpp"

#include "platform/feedback/hid/provider.hpp"
#include "platform/haptics/hid/provider.hpp"
#if AKIRA_DRAKO3D
#include "platform/video/drako3d/provider.hpp"
#endif
#include "platform/video/gpu_access.hpp"
#if AKIRA_VULKAN
#include "platform/video/vulkan/provider.hpp"
#endif
#include "platform/pad_settings/hid/provider.hpp"

namespace akira::platform
{

std::unique_ptr<PlatformFactory> PlatformFactory::create(PlatformServices services)
{
    return std::make_unique<OuncePlatformFactory>(services);
}

PlatformCapability PlatformFactory::fixedCapabilities() noexcept
{
    return OuncePlatformFactory::Capabilities;
}

OuncePlatformFactory::OuncePlatformFactory(PlatformServices services)
    : m_services(services)
    , m_updates(services.app, "chiaki-ng/akira-ounce")
    , m_pads(services.controllers, services.rumble)
{
}

PlatformInfo OuncePlatformFactory::info() const noexcept
{
    const bool softwareHevc = m_services.stream.softwareHevc();
    return {"ounce", softwareHevc ? Capabilities | PlatformCapability::hevc : Capabilities};
}

std::span<const char* const> OuncePlatformFactory::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"ounce"});
    return files;
}

void OuncePlatformFactory::prepareVideo()
{
#if AKIRA_DRAKO3D && AKIRA_VULKAN
    const bool drako3d = m_services.stream.videoRenderer() == play::RendererKind::Drako3d
        && drako3d::Drako3dVideoProvider::shadersPresent();
    m_services.gpu.prefer(drako3d ? play::RendererKind::Drako3d : play::RendererKind::Vulkan);
#elif AKIRA_DRAKO3D
    m_services.gpu.prefer(play::RendererKind::Drako3d);
#else
    m_services.gpu.prefer(play::RendererKind::Vulkan);
#endif
}

std::vector<RendererChoice> OuncePlatformFactory::rendererChoices() const
{
    std::vector<RendererChoice> choices;
#if AKIRA_VULKAN
    choices.push_back({ play::RendererKind::Vulkan, "Vulkan" });
#endif
#if AKIRA_DRAKO3D
    if (drako3d::Drako3dVideoProvider::shadersPresent())
        choices.push_back({ play::RendererKind::Drako3d, "drako3d" });
#endif
    return choices;
}

std::unique_ptr<VideoProvider> OuncePlatformFactory::createVideo()
{
#if AKIRA_DRAKO3D && AKIRA_VULKAN
    if (const auto deko3d = m_services.gpu.deko3d())
        return drako3dVideo(deko3d->get());
    return vulkanVideo(m_services.gpu.vulkan().value().get());
#elif AKIRA_DRAKO3D
    return drako3dVideo(m_services.gpu.deko3d().value().get());
#else
    return vulkanVideo(m_services.gpu.vulkan().value().get());
#endif
}

#if AKIRA_DRAKO3D
std::unique_ptr<VideoProvider> OuncePlatformFactory::drako3dVideo(deko3d::GpuHost& gpu)
{
    return std::make_unique<drako3d::Drako3dVideoProvider>(gpu, m_services.stream, m_services.streamState,
        m_services.developer, m_services.wireGuard, m_services.overlayPalette);
}
#endif

#if AKIRA_VULKAN
std::unique_ptr<VideoProvider> OuncePlatformFactory::vulkanVideo(vulkan::GpuHost& gpu)
{
    return std::make_unique<vulkan::VulkanVideoProvider>(gpu, m_services.stream, m_services.streamState,
        m_services.developer, m_services.wireGuard, m_services.overlayPalette);
}
#endif

std::unique_ptr<HapticsProvider> OuncePlatformFactory::createHaptics()
{
    return std::make_unique<hid::HidHapticsProvider>();
}

std::unique_ptr<FeedbackProvider> OuncePlatformFactory::createFeedback(play::HapticManager& haptics)
{
    return std::make_unique<hid::HidFeedbackProvider>(haptics);
}

std::unique_ptr<PadSettingsProvider> OuncePlatformFactory::createPadSettings(const platform::PadDescription& pad)
{
    return std::make_unique<hid::HidPadSettingsProvider>(pad);
}

}
