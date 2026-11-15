#include "platform/factories/nx/factory.hpp"

#include "platform/feedback/extended/provider.hpp"
#include "platform/haptics/extended/provider.hpp"
#include "platform/pad_settings/extended/provider.hpp"
#include "platform/stream/couch/provider.hpp"

namespace akira::platform
{

PlatformFactory& PlatformFactory::instance()
{
    static NxPlatformFactory factory;
    return factory;
}

PlatformInfo NxPlatformFactory::info() const noexcept
{
    return {"nx", PlatformCapability::couch_seats | PlatformCapability::self_update | PlatformCapability::stats_export};
}

std::unique_ptr<HapticsProvider> NxPlatformFactory::createHaptics()
{
    return std::make_unique<extended::ExtendedHapticsProvider>(m_pads);
}

std::unique_ptr<FeedbackProvider> NxPlatformFactory::createFeedback(HapticManager& haptics)
{
    return std::make_unique<extended::ExtendedFeedbackProvider>(m_pads, haptics);
}

std::unique_ptr<StreamControlsProvider> NxPlatformFactory::createStreamControls(std::weak_ptr<StreamControlsHost> host)
{
    return std::make_unique<couch::CouchStreamControlsProvider>(m_pads, std::move(host));
}

std::unique_ptr<PadSettingsProvider> NxPlatformFactory::createPadSettings(const input::PadDescription& pad)
{
    return std::make_unique<extended::ExtendedPadSettingsProvider>(pad);
}

}
