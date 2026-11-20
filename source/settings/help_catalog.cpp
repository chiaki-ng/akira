#include "settings/help_catalog.hpp"

#include <memory>

#include <json-c/json.h>

namespace akira::settings {

HelpCatalog::HelpCatalog(const std::filesystem::path& directory, std::string_view locale)
{
    for (const std::string_view name : files)
    {
        const std::string file = std::string(name) + ".json";
        merge(directory / file);
        if (!locale.empty())
            merge(directory / std::string(locale) / file);
    }
}

void HelpCatalog::merge(const std::filesystem::path& file)
{
    const std::unique_ptr<json_object, decltype(&json_object_put)> root(
        json_object_from_file(file.string().c_str()), &json_object_put);
    if (!root || !json_object_is_type(root.get(), json_type_object))
        return;
    json_object_object_foreach(root.get(), key, value)
    {
        if (!json_object_is_type(value, json_type_object))
            continue;
        json_object* title = nullptr;
        json_object* body = nullptr;
        Entry entry;
        if (json_object_object_get_ex(value, "title", &title))
            entry.title = json_object_get_string(title);
        if (json_object_object_get_ex(value, "body", &body))
            entry.body = json_object_get_string(body);
        entries.insert_or_assign(key, std::move(entry));
    }
}

std::optional<HelpCatalog::Entry> HelpCatalog::find(std::string_view id) const
{
    const auto found = entries.find(id);
    if (found == entries.end())
        return std::nullopt;
    return found->second;
}

std::string HelpCatalog::title(std::string_view id) const
{
    const auto found = entries.find(id);
    return found == entries.end() ? std::string() : found->second.title;
}

std::string HelpCatalog::body(std::string_view id) const
{
    const auto found = entries.find(id);
    return found == entries.end() ? std::string() : found->second.body;
}

}
