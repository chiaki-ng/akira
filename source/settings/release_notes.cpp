#include "settings/release_notes.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace akira::settings {

ReleaseNotes::ReleaseNotes(std::string_view published)
{
    std::string body(published);
    std::erase(body, '\r');
    text = reshaped(withoutComments(withoutFooter(body)));
}

std::string ReleaseNotes::withoutFooter(std::string_view body)
{
    const std::size_t open = body.find(footerOpen);
    if (open == std::string_view::npos)
        return std::string(body);
    const std::size_t close = body.find(footerClose, open);
    std::string kept(body.substr(0, open));
    if (close != std::string_view::npos)
        kept.append(body.substr(close + footerClose.size()));
    return kept;
}

std::string ReleaseNotes::withoutComments(std::string_view body)
{
    std::string kept;
    while (!body.empty()) {
        const std::size_t open = body.find(commentOpen);
        kept.append(body.substr(0, open));
        if (open == std::string_view::npos)
            break;
        const std::size_t close = body.find(commentClose, open + commentOpen.size());
        if (close == std::string_view::npos)
            break;
        body.remove_prefix(close + commentClose.size());
    }
    return kept;
}

std::string ReleaseNotes::withoutImages(std::string_view line)
{
    std::string pictureless;
    for (std::size_t at = 0; at < line.size(); ++at) {
        if (line[at] == '!' && at + 1 < line.size() && line[at + 1] == '[') {
            const std::size_t label = line.find("](", at);
            const std::size_t target = label == std::string_view::npos ? label : line.find(')', label);
            if (target != std::string_view::npos) {
                at = target;
                continue;
            }
        }
        pictureless.push_back(line[at]);
    }
    std::string kept;
    std::string_view rest = pictureless;
    while (!rest.empty()) {
        const std::size_t empty = rest.find("[](");
        kept.append(rest.substr(0, empty));
        if (empty == std::string_view::npos)
            break;
        const std::size_t target = rest.find(')', empty);
        if (target == std::string_view::npos) {
            kept.append(rest.substr(empty));
            break;
        }
        rest.remove_prefix(target + 1);
    }
    return kept;
}

std::string ReleaseNotes::linked(std::string_view line)
{
    std::string kept;
    std::size_t at = 0;
    while (at < line.size()) {
        const std::size_t found = std::min(line.find("https://", at), line.find("http://", at));
        if (found == std::string_view::npos) {
            kept.append(line.substr(at));
            break;
        }
        kept.append(line.substr(at, found - at));
        std::size_t end = line.find_first_of(" \t", found);
        if (end == std::string_view::npos)
            end = line.size();
        while (end > found && std::string_view(".,;:!?").find(line[end - 1]) != std::string_view::npos)
            --end;
        const char before = found == 0 ? ' ' : line[found - 1];
        if (before == '(' || before == '<')
            kept.append(line.substr(found, end - found));
        else
            kept.append("<").append(line.substr(found, end - found)).append(">");
        at = end;
    }
    return kept;
}

std::string_view ReleaseNotes::trimmed(std::string_view line)
{
    const std::size_t first = line.find_first_not_of(" \t");
    if (first == std::string_view::npos)
        return {};
    const std::size_t last = line.find_last_not_of(" \t");
    return line.substr(first, last - first + 1);
}

bool ReleaseNotes::isRule(std::string_view line)
{
    if (line.size() < 3)
        return false;
    const char mark = line.front();
    if (mark != '-' && mark != '*' && mark != '_')
        return false;
    return std::ranges::all_of(line, [mark](char each) { return each == mark || each == ' '; });
}

bool ReleaseNotes::isFence(std::string_view line)
{
    return line.starts_with("```") || line.starts_with("~~~");
}

bool ReleaseNotes::isTableRow(std::string_view line)
{
    return line.starts_with('|');
}

std::string ReleaseNotes::reshaped(std::string_view body)
{
    std::vector<std::string> lines;
    std::optional<std::string> heading;
    bool fenced = false;
    bool gap = false;
    auto emit = [&](std::string line) {
        if (heading) {
            if (!lines.empty())
                lines.emplace_back();
            lines.push_back("**" + *heading + "**");
            heading.reset();
            gap = false;
        }
        if (gap && !lines.empty())
            lines.emplace_back();
        gap = false;
        lines.push_back(std::move(line));
    };
    while (!body.empty()) {
        const std::size_t end = body.find('\n');
        const std::string_view raw = body.substr(0, end);
        body.remove_prefix(end == std::string_view::npos ? body.size() : end + 1);
        const std::string_view line = trimmed(raw);
        if (isFence(line)) {
            fenced = !fenced;
            continue;
        }
        if (fenced || isTableRow(line) || isRule(line))
            continue;
        if (line.empty()) {
            gap = true;
            continue;
        }
        const std::string visible(trimmed(linked(withoutImages(line))));
        if (visible.empty())
            continue;
        if (visible.starts_with('#')) {
            const std::size_t words = visible.find_first_not_of('#');
            if (words != std::string::npos && visible[words] == ' ') {
                heading = std::string(trimmed(std::string_view(visible).substr(words)));
                continue;
            }
        }
        if (visible.size() > 2 && (visible[0] == '-' || visible[0] == '*' || visible[0] == '+') && visible[1] == ' ') {
            emit(std::string(bullet) + std::string(trimmed(std::string_view(visible).substr(2))));
            continue;
        }
        emit(visible);
    }
    std::string joined;
    for (const std::string& line : lines) {
        if (!joined.empty())
            joined.push_back('\n');
        joined.append(line);
    }
    return joined;
}

}
