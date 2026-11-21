#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::app {

class AppSettings final : public settings::Section {
public:
    explicit AppSettings(settings::SettingsStore& store);
    ~AppSettings() override;

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    void write(toml::table& config) const override;

    const std::string& uiTheme() const { return theme; }
    void setUiTheme(const std::string& id) { theme = id; }
    auto savedUiTheme() const -> std::optional<std::string> { return themeSaved ? std::optional(theme) : std::nullopt; }
    bool themePackMigrated() const { return packMigrated; }
    void markThemePackMigrated() { packMigrated = true; }
    bool hideAccountName() const { return hideName; }
    void setHideAccountName(bool enabled) { hideName = enabled; }
    std::string maskAccountName(const std::string& name) const;
    bool connectionShowStages() const { return showStages; }
    void setConnectionShowStages(bool show) { showStages = show; }
    bool screenshotChord() const { return screenshots; }
    void setScreenshotChord(bool enabled) { screenshots = enabled; }
    bool sleepOnExit() const { return sleepWhenExiting; }
    void setSleepOnExit(bool enabled) { sleepWhenExiting = enabled; }
    const std::string& debugLocale() const { return locale; }
    void setDebugLocale(const std::string& value) { locale = value; }

    const std::string& updateChannel() const { return channel; }
    void setUpdateChannel(const std::string& value) { channel = value; }
    bool autoCheckUpdates() const { return autoCheck; }
    void setAutoCheckUpdates(bool enabled) { autoCheck = enabled; }
    int64_t lastUpdateCheck() const { return lastCheck; }
    void setLastUpdateCheck(int64_t epochSeconds) { lastCheck = epochSeconds; }
    const std::string& updateInstallPath() const { return installPath; }
    void setUpdateInstallPath(const std::string& path) { installPath = path; }

private:
    void read(const toml::table& config);

    settings::SettingsStore& store;

    std::string theme = "playstation";
    bool themeSaved = false;
    bool packMigrated = false;
    bool hideName = false;
    bool showStages = true;
    bool screenshots = true;
    bool sleepWhenExiting = false;
    std::string locale;
    std::string channel = "stable";
    bool autoCheck = true;
    int64_t lastCheck = 0;
    std::string installPath;
};

}
