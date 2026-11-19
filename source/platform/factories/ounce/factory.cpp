#include "platform/factories/ounce/factory.hpp"

#include <array>

#include <borealis.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>

#include "core/settings_manager.hpp"

#include "platform/feedback/hid/provider.hpp"
#include "platform/haptics/hid/provider.hpp"
#include "platform/video/drako3d/provider.hpp"
#include "platform/video/vulkan/provider.hpp"
#include "platform/pad_settings/hid/provider.hpp"
#include "platform/stream/single/provider.hpp"

namespace akira::platform
{

PlatformFactory& PlatformFactory::instance()
{
    static OuncePlatformFactory factory;
    return factory;
}

PlatformInfo OuncePlatformFactory::info() const noexcept
{
    const bool softwareHevc = SettingsManager::getInstance()->getSoftwareHevc();
    return {"ounce", softwareHevc ? PlatformCapability::fsr_4k | PlatformCapability::hevc : PlatformCapability::fsr_4k};
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
    const bool drako3d = SettingsManager::getInstance()->getVideoRenderer() == RendererKind::Drako3d
        && drako3d::Drako3dVideoProvider::shadersPresent();
    switchPlatform().setRenderer(drako3d ? brls::SwitchRenderer::Deko3d : brls::SwitchRenderer::Vulkan);
}

std::vector<RendererChoice> OuncePlatformFactory::rendererChoices() const
{
    std::vector<RendererChoice> choices{ { RendererKind::Vulkan, "Vulkan" } };
    if (drako3d::Drako3dVideoProvider::shadersPresent())
        choices.push_back({ RendererKind::Drako3d, "drako3d" });
    return choices;
}

std::unique_ptr<VideoProvider> OuncePlatformFactory::createVideo()
{
    if (switchPlatform().getRenderer() == brls::SwitchRenderer::Deko3d)
        return std::make_unique<drako3d::Drako3dVideoProvider>();
    return std::make_unique<vulkan::VulkanVideoProvider>();
}

std::unique_ptr<HapticsProvider> OuncePlatformFactory::createHaptics()
{
    return std::make_unique<hid::HidHapticsProvider>();
}

std::unique_ptr<FeedbackProvider> OuncePlatformFactory::createFeedback(HapticManager& haptics)
{
    return std::make_unique<hid::HidFeedbackProvider>(haptics);
}

std::unique_ptr<StreamControlsProvider> OuncePlatformFactory::createStreamControls(std::weak_ptr<StreamControlsHost> host)
{
    return std::make_unique<single::SingleStreamControlsProvider>(std::move(host));
}

std::unique_ptr<PadSettingsProvider> OuncePlatformFactory::createPadSettings(const input::PadDescription& pad)
{
    return std::make_unique<hid::HidPadSettingsProvider>(pad);
}

}
