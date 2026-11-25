#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace akira::trophies {

class IconCache {
public:
    using Bytes = std::vector<std::uint8_t>;

    IconCache(std::filesystem::path directory, std::size_t memoryLimit, std::size_t iconLimit);

    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    std::optional<Bytes> remembered(const std::string& url) const;
    void remember(const std::string& url, const Bytes& bytes);
    std::size_t rememberedBytes() const;

    std::optional<Bytes> load(const std::string& url);
    void save(const std::string& url, const Bytes& bytes);
    bool savesToDisk() const { return diskWritable.load(); }

    void discard(const std::string& url);
    void clear();

    std::filesystem::path pathFor(const std::string& url) const;

private:
    void ensureDirectory();
    void stopDiskWrites(const std::string& reason);

    const std::filesystem::path directory;
    const std::size_t memoryLimit;
    const std::size_t iconLimit;

    std::atomic<bool> directoryReady{false};
    std::atomic<bool> diskWritable{true};

    mutable std::mutex mutex;
    std::unordered_map<std::string, Bytes> memory;
    std::deque<std::string> order;
    std::size_t memoryBytes = 0;
};

}
