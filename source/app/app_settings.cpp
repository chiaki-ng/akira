#include "app/app_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::app {

AppSettings::AppSettings(settings::SettingsStore& store)
    : store(store)
{
    read(store.loaded());
    store.attach(*this);
}

AppSettings::~AppSettings()
{
    store.detach(*this);
}

void AppSettings::read(const toml::table& config)
{
    if (auto val = config["ui"]["theme"].value<std::string>())
    {
        theme = *val;
        themeSaved = true;
    }
    if (auto val = config["ui"]["theme_pack_migrated"].value<bool>())
        packMigrated = *val;
    if (auto val = config["ui"]["hide_account_name"].value<bool>())
        hideName = *val;
    if (auto val = config["ui"]["connection_show_stages"].value<bool>())
        showStages = *val;
    if (auto val = config["ui"]["screenshot_chord"].value<bool>())
        screenshots = *val;
    if (auto val = config["ui"]["rail_edge"].value<std::string>())
        edge = edgeNamed(*val).value_or(RailEdge::Left);
    if (auto val = config["stream"]["sleep_on_exit"].value<bool>())
        sleepWhenExiting = *val;
    if (auto val = config["debug"]["locale"].value<std::string>())
        locale = *val;

    if (auto val = config["updates"]["channel"].value<std::string>())
        channel = *val;
    if (auto val = config["updates"]["auto_check"].value<bool>())
        autoCheck = *val;
    if (auto val = config["updates"]["last_check"].value<int64_t>())
        lastCheck = *val;
    if (auto val = config["updates"]["install_path"].value<std::string>())
        installPath = *val;
}

void AppSettings::write(toml::table& config) const
{
    toml::table ui;
    ui.insert("theme", theme);
    if (packMigrated) ui.insert("theme_pack_migrated", true);
    if (hideName) ui.insert("hide_account_name", true);
    ui.insert("connection_show_stages", showStages);
    if (!screenshots) ui.insert("screenshot_chord", false);
    if (edge != RailEdge::Left) ui.insert("rail_edge", std::string(edgeName(edge)));
    config.insert("ui", std::move(ui));

    if (sleepWhenExiting)
        settings::SettingsStore::table(config, "stream").insert("sleep_on_exit", true);
    if (!locale.empty())
        settings::SettingsStore::table(config, "debug").insert("locale", locale);

    toml::table updates;
    updates.insert("channel", channel);
    updates.insert("auto_check", autoCheck);
    if (lastCheck != 0) updates.insert("last_check", lastCheck);
    if (!installPath.empty()) updates.insert("install_path", installPath);
    config.insert("updates", std::move(updates));
}

auto AppSettings::edgeName(RailEdge value) -> std::string_view
{
    switch (value)
    {
        case RailEdge::Left: return "left";
        case RailEdge::Top: return "top";
        case RailEdge::Right: return "right";
        case RailEdge::Bottom: return "bottom";
    }
    return "left";
}

auto AppSettings::edgeNamed(std::string_view name) -> std::optional<RailEdge>
{
    for (const RailEdge each : railEdges)
        if (edgeName(each) == name)
            return each;
    return std::nullopt;
}

std::string AppSettings::maskAccountName(const std::string& name) const
{
    if (!hideName || name.size() <= 2)
        return name;
    return name.substr(0, 2) + "***";
}

}
