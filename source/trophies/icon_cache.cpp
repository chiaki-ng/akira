#include "trophies/icon_cache.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <system_error>
#include <utility>

#include <sys/stat.h>

namespace akira::trophies {

IconCache::IconCache(std::filesystem::path directory, std::size_t memoryLimit, std::size_t iconLimit)
    : directory(std::move(directory)), memoryLimit(memoryLimit), iconLimit(iconLimit)
{
}

std::optional<IconCache::Bytes> IconCache::remembered(const std::string& url) const
{
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = memory.find(url);
    if (entry == memory.end())
        return std::nullopt;
    return entry->second;
}

void IconCache::remember(const std::string& url, const Bytes& bytes)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (memory.find(url) == memory.end())
    {
        memoryBytes += bytes.size();
        memory[url] = bytes;
        order.push_back(url);
    }

    while (memoryBytes > memoryLimit && !order.empty())
    {
        const std::string oldest = order.front();
        order.pop_front();

        if (oldest == url)
            continue;

        auto entry = memory.find(oldest);
        if (entry != memory.end())
        {
            memoryBytes -= entry->second.size();
            memory.erase(entry);
        }
    }
}

std::size_t IconCache::rememberedBytes() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return memoryBytes;
}

std::filesystem::path IconCache::pathFor(const std::string& url) const
{
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : url)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    return std::format("{}/{:016x}.img", directory.string(), hash);
}

void IconCache::ensureDirectory()
{
    if (directoryReady.load())
        return;

    const std::filesystem::path parent = directory.parent_path();
    mkdir(parent.parent_path().c_str(), 0755);
    mkdir(parent.c_str(), 0755);
    mkdir(directory.c_str(), 0755);

    directoryReady.store(true);
}

std::optional<IconCache::Bytes> IconCache::load(const std::string& url)
{
    ensureDirectory();

    const std::string path = pathFor(url).string();
    FILE* file = fopen(path.c_str(), "rb");
    if (!file)
        return std::nullopt;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || static_cast<size_t>(size) > iconLimit)
    {
        fclose(file);
        return std::nullopt;
    }

    Bytes bytes(static_cast<size_t>(size));
    size_t read = fread(bytes.data(), 1, bytes.size(), file);
    fclose(file);

    if (read != bytes.size())
        return std::nullopt;

    return bytes;
}

void IconCache::save(const std::string& url, const Bytes& bytes)
{
    if (!diskWritable.load())
        return;

    ensureDirectory();

    const std::string path = pathFor(url).string();
    const std::string temp = path + ".tmp";

    FILE* file = fopen(temp.c_str(), "wb");
    if (!file)
    {
        stopDiskWrites(std::format("could not open {}: {}", temp, strerror(errno)));
        return;
    }

    size_t written = fwrite(bytes.data(), 1, bytes.size(), file);
    fclose(file);

    if (written != bytes.size())
    {
        remove(temp.c_str());
        stopDiskWrites(std::format("short write to {} ({}/{} bytes)", temp, written, bytes.size()));
        return;
    }

    remove(path.c_str());
    if (rename(temp.c_str(), path.c_str()) != 0)
    {
        remove(temp.c_str());
        stopDiskWrites(std::format("could not replace {}", path));
    }
}

void IconCache::stopDiskWrites(const std::string& reason)
{
    if (!diskWritable.exchange(false))
        return;

    log::error("Trophy: icon disk cache disabled for this session ({}); icons will still load from memory", reason);
}

void IconCache::discard(const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto entry = memory.find(url);
        if (entry != memory.end())
        {
            memoryBytes -= entry->second.size();
            memory.erase(entry);
        }

        auto position = std::find(order.begin(), order.end(), url);
        if (position != order.end())
            order.erase(position);
    }

    remove(pathFor(url).string().c_str());
}

void IconCache::clear()
{
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    directoryReady.store(false);

    std::lock_guard<std::mutex> lock(mutex);
    memory.clear();
    order.clear();
    memoryBytes = 0;
}

}
