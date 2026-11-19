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
    static constexpr PlatformCapability Capabilities = PlatformCapability::fsr_4k;

    explicit OuncePlatformFactory(PlatformServices services);

    PlatformInfo info() const noexcept override;
    std::span<const char* const> helpFiles() const override;

    per_mode::PerModeDisplayProvider& display() override { return m_display; }
    none::NoUpdateProvider& updates() override { return m_updates; }
    none::NoStatsProvider& stats() override { return m_stats; }
    hid::HidPadProvider& pads() override { return m_pads; }
    play::CouchSeats* couchSeats() override { return nullptr; }

    void prepareVideo() override;
    std::vector<RendererChoice> rendererChoices() const override;
    std::unique_ptr<VideoProvider> createVideo() override;
    std::unique_ptr<HapticsProvider> createHaptics() override;
    std::unique_ptr<FeedbackProvider> createFeedback(play::HapticManager& haptics) override;
    std::unique_ptr<PadSettingsProvider> createPadSettings(const platform::PadDescription& pad) override;

private:
    PlatformServices m_services;
    per_mode::PerModeDisplayProvider m_display;
    none::NoUpdateProvider m_updates;
    none::NoStatsProvider m_stats;
    hid::HidPadProvider m_pads;
};

}
