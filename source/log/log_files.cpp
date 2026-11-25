#include "log/log_files.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <format>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace akira::log {

void LogFiles::pruneOldLogs()
{
    static constexpr size_t KEEP = 40;

    DIR* dir = opendir(directory);
    if (!dir)
        return;

    std::vector<std::pair<time_t, std::string>> logFiles;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() <= 4 || name.substr(name.size() - 4) != ".log")
            continue;
        std::string path = std::string(directory) + "/" + name;
        struct stat st;
        time_t mtime = (stat(path.c_str(), &st) == 0) ? st.st_mtime : 0;
        logFiles.emplace_back(mtime, std::move(path));
    }
    closedir(dir);

    if (logFiles.size() < KEEP)
        return;

    std::sort(logFiles.begin(), logFiles.end());
    size_t toDelete = logFiles.size() - (KEEP - 1);
    for (size_t i = 0; i < toDelete; i++)
        remove(logFiles[i].second.c_str());
}

std::string LogFiles::appLogPath()
{
    mkdir(directory, 0755);

    pruneOldLogs();

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    return std::format("{}/{:02}{:02}{:02}_{:02}{:02}{:02}.log",
             directory, t->tm_mday, t->tm_mon + 1, t->tm_year % 100,
             t->tm_hour, t->tm_min, t->tm_sec);
}

std::string LogFiles::connectionLogPath(const std::string& connType)
{
    mkdir(directory, 0755);

    pruneOldLogs();

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    return std::format("{}/{:02}{:02}{:02}_{:02}{:02}{:02}_{}.log",
             directory, t->tm_mday, t->tm_mon + 1, t->tm_year % 100,
             t->tm_hour, t->tm_min, t->tm_sec, connType);
}

}
