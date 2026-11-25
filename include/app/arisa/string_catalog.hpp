#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <arisa/provider/assets/provider.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/string_catalog.hpp>

#include "app/strings.hpp"

namespace akira::app::arisa {

class StringCatalog final : public app::Strings {
public:
    static constexpr std::string_view directory = "strings";
    static constexpr std::string_view fallbackLocale = "en";
    static constexpr std::string_view simplifiedChinese = "zh-Hans";

    StringCatalog(const ::arisa::provider::AssetProvider& assets, std::string_view locale);

    static auto resolve(std::string_view chosen, std::span<const std::string> system) -> std::string;

    void choose(std::string_view tag);
    auto text(std::string_view key) const -> std::string override;
    auto loaded() const -> bool;
    auto language() const -> const std::string& { return tag; }

protected:
    auto pattern(std::string_view key, std::int64_t count) const -> std::string override;

private:
    static auto load(const ::arisa::provider::AssetProvider& assets) -> ::arisa::text::StringCatalog;
    static auto localeFor(std::string_view tag) -> ::arisa::text::Locale;
    static auto shipped(std::string_view system) -> std::string;

    static constexpr std::array<std::string_view, 4> simplifiedTags{ "zh-Hans", "zh-CN", "zh-SG", "zh" };

    ::arisa::text::StringCatalog catalog;
    std::string tag;
    ::arisa::text::Locale locale;
};

}
