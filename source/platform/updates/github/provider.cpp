#include "platform/updates/github/provider.hpp"

#include <array>

#include <borealis.hpp>

#include "platform/updates/github/settings_updates_view.hpp"
#include "platform/updates/github/update_flow.hpp"
#include "platform/updates/github/update_manager.hpp"

using namespace brls::literals;

namespace akira::platform::github
{

GithubUpdateProvider::GithubUpdateProvider(HttpPool& http, app::AppSettings& app,
    settings::DeveloperSettings& developer, settings::SettingsStore& store)
    : m_manager(app)
    , m_services{ .manager = m_manager, .http = http, .app = app, .developer = developer, .store = store }
    , m_flow(m_services)
{
}

void GithubUpdateProvider::prepare(int argc, char* argv[])
{
    m_manager.setSelfPath(argc > 0 && argv[0] ? argv[0] : "");
}

void GithubUpdateProvider::launched()
{
    m_flow.checkOnLaunch();
}

std::span<const char* const> GithubUpdateProvider::helpFiles() const
{
    static constexpr auto files = std::to_array<const char*>({"updates"});
    return files;
}

std::vector<SettingsMenu> GithubUpdateProvider::settingsMenus() const
{
    return {{"akira/settings/menu_updates"_i18n, [this]() -> brls::Box* { return new SettingsUpdatesView(m_flow, m_services); }}};
}

void GithubUpdateProvider::initSimulateRow(brls::DetailCell& cell) const
{
    cell.setText("akira/settings/simulate_update"_i18n);
    cell.registerClickAction([this](brls::View*) {
        m_flow.simulate();
        return true;
    });
}

}
