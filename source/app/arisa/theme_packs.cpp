#include "app/arisa/theme_packs.hpp"

#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/appearance/appearance_settings.hpp>

#include <string>
#include <utility>
#include <vector>

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

}
