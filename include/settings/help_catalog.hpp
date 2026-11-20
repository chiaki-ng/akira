#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace akira::settings {

class HelpCatalog {
public:
    struct Entry {
        std::string title;
        std::string body;
    };

    HelpCatalog(const std::filesystem::path& directory, std::string_view locale);

    std::optional<Entry> find(std::string_view id) const;
    std::string title(std::string_view id) const;
    std::string body(std::string_view id) const;
    std::size_t size() const { return entries.size(); }

private:
    static constexpr std::array<std::string_view, 7> files{
        "general", "quality", "controls", "debug", "account", "poweruser", "developer",
    };

    void merge(const std::filesystem::path& file);

    std::map<std::string, Entry, std::less<>> entries;
};

}
