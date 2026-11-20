#include "settings/config_listing.hpp"

#include <fstream>
#include <utility>

namespace akira::settings {

auto ConfigListing::read(const std::filesystem::path& file, bool revealed) -> std::optional<std::vector<Line>>
{
    std::ifstream stream(file);
    if (!stream.is_open())
        return std::nullopt;
    std::vector<Line> lines;
    std::string raw;
    while (std::getline(stream, raw))
        lines.push_back(parse(revealed ? raw : censored(raw)));
    return lines;
}

auto ConfigListing::masked(std::string_view value) -> std::string
{
    if (value.size() <= 5)
        return std::string(value);
    return "****" + std::string(value.substr(value.size() - 5));
}

auto ConfigListing::censored(const std::string& line) -> std::string
{
    for (const std::string_view key : sensitiveKeys)
    {
        const std::size_t keyAt = line.find(key);
        if (keyAt == std::string::npos)
            continue;
        const std::size_t equals = line.find('=', keyAt);
        if (equals == std::string::npos)
            continue;
        std::size_t start = line.find_first_not_of(" \t", equals + 1);
        if (start == std::string::npos)
            continue;
        char quote = 0;
        if (line[start] == '\'' || line[start] == '"')
            quote = line[start++];
        std::size_t end = line.size();
        if (quote != 0 && end > start && line[end - 1] == quote)
            --end;
        if (start >= end)
            continue;
        const std::string head = line.substr(0, equals + 1) + " ";
        const std::string hidden = masked(std::string_view(line).substr(start, end - start));
        if (quote != 0)
            return head + quote + hidden + quote;
        return head + hidden;
    }
    return line;
}

auto ConfigListing::scalar(std::string_view text) -> Token
{
    if (text.empty())
        return Token::Plain;
    const char first = text.front();
    if (first == '\'' || first == '"')
        return Token::String;
    if (text.starts_with("true") || text.starts_with("false"))
        return Token::Boolean;
    if (first == '-' || first == '+' || (first >= '0' && first <= '9'))
        return Token::Number;
    return Token::Plain;
}

auto ConfigListing::value(std::string_view text) -> Token
{
    if (!text.starts_with('['))
        return scalar(text);
    const std::size_t element = text.find_first_not_of(" \t", 1);
    if (element == std::string_view::npos || text[element] == ']')
        return Token::Punct;
    return scalar(text.substr(element));
}

auto ConfigListing::trimmed(std::string_view text) -> std::string_view
{
    const std::size_t begin = text.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
        return {};
    return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
}

auto ConfigListing::indent(std::string_view line) -> std::string_view
{
    const std::size_t begin = line.find_first_not_of(" \t");
    return line.substr(0, begin == std::string_view::npos ? line.size() : begin);
}

auto ConfigListing::section(std::string lead, std::string_view body) -> Line
{
    const std::size_t depth = body.starts_with("[[") ? 2 : 1;
    const std::string_view name = body.substr(depth, body.size() - depth * 2);
    Line line{ .header = true };
    line.pieces.push_back({ std::move(lead) + std::string(body.substr(0, depth)), Token::Punct });
    std::size_t segment = 0;
    while (segment <= name.size())
    {
        const std::size_t dot = name.find('.', segment);
        const std::size_t end = dot == std::string_view::npos ? name.size() : dot;
        line.pieces.push_back({ std::string(name.substr(segment, end - segment)), Token::Header });
        if (dot == std::string_view::npos)
            break;
        line.pieces.push_back({ ".", Token::Punct });
        segment = dot + 1;
    }
    line.pieces.push_back({ std::string(body.substr(body.size() - depth)), Token::Punct });
    return line;
}

auto ConfigListing::parse(std::string_view raw) -> Line
{
    const std::string lead(indent(raw));
    const std::string_view body = trimmed(raw);
    if (body.empty())
        return Line{};
    if (body.starts_with('#'))
        return Line{ .pieces = { { lead + std::string(body), Token::Comment } } };
    if (body.starts_with('[') && body.ends_with(']'))
        return section(lead, body);
    const std::size_t equals = body.find('=');
    if (equals == std::string_view::npos)
        return Line{ .pieces = { { lead + std::string(body), Token::Plain } } };
    const std::string_view key = trimmed(body.substr(0, equals));
    const std::string_view assigned = trimmed(body.substr(equals + 1));
    return Line{ .pieces = { { lead + std::string(key), Token::Key }, { " = ", Token::Punct },
                     { std::string(assigned), value(assigned) } } };
}

}
