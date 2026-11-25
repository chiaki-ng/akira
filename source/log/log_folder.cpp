#include "log/log_folder.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <format>
#include <system_error>
#include <tuple>

namespace akira::log {

auto LogFolder::list(const std::filesystem::path& folder) -> std::optional<std::vector<Entry>>
{
    std::error_code error;
    std::filesystem::directory_iterator walk(folder, error);
    if (error)
        return std::nullopt;
    std::vector<Entry> entries;
    for (const std::filesystem::directory_entry& item : walk)
    {
        const std::filesystem::path path = item.path();
        struct stat info {};
        if (lstat(path.c_str(), &info) != 0 || S_ISLNK(info.st_mode))
            continue;
        if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode))
            continue;
        entries.push_back(Entry{ .name = path.filename().string(),
            .path = path,
            .directory = S_ISDIR(info.st_mode),
            .size = static_cast<std::uintmax_t>(info.st_size),
            .modified = info.st_mtime });
    }
    sort(entries);
    return entries;
}

void LogFolder::sort(std::vector<Entry>& entries)
{
    std::ranges::sort(entries, [](const Entry& left, const Entry& right) {
        if (left.directory != right.directory)
            return left.directory;
        if (!left.directory && left.modified != right.modified)
            return left.modified > right.modified;
        return left.name < right.name;
    });
}

auto LogFolder::sizeText(std::uintmax_t bytes) -> std::string
{
    constexpr std::uintmax_t kilobyte = 1024;
    if (bytes < kilobyte)
        return std::format("{} B", bytes);
    if (bytes < kilobyte * kilobyte)
        return std::format("{} KB", (bytes + kilobyte / 2) / kilobyte);
    return std::format("{:.1f} MB", static_cast<double>(bytes) / static_cast<double>(kilobyte * kilobyte));
}

auto LogFolder::modifiedText(std::time_t modified) -> std::string
{
    if (modified <= 0)
        return {};
    std::tm local{};
    if (!localtime_r(&modified, &local))
        return {};
    std::array<char, 32> text{};
    if (std::strftime(text.data(), text.size(), "%d %b %Y  %H:%M", &local) == 0)
        return {};
    return text.data();
}

}
