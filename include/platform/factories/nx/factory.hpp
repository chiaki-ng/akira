#pragma once

#include "platform/display/fixed/provider.hpp"
#include "platform/factory.hpp"
#include "platform/pads/extended/provider.hpp"
#include "platform/stats/nxext/provider.hpp"
#include "platform/updates/github/provider.hpp"

namespace akira::platform
{

class NxPlatformFactory final : public PlatformFactory
{
public:
    static constexpr PlatformCapability Capabilities = PlatformCapability::couch_seats | PlatformCapability::self_update
        | PlatformCapability::stats_export | PlatformCapability::hevc;

    explicit NxPlatformFactory(PlatformServices services);

    PlatformInfo info() const noexcept override;
    std::span<const char* const> helpFiles() const override { return {}; }

    fixed::FixedDisplayProvider& display() override { return m_display; }
    github::GithubUpdateProvider& updates() override { return m_updates; }
    nxext::NxextStatsProvider& stats() override { return m_stats; }
    extended::ExtendedPadProvider& pads() override { return m_pads; }

    void prepareVideo() override;
    std::vector<RendererChoice> rendererChoices() const override;
    std::unique_ptr<VideoProvider> createVideo() override;
    std::unique_ptr<HapticsProvider> createHaptics() override;
    std::unique_ptr<FeedbackProvider> createFeedback(play::HapticManager& haptics) override;
    std::unique_ptr<StreamControlsProvider> createStreamControls(std::weak_ptr<StreamControlsHost> host) override;
    std::unique_ptr<PadSettingsProvider> createPadSettings(const platform::PadDescription& pad) override;

private:
    PlatformServices m_services;
    fixed::FixedDisplayProvider m_display;
    github::GithubUpdateProvider m_updates;
    nxext::NxextStatsProvider m_stats;
    extended::ExtendedPadProvider m_pads;
};

}
