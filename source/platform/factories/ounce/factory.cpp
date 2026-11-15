#include "platform/factories/ounce/factory.hpp"

#include <array>

#include "platform/feedback/hid/provider.hpp"
#include "platform/haptics/hid/provider.hpp"
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
    return {"ounce", PlatformCapability::none};
}

std::span<const char* const> OuncePlatformFactory::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"ounce"});
    return files;
}

std::unique_ptr<VideoProvider> OuncePlatformFactory::createVideo()
{
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
