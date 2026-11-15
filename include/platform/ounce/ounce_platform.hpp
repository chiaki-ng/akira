#pragma once

#include "platform/ounce/ounce_pad_source.hpp"
#include "platform/platform.hpp"

namespace akira::platform
{

class OuncePlatform final : public Platform
{
public:
    OuncePlatform() = default;

    void beforeInit(int argc, char* argv[]) override;
    void launched() override;

    std::span<const char* const> helpFiles() const override;
    std::vector<SettingsMenu> settingsMenus() const override;
    void initIpcStatsToggle(brls::BooleanCell& cell) const override;
    void initSimulateUpdate(brls::DetailCell& cell) const override;

    void streamStarted(Session& session) override;
    void streamStopped() override;

    OuncePadSource& pads() override;
    std::unique_ptr<PadHaptics> createHaptics() override;
    std::unique_ptr<PadFeedback> createFeedback(HapticManager& haptics) override;
    std::unique_ptr<StreamControls> createStreamControls(std::weak_ptr<StreamControlsHost> host) override;
    std::unique_ptr<PadSettings> createPadSettings(const input::PadDescription& pad) override;

private:
    OuncePadSource m_pads;
};

}
