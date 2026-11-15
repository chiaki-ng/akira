#pragma once

#include "platform/updates/provider.hpp"

namespace akira::platform::github
{

class GithubUpdateProvider final : public UpdateProvider
{
public:
    void prepare(int argc, char* argv[]) override;
    void launched() override;
    std::span<const char* const> helpFiles() const override;
    std::vector<SettingsMenu> settingsMenus() const override;
    void initSimulateRow(brls::DetailCell& cell) const override;
};

}
