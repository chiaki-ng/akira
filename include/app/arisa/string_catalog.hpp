#pragma once

#include <cstdint>
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

    StringCatalog(const ::arisa::provider::AssetProvider& assets, std::string_view locale);

    auto text(std::string_view key) const -> std::string override;
    auto loaded() const -> bool;

protected:
    auto pattern(std::string_view key, std::int64_t count) const -> std::string override;

private:
    static auto load(const ::arisa::provider::AssetProvider& assets) -> ::arisa::text::StringCatalog;
    static auto localeFor(std::string_view tag) -> ::arisa::text::Locale;

    ::arisa::text::StringCatalog catalog;
    ::arisa::text::Locale locale;
};

}
