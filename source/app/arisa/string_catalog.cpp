#include "app/arisa/string_catalog.hpp"

#include <arisa/text/localized_string_key.hpp>

#include "log/log.hpp"

namespace akira::app::arisa {

StringCatalog::StringCatalog(const ::arisa::provider::AssetProvider& assets, std::string_view locale)
    : catalog(load(assets))
    , locale(localeFor(locale))
{
}

auto StringCatalog::load(const ::arisa::provider::AssetProvider& assets) -> ::arisa::text::StringCatalog
{
    auto loaded = ::arisa::text::StringCatalog::load(assets, directory);
    for (const auto& failure : loaded.failures)
        log::error("[STRINGS] {}:{} {}", failure.file, failure.line, failure.reason);
    return std::move(loaded.catalog);
}

auto StringCatalog::localeFor(std::string_view tag) -> ::arisa::text::Locale
{
    if (auto chosen = ::arisa::text::Locale::create(tag.empty() ? fallbackLocale : tag))
        return *chosen;
    return *::arisa::text::Locale::create(fallbackLocale);
}

auto StringCatalog::text(std::string_view key) const -> std::string
{
    return catalog.pattern(::arisa::text::LocalizedStringKey{std::string(key)}, locale);
}

auto StringCatalog::pattern(std::string_view key, std::int64_t count) const -> std::string
{
    return catalog.pattern(::arisa::text::LocalizedStringKey{ std::string(key) }, locale, count);
}

auto StringCatalog::loaded() const -> bool
{
    return !catalog.empty();
}

}
