#include "platform/factories/ounce/factory.hpp"

#include <array>

#include <borealis.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>

#include "play/stream_settings.hpp"

#include "platform/feedback/hid/provider.hpp"
#include "platform/haptics/hid/provider.hpp"
#include "platform/video/drako3d/provider.hpp"
#include "platform/video/vulkan/provider.hpp"
#include "platform/pad_settings/hid/provider.hpp"
#include "platform/stream/single/provider.hpp"

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
    , m_pads(services.controllers)
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

brls::SwitchPlatform& OuncePlatformFactory::switchPlatform()
{
    return *static_cast<brls::SwitchPlatform*>(brls::Application::getPlatform());
}

void OuncePlatformFactory::prepareVideo()
{
    const bool drako3d = m_services.stream.videoRenderer() == play::RendererKind::Drako3d
        && drako3d::Drako3dVideoProvider::shadersPresent();
    switchPlatform().setRenderer(drako3d ? brls::SwitchRenderer::Deko3d : brls::SwitchRenderer::Vulkan);
}

std::vector<RendererChoice> OuncePlatformFactory::rendererChoices() const
{
    std::vector<RendererChoice> choices{ { play::RendererKind::Vulkan, "Vulkan" } };
    if (drako3d::Drako3dVideoProvider::shadersPresent())
        choices.push_back({ play::RendererKind::Drako3d, "drako3d" });
    return choices;
}

std::unique_ptr<VideoProvider> OuncePlatformFactory::createVideo()
{
    if (switchPlatform().getRenderer() == brls::SwitchRenderer::Deko3d)
        return std::make_unique<drako3d::Drako3dVideoProvider>(m_services.stream, m_services.streamState,
            m_services.store, m_services.developer, m_services.wireGuard);
    return std::make_unique<vulkan::VulkanVideoProvider>(m_services.stream, m_services.streamState, m_services.store,
        m_services.developer, m_services.wireGuard);
}

std::unique_ptr<HapticsProvider> OuncePlatformFactory::createHaptics()
{
    return std::make_unique<hid::HidHapticsProvider>();
}

std::unique_ptr<FeedbackProvider> OuncePlatformFactory::createFeedback(play::HapticManager& haptics)
{
    return std::make_unique<hid::HidFeedbackProvider>(haptics);
}

std::unique_ptr<StreamControlsProvider> OuncePlatformFactory::createStreamControls(std::weak_ptr<StreamControlsHost> host)
{
    return std::make_unique<single::SingleStreamControlsProvider>(std::move(host));
}

std::unique_ptr<PadSettingsProvider> OuncePlatformFactory::createPadSettings(const platform::PadDescription& pad)
{
    return std::make_unique<hid::HidPadSettingsProvider>(pad);
}

}
