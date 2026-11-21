#include "test_util.hpp"

#include <arisa/provider/standard/directory_assets.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/app_settings.hpp"
#include "app/arisa/string_catalog.hpp"
#include "app/arisa/theme_packs.hpp"
#include "screen_shots.hpp"
#include "settings/settings_store.hpp"

namespace {

using akira::app::AppSettings;
using akira::app::arisa::StringCatalog;
using akira::app::arisa::ThemePacks;
using Scheme = arisa::appearance::ColorSchemePreference;

class SavedConfig {
public:
    explicit SavedConfig(const std::string& contents)
        : root(std::filesystem::temp_directory_path() / ("akira-startup-" + std::to_string(counter++)))
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        if (!contents.empty())
            std::ofstream(root / "akira.toml") << contents;
    }

    ~SavedConfig()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    SavedConfig(const SavedConfig&) = delete;
    SavedConfig& operator=(const SavedConfig&) = delete;

    std::filesystem::path root;

private:
    static inline std::atomic<int> counter{ 0 };
};

}

TEST(arisa_language_follows_the_setting_then_the_console)
{
    const std::vector<std::string> french{ "fr" };
    const std::vector<std::string> chinese{ "fr", "zh-CN" };
    const std::vector<std::string> british{ "en-GB" };
    const std::vector<std::string> traditional{ "zh-TW" };
    CHECK(StringCatalog::resolve("en-US", chinese) == "en-US");
    CHECK(StringCatalog::resolve("zh-Hans", french) == "zh-Hans");
    CHECK(StringCatalog::resolve("", chinese) == "zh-Hans");
    CHECK(StringCatalog::resolve("", british) == "en-GB");
    CHECK(StringCatalog::resolve("", french) == "en");
    CHECK(StringCatalog::resolve("", traditional) == "en");
    CHECK(StringCatalog::resolve("", {}) == "en");
}

TEST(arisa_language_choice_reaches_the_string_catalog)
{
    const arisa::provider::standard::DirectoryAssets assets(AKIRA_RESOURCES_DIR);
    StringCatalog strings(assets, StringCatalog::fallbackLocale);
    CHECK(strings.text("akira/common/cancel") == "Cancel");
    strings.choose("zh-Hans");
    CHECK(strings.language() == "zh-Hans");
    CHECK(strings.text("akira/common/cancel") == "取消");
    strings.choose("en-US");
    CHECK(strings.text("akira/common/cancel") == "Cancel");
    strings.choose("");
    CHECK(strings.language() == "en");
}

TEST(arisa_borealis_themes_map_to_packs)
{
    CHECK(ThemePacks::borealisChoice("playstation") == std::optional(ThemePacks::Choice{ "akira-cobalt", Scheme::dark }));
    CHECK(ThemePacks::borealisChoice("ps30") == std::optional(ThemePacks::Choice{ "akira-anniversary", Scheme::dark }));
    CHECK(ThemePacks::borealisChoice("ps30-light") == std::optional(ThemePacks::Choice{ "akira-anniversary", Scheme::light }));
    CHECK(!ThemePacks::borealisChoice("unknown").has_value());
}

TEST(arisa_borealis_theme_migrates_once)
{
    SavedConfig config("version = 11\n\n[ui]\ntheme = 'ps30-light'\n");
    akira::settings::SettingsStore store(config.root);
    AppSettings settings(store);
    tests::ScreenShots shots;
    REQUIRE(shots.start(shots.shell(shots.parts())));
    CHECK(ThemePacks::migrate(shots.appearance(), settings));
    CHECK(settings.themePackMigrated());
    CHECK(shots.appearance().overrides().theme == std::optional<std::string>("akira-anniversary"));
    CHECK(shots.appearance().overrides().color_scheme == std::optional(Scheme::light));
    REQUIRE(shots.settle());
    CHECK(shots.write("theme_migrated_anniversary"));
    shots.appearance().update([](arisa::appearance::AppearanceOverrides& overrides) { overrides.theme = "akira-glass"; });
    CHECK(!ThemePacks::migrate(shots.appearance(), settings));
    CHECK(shots.appearance().overrides().theme == std::optional<std::string>("akira-glass"));
    store.save();
    akira::settings::SettingsStore reopened(config.root);
    AppSettings again(reopened);
    CHECK(again.themePackMigrated());
}

TEST(arisa_fresh_install_keeps_the_default_pack)
{
    SavedConfig config("");
    akira::settings::SettingsStore store(config.root);
    AppSettings settings(store);
    tests::ScreenShots shots;
    REQUIRE(shots.start(shots.shell(shots.parts())));
    CHECK(ThemePacks::migrate(shots.appearance(), settings));
    CHECK(!shots.appearance().overrides().theme.has_value());
    CHECK(settings.themePackMigrated());
}

TEST(arisa_theme_chosen_in_arisa_is_not_overwritten)
{
    SavedConfig config("version = 11\n\n[ui]\ntheme = 'ps30'\n");
    akira::settings::SettingsStore store(config.root);
    AppSettings settings(store);
    tests::ScreenShots shots;
    REQUIRE(shots.start(shots.shell(shots.parts())));
    shots.appearance().update([](arisa::appearance::AppearanceOverrides& overrides) { overrides.theme = "akira-glass"; });
    CHECK(ThemePacks::migrate(shots.appearance(), settings));
    CHECK(shots.appearance().overrides().theme == std::optional<std::string>("akira-glass"));
}
