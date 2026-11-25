#include "app/arisa/theme_packs.hpp"

#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/appearance/appearance_settings.hpp>

#include <string>
#include <utility>
#include <vector>

#include "app/app_settings.hpp"
#include "log/log.hpp"

namespace akira::app::arisa {

namespace appearance = ::arisa::appearance;
namespace runtime = ::arisa::runtime;

ThemePacks::ThemePacks(std::filesystem::path folder)
    : folder(std::move(folder))
{
}

auto ThemePacks::options(std::string_view chosen) -> runtime::AppearanceOptions
{
    return runtime::AppearanceOptions{
        .policy = appearance::AppearancePolicy{
            .application = "akira",
            .defaults = appearance::AppearanceSettings{
                .theme = std::string(chosen),
                .color_scheme = appearance::ColorSchemePreference::dark,
            },
            .packs = std::vector<std::string>(names.begin(), names.end()),
        },
        .themes = folder,
    };
}

auto ThemePacks::borealisChoice(std::string_view theme) -> std::optional<Choice>
{
    using Scheme = appearance::ColorSchemePreference;
    if (theme == "playstation")
        return Choice{ .pack = names[2], .scheme = Scheme::dark };
    if (theme == "ps30")
        return Choice{ .pack = names[3], .scheme = Scheme::dark };
    if (theme == "ps30-light")
        return Choice{ .pack = names[3], .scheme = Scheme::light };
    return std::nullopt;
}

auto ThemePacks::migrate(const appearance::AppearanceModel& look, AppSettings& settings) -> bool
{
    if (settings.themePackMigrated())
        return false;
    settings.markThemePackMigrated();
    const std::optional<std::string> saved = settings.savedUiTheme();
    if (look.overrides().theme || !saved)
        return true;
    const std::optional<Choice> choice = borealisChoice(*saved);
    if (!choice)
    {
        log::info("[THEME] borealis theme {} has no pack, keeping {}", *saved, standard);
        return true;
    }
    look.update([&choice](appearance::AppearanceOverrides& overrides) {
        overrides.theme = std::string(choice->pack);
        overrides.color_scheme = choice->scheme;
    });
    log::info("[THEME] borealis theme {} became {}", *saved, choice->pack);
    return true;
}

}
