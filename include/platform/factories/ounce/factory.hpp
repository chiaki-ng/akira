#pragma once

#include "platform/display/per_mode/provider.hpp"
#include "platform/factory.hpp"
#include "platform/pads/hid/provider.hpp"
#include "platform/stats/none/provider.hpp"
#include "platform/updates/none/provider.hpp"

namespace akira::platform
{

class OuncePlatformFactory final : public PlatformFactory
{
public:
    OuncePlatformFactory() = default;

    PlatformInfo info() const noexcept override;
    std::span<const char* const> helpFiles() const override;

    per_mode::PerModeDisplayProvider& display() override { return m_display; }
    none::NoUpdateProvider& updates() override { return m_updates; }
    none::NoStatsProvider& stats() override { return m_stats; }
    hid::HidPadProvider& pads() override { return m_pads; }

    std::unique_ptr<VideoProvider> createVideo() override;
    std::unique_ptr<HapticsProvider> createHaptics() override;
    std::unique_ptr<FeedbackProvider> createFeedback(HapticManager& haptics) override;
    std::unique_ptr<StreamControlsProvider> createStreamControls(std::weak_ptr<StreamControlsHost> host) override;
    std::unique_ptr<PadSettingsProvider> createPadSettings(const input::PadDescription& pad) override;

private:
    per_mode::PerModeDisplayProvider m_display;
    none::NoUpdateProvider m_updates;
    none::NoStatsProvider m_stats;
    hid::HidPadProvider m_pads;
};

}
