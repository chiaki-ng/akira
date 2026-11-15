#include "platform/updates/github/provider.hpp"

#include <array>

#include <borealis.hpp>

#include "platform/updates/github/settings_updates_view.hpp"
#include "platform/updates/github/update_flow.hpp"
#include "platform/updates/github/update_manager.hpp"

using namespace brls::literals;

namespace akira::platform::github
{

void GithubUpdateProvider::prepare(int argc, char* argv[])
{
    UpdateManager::setSelfPath(argc > 0 && argv[0] ? argv[0] : "");
}

void GithubUpdateProvider::launched()
{
    UpdateFlow::checkOnLaunch();
}

std::span<const char* const> GithubUpdateProvider::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"updates"});
    return files;
}

std::vector<SettingsMenu> GithubUpdateProvider::settingsMenus() const
{
    return {{"akira/settings/menu_updates"_i18n, []() -> brls::Box* { return new SettingsUpdatesView(); }}};
}

void GithubUpdateProvider::initSimulateRow(brls::DetailCell& cell) const
{
    cell.setText("akira/settings/simulate_update"_i18n);
    cell.registerClickAction([](brls::View*) {
        UpdateFlow::simulate();
        return true;
    });
}

}
