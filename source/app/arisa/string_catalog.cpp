#include "app/arisa/string_catalog.hpp"

#include <arisa/text/localized_string_key.hpp>

#include <algorithm>

#include "log/log.hpp"

namespace akira::app::arisa {

StringCatalog::StringCatalog(const ::arisa::provider::AssetProvider& assets, std::string_view locale)
    : catalog(load(assets))
    , tag(locale.empty() ? fallbackLocale : locale)
    , locale(localeFor(tag))
{
}

auto StringCatalog::resolve(std::string_view chosen, std::span<const std::string> system) -> std::string
{
    if (!chosen.empty())
        return std::string(chosen);
    for (const std::string& language : system)
        if (std::string found = shipped(language); !found.empty())
            return found;
    return std::string(fallbackLocale);
}

auto StringCatalog::shipped(std::string_view system) -> std::string
{
    if (std::ranges::contains(simplifiedTags, system))
        return std::string(simplifiedChinese);
    if (system == fallbackLocale || system.starts_with("en-"))
        return std::string(system);
    return {};
}

void StringCatalog::choose(std::string_view chosen)
{
    tag = chosen.empty() ? std::string(fallbackLocale) : std::string(chosen);
    locale = localeFor(tag);
    log::info("[STRINGS] interface language {}", tag);
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
