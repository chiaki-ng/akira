#pragma once

#include "platform/updates/provider.hpp"

namespace akira::platform::none
{

class NoUpdateProvider final : public UpdateProvider
{
public:
    void prepare(int, char*[]) override {}
    void launched() override {}
    std::span<const char* const> helpFiles() const override { return {}; }
    std::vector<SettingsMenu> settingsMenus() const override { return {}; }
    void initSimulateRow(brls::DetailCell&) const override {}
};

}
