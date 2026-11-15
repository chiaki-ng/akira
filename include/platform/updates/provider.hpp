#pragma once

#include <span>
#include <vector>

#include "platform/settings_menu.hpp"

namespace brls
{
class DetailCell;
}

namespace akira::platform
{

class UpdateProvider
{
public:
    virtual ~UpdateProvider() = default;

    virtual void prepare(int argc, char* argv[]) = 0;
    virtual void launched() = 0;
    virtual std::span<const char* const> helpFiles() const = 0;
    virtual std::vector<SettingsMenu> settingsMenus() const = 0;
    virtual void initSimulateRow(brls::DetailCell& cell) const = 0;
};

}
