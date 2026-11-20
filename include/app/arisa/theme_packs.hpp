#pragma once

#include <array>
#include <filesystem>
#include <string_view>

#include <arisa/provider/standard/directory_storage.hpp>
#include <arisa/runtime/application_options.hpp>

namespace akira::app::arisa {

class ThemePacks {
public:
    static constexpr std::string_view standard = "akira-default";
    static constexpr std::array<std::string_view, 4> names{ "akira-default", "akira-glass", "akira-cobalt",
        "akira-anniversary" };

    explicit ThemePacks(std::filesystem::path folder);

    ThemePacks(const ThemePacks&) = delete;
    ThemePacks& operator=(const ThemePacks&) = delete;

    auto options(std::string_view chosen = standard) -> ::arisa::runtime::AppearanceOptions;

private:
    ::arisa::provider::standard::DirectoryStorage folder;
};

}
