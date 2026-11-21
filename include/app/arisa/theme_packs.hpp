#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/provider/standard/directory_storage.hpp>
#include <arisa/runtime/application_options.hpp>

namespace akira::app {
class AppSettings;
}

namespace akira::app::arisa {

class ThemePacks {
public:
    static constexpr std::string_view standard = "akira-default";
    static constexpr std::array<std::string_view, 4> names{ "akira-default", "akira-glass", "akira-cobalt",
        "akira-anniversary" };

    struct Choice {
        std::string_view pack;
        ::arisa::appearance::ColorSchemePreference scheme;

        friend bool operator==(const Choice&, const Choice&) = default;
    };

    explicit ThemePacks(std::filesystem::path folder);

    static auto borealisChoice(std::string_view theme) -> std::optional<Choice>;
    [[nodiscard]] static auto migrate(const ::arisa::appearance::AppearanceModel& look, AppSettings& settings) -> bool;

    ThemePacks(const ThemePacks&) = delete;
    ThemePacks& operator=(const ThemePacks&) = delete;

    auto options(std::string_view chosen = standard) -> ::arisa::runtime::AppearanceOptions;

private:
    ::arisa::provider::standard::DirectoryStorage folder;
};

}
