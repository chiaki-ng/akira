#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "platform/pad_feedback.hpp"
#include "platform/pad_haptics.hpp"
#include "platform/pad_settings.hpp"
#include "platform/pad_source.hpp"
#include "platform/stream_controls.hpp"

namespace brls
{
class Box;
class BooleanCell;
class DetailCell;
}

class HapticManager;
class Session;

namespace akira::platform
{

class Platform
{
public:
    struct SettingsMenu
    {
        std::string title;
        std::function<brls::Box*()> create;
    };

    static Platform& instance();

    virtual ~Platform() = default;

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    virtual void beforeInit(int argc, char* argv[]) = 0;
    virtual void launched() = 0;

    virtual std::span<const char* const> helpFiles() const = 0;
    virtual std::vector<SettingsMenu> settingsMenus() const = 0;
    virtual void initIpcStatsToggle(brls::BooleanCell& cell) const = 0;
    virtual void initSimulateUpdate(brls::DetailCell& cell) const = 0;

    virtual void streamStarted(Session& session) = 0;
    virtual void streamStopped() = 0;

    virtual PadSource& pads() = 0;
    virtual std::unique_ptr<PadHaptics> createHaptics() = 0;
    virtual std::unique_ptr<PadFeedback> createFeedback(HapticManager& haptics) = 0;
    virtual std::unique_ptr<StreamControls> createStreamControls(std::weak_ptr<StreamControlsHost> host) = 0;
    virtual std::unique_ptr<PadSettings> createPadSettings(const input::PadDescription& pad) = 0;

protected:
    Platform() = default;
};

}
