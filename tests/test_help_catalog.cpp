#include "test_util.hpp"

#include <array>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>

#include "settings/help_catalog.hpp"

using akira::settings::HelpCatalog;

TEST(help_catalog_merges_a_locale_over_the_base)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / std::format("akira-help-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(root / "zz");
    std::ofstream(root / "quality.json") << R"({ "settings/a": { "title": "A", "body": "base a" },
        "settings/b": { "title": "B", "body": "base b" } })";
    std::ofstream(root / "zz" / "quality.json") << R"({ "settings/b": { "title": "B2", "body": "local b" } })";
    const HelpCatalog help(root, "zz");
    CHECK(help.size() == 2);
    CHECK(help.body("settings/a") == "base a");
    CHECK(help.body("settings/b") == "local b");
    CHECK(help.body("settings/missing").empty());
    CHECK(!help.find("settings/missing").has_value());
    std::error_code error;
    std::filesystem::remove_all(root, error);
}

TEST(help_catalog_has_every_row_the_arisa_settings_show)
{
    const HelpCatalog help(std::filesystem::path(AKIRA_RESOURCES_DIR) / "help", "");
    constexpr std::array<std::string_view, 15> rows{
        "settings/streamProfile", "settings/localResolution", "settings/localFps", "settings/localBitrate",
        "settings/remoteResolution", "settings/remoteFps", "settings/remoteBitrate", "settings/upscaleTarget",
        "settings/rcasEnabled", "settings/rcasSharpness", "settings/touchpad", "settings/swipeGestures",
        "settings/rumbleSource", "settings/hapticIntensity", "settings/natType",
    };
    for (const std::string_view row : rows)
    {
        if (help.body(row).empty())
            std::printf("      missing help for %.*s\n", static_cast<int>(row.size()), row.data());
        CHECK(!help.body(row).empty());
    }
    CHECK(help.size() >= 65);
}
