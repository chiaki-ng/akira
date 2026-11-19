#pragma once

#include "platform/updates/github/update_flow.hpp"
#include "platform/updates/github/update_manager.hpp"
#include "platform/updates/provider.hpp"

namespace akira::platform::github
{

class GithubUpdateProvider final : public UpdateProvider
{
public:
    GithubUpdateProvider(HttpPool& http, app::AppSettings& app, settings::DeveloperSettings& developer,
        settings::SettingsStore& store);

    void prepare(int argc, char* argv[]) override;
    void launched() override;
    std::span<const char* const> helpFiles() const override;
    std::vector<SettingsMenu> settingsMenus() const override;
    void initSimulateRow(brls::DetailCell& cell) const override;

private:
    UpdateManager m_manager;
    UpdateFlow::Services m_services;
    UpdateFlow m_flow;
};

}
