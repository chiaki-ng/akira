#pragma once

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
};

}
