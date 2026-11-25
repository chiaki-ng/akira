#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

#include "settings/section.hpp"

namespace akira::settings {

class SettingsStore {
public:
    explicit SettingsStore(std::filesystem::path directory);

    SettingsStore(const SettingsStore&) = delete;
    SettingsStore& operator=(const SettingsStore&) = delete;

    const toml::table& loaded() const { return config; }

    void attach(const Section& section);
    void attachLive(const Section& section);
    void detach(const Section& section);

    void finishLoading();

    void save();
    [[nodiscard]] bool trySave();
    [[nodiscard]] bool saveLive();

    const std::filesystem::path& path() const { return file; }

    static toml::table& table(toml::table& config, std::string_view key);

private:
    void load();
    void parseToml();
    void parseLegacy();
    void backup(int64_t version) const;
    void ensureDirectory() const;
    toml::table snapshot() const;
    bool persist(toml::table snapshot);

    std::filesystem::path directory;
    std::filesystem::path file;
    std::filesystem::path legacyFile;

    toml::table config;
    bool rewrite = false;
    std::optional<int64_t> migratedFrom;

    std::vector<const Section*> sections;
    std::vector<const Section*> liveSections;
    toml::table owned;

    std::mutex writeMutex;
    std::atomic<uint64_t> writeSeq{0};
};

}
