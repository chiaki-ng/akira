#pragma once

#include <memory>
#include <span>
#include <vector>

#include "platform/capability.hpp"
#include "platform/display/provider.hpp"
#include "platform/feedback/provider.hpp"
#include "platform/haptics/provider.hpp"
#include "platform/pad_settings/provider.hpp"
#include "platform/pads/provider.hpp"
#include "platform/stats/provider.hpp"
#include "platform/stream/provider.hpp"
#include "platform/updates/provider.hpp"
#include "platform/video/provider.hpp"
#include "platform/video/renderer_choice.hpp"

class HapticManager;

namespace akira::platform
{

class PlatformFactory
{
public:
    static PlatformFactory& instance();

    virtual ~PlatformFactory() = default;

    PlatformFactory(const PlatformFactory&) = delete;
    PlatformFactory& operator=(const PlatformFactory&) = delete;

    virtual PlatformInfo info() const noexcept = 0;
    bool supplies(PlatformCapability capability) const noexcept
    {
        return (info().capabilities & capability) == capability;
    }

    virtual std::span<const char* const> helpFiles() const = 0;

    virtual DisplayProvider& display() = 0;
    virtual UpdateProvider& updates() = 0;
    virtual StatsProvider& stats() = 0;
    virtual PadProvider& pads() = 0;

    virtual void prepareVideo() = 0;
    virtual std::vector<RendererChoice> rendererChoices() const = 0;
    virtual std::unique_ptr<VideoProvider> createVideo() = 0;
    virtual std::unique_ptr<HapticsProvider> createHaptics() = 0;
    virtual std::unique_ptr<FeedbackProvider> createFeedback(HapticManager& haptics) = 0;
    virtual std::unique_ptr<StreamControlsProvider> createStreamControls(std::weak_ptr<StreamControlsHost> host) = 0;
    virtual std::unique_ptr<PadSettingsProvider> createPadSettings(const input::PadDescription& pad) = 0;

protected:
    PlatformFactory() = default;
};

}
