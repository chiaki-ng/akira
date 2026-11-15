#pragma once

#include <memory>

#include "platform/nx/nx_pad_source.hpp"
#include "platform/platform.hpp"

class IpcStatsService;

namespace akira::platform
{

class NxPlatform final : public Platform
{
public:
    static NxPlatform& instance();

    NxPlatform();
    ~NxPlatform() override;

    void beforeInit(int argc, char* argv[]) override;
    void launched() override;

    std::span<const char* const> helpFiles() const override;
    std::vector<SettingsMenu> settingsMenus() const override;
    void initIpcStatsToggle(brls::BooleanCell& cell) const override;
    void initSimulateUpdate(brls::DetailCell& cell) const override;

    void streamStarted(Session& session) override;
    void streamStopped() override;

    NxPadSource& pads() override;
    std::unique_ptr<PadHaptics> createHaptics() override;
    std::unique_ptr<PadFeedback> createFeedback(HapticManager& haptics) override;

private:
    NxPadSource m_pads;
    std::unique_ptr<IpcStatsService> m_ipc_service;
};

}
