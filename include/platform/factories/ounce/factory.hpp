#pragma once

#include "platform/display/per_mode/provider.hpp"
#include "platform/factory.hpp"
#include "platform/pads/hid/provider.hpp"
#include "platform/stats/none/provider.hpp"
#include "platform/updates/github/provider.hpp"
#include "platform/video/gpu_access.hpp"

namespace akira::platform
{

class OuncePlatformFactory final : public PlatformFactory
{
public:
    static constexpr PlatformCapability Capabilities = PlatformCapability::fsr_4k | PlatformCapability::self_update;

    explicit OuncePlatformFactory(PlatformServices services);

    PlatformInfo info() const noexcept override;
    std::span<const char* const> helpFiles() const override;

    per_mode::PerModeDisplayProvider& display() override { return m_display; }
    github::GithubUpdateProvider& updates() override { return m_updates; }
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
#if AKIRA_DRAKO3D
    std::unique_ptr<VideoProvider> drako3dVideo(deko3d::GpuHost& gpu);
#endif
#if AKIRA_VULKAN
    std::unique_ptr<VideoProvider> vulkanVideo(vulkan::GpuHost& gpu);
#endif

    PlatformServices m_services;
    per_mode::PerModeDisplayProvider m_display;
    github::GithubUpdateProvider m_updates;
    none::NoStatsProvider m_stats;
    hid::HidPadProvider m_pads;
};

}
