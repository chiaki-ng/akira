#pragma once

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace brls
{
class Box;
class BooleanCell;
class DetailCell;
}

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

protected:
    Platform() = default;
};

}
