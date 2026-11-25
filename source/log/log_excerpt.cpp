#include "log/log_excerpt.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace akira::log {

auto LogExcerpt::read(const std::filesystem::path& path) -> std::optional<LogExcerpt>
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream.is_open())
        return std::nullopt;
    const std::streamoff end = stream.tellg();
    if (end < 0)
        return std::nullopt;
    const std::streamoff start = std::max<std::streamoff>(0, end - static_cast<std::streamoff>(maxBytes));
    stream.seekg(start);
    std::string raw(static_cast<std::size_t>(end - start), '\0');
    stream.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    raw.resize(static_cast<std::size_t>(stream.gcount()));
    return of(std::move(raw), static_cast<std::uintmax_t>(end), start > 0);
}

auto LogExcerpt::sanitize(std::string_view input) -> std::string
{
    std::string output;
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        const auto code = static_cast<unsigned char>(input[index]);
        if (code == 0x1b && index + 1 < input.size() && input[index + 1] == '[')
        {
            index += 2;
            while (index < input.size())
            {
                const auto part = static_cast<unsigned char>(input[index]);
                if (part >= 0x40 && part <= 0x7e)
                    break;
                ++index;
            }
            continue;
        }
        if (code == '\r')
            continue;
        if (code == '\n' || code == '\t' || code >= 0x20)
            output.push_back(static_cast<char>(code));
    }
    return output;
}

auto LogExcerpt::levelOf(std::string_view line) -> std::optional<Level>
{
    const auto has = [line](std::string_view tag) { return line.find(tag) != std::string_view::npos; };
    if (has("[ERROR]") || has("[FATAL]"))
        return Level::Error;
    if (has("[WARN]") || has("[WARNING]"))
        return Level::Warning;
    if (has("[DEBUG]"))
        return Level::Debug;
    if (has("[TRACE]") || has("[VERBOSE]"))
        return Level::Verbose;
    if (has("[INFO]"))
        return Level::Info;
    return std::nullopt;
}

auto LogExcerpt::blocksOf(std::string_view text) -> std::vector<Block>
{
    std::vector<Block> blocks;
    Level current = Level::Info;
    std::size_t offset = 0;
    while (offset < text.size())
    {
        const std::size_t newline = text.find('\n', offset);
        const std::size_t length = newline == std::string_view::npos ? text.size() - offset : newline - offset + 1;
        const std::string_view line = text.substr(offset, length);
        current = levelOf(line).value_or(current);
        if (blocks.empty() || blocks.back().level != current)
            blocks.push_back(Block{ .level = current, .text = std::string(line) });
        else
            blocks.back().text += line;
        offset += length;
    }
    for (Block& block : blocks)
        while (!block.text.empty() && block.text.back() == '\n')
            block.text.pop_back();
    return blocks;
}

auto LogExcerpt::of(std::string raw, std::uintmax_t totalBytes, bool startedMidway) -> LogExcerpt
{
    if (startedMidway)
    {
        const std::size_t newline = raw.find('\n');
        if (newline != std::string::npos)
            raw.erase(0, newline + 1);
    }
    std::string clean = sanitize(raw);
    LogExcerpt made;
    made.total = totalBytes;
    made.cut = startedMidway;
    std::size_t lines = clean.empty() ? 0 : 1 + static_cast<std::size_t>(std::ranges::count(clean, '\n'));
    if (lines > maxLines)
    {
        std::size_t keepFrom = clean.size();
        std::size_t found = 0;
        while (keepFrom > 0 && found < maxLines)
        {
            --keepFrom;
            if (clean[keepFrom] == '\n')
                ++found;
        }
        if (keepFrom < clean.size())
            clean.erase(0, keepFrom + 1);
        lines = maxLines;
        made.cut = true;
    }
    made.lineCount = lines;
    made.shown = blocksOf(clean);
    return made;
}

}
