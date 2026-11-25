#include "app/build_info.hpp"

#include <fstream>
#include <iterator>
#include <ranges>

namespace akira::app {

auto BuildInfo::read(const std::filesystem::path& path) -> std::optional<BuildInfo>
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
        return std::nullopt;
    const std::string text{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    return parse(text);
}

auto BuildInfo::heading(std::string_view line) -> std::optional<std::string>
{
    if (line.find("===") == std::string_view::npos)
        return std::nullopt;
    const auto first = line.find_first_not_of("= \t");
    if (first == std::string_view::npos)
        return std::string();
    const auto last = line.find_last_not_of("= \t\r");
    return std::string(line.substr(first, last - first + 1));
}

auto BuildInfo::parse(std::string_view text) -> BuildInfo
{
    BuildInfo made;
    for (const auto piece : text | std::views::split('\n'))
    {
        std::string_view line(piece.begin(), piece.end());
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.find_first_not_of(" \t") == std::string_view::npos)
            continue;
        if (const std::optional<std::string> title = heading(line))
        {
            made.listed.push_back(Section{ .title = *title });
            continue;
        }
        if (made.listed.empty())
            made.listed.push_back(Section{});
        made.listed.back().lines.emplace_back(line);
    }
    return made;
}

}
