#include "settings/settings_store.hpp"
#include "settings/migrations/registry.hpp"
#include "util/base64.hpp"
#include "log/log.hpp"

#include <libchiaking/consoles_types.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <map>
#include <regex>
#include <string>
#include <sys/stat.h>

namespace akira::settings {

SettingsStore::SettingsStore(std::filesystem::path directory)
    : directory(std::move(directory)),
      file(this->directory / "akira.toml"),
      legacyFile(this->directory / "akira.conf")
{
    ensureDirectory();
    load();
}

void SettingsStore::attach(const Section& section)
{
    sections.push_back(&section);
}

void SettingsStore::attachLive(const Section& section)
{
    liveSections.push_back(&section);
}

void SettingsStore::detach(const Section& section)
{
    std::erase(sections, &section);
    std::erase(liveSections, &section);
}

toml::table& SettingsStore::table(toml::table& config, std::string_view key)
{
    if (auto* existing = config[key].as_table())
        return *existing;
    return *config.insert_or_assign(key, toml::table{}).first->second.as_table();
}

void SettingsStore::ensureDirectory() const
{
    mkdir(directory.c_str(), 0755);
}

void SettingsStore::load()
{
    struct stat buffer;
    if (stat(file.c_str(), &buffer) == 0) {
        parseToml();
    } else if (stat(legacyFile.c_str(), &buffer) == 0) {
        log::info("Migrating from legacy config format");
        parseLegacy();
        rewrite = true;
    } else {
        log::info("No config file found, using defaults");
    }
}

void SettingsStore::parseToml()
{
    log::info("Parsing TOML config file: {}", file.string());

    try {
        config = toml::parse_file(file.string());

        // Best-effort: on failure (e.g. newer config version) load fields as-is
        // and skip the rewrite rather than aborting startup.
        const int64_t version = config["version"].value_or(int64_t{0});
        try {
            if (migrations::buildSettingsMigrator().migrate(config).changed()) {
                migratedFrom = version;
                rewrite = true;
            }
        } catch (const tomlmigrate::MigrationError& err) {
            log::error("Config migration skipped, loading fields as-is: {}", err.what());
        }
    } catch (const toml::parse_error& err) {
        config = toml::table{};
        log::error("Failed to parse TOML config: {}", err.what());
    } catch (const std::exception& err) {
        config = toml::table{};
        log::error("Failed to load TOML config: {}", err.what());
    }
}

void SettingsStore::parseLegacy()
{
    log::info("Parsing legacy config file: {}", legacyFile.string());

    std::ifstream configFile(legacyFile);
    if (!configFile.is_open()) {
        log::error("Failed to open legacy config file");
        return;
    }

    enum class ConfigItem {
        Unknown, HostName, HostAddr, PsnOnlineId, PsnAccountId, PsnRefreshToken,
        PsnAccessToken, ConsolePIN, RpKey, RpKeyType, RpRegistKey, VideoResolution,
        VideoFps, Target, Haptic, RemoteDuid, CompanionPort,
        PsnTokenExpiresAt, GlobalDuid
    };

    const std::map<ConfigItem, std::regex> regexMap = {
        {ConfigItem::HostName, std::regex("^\\[\\s*(.+)\\s*\\]")},
        {ConfigItem::HostAddr, std::regex("^\\s*host_(?:ip|addr)\\s*=\\s*\"?((\\d+\\.\\d+\\.\\d+\\.\\d+)|([A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)+))\"?")},
        {ConfigItem::PsnOnlineId, std::regex("^\\s*psn_online_id\\s*=\\s*\"?([\\w_-]+)\"?")},
        {ConfigItem::PsnAccountId, std::regex("^\\s*psn_account_id\\s*=\\s*\"?([\\w/=+]+)\"?")},
        {ConfigItem::PsnRefreshToken, std::regex("^\\s*psn_refresh_token\\s*=\\s*\"?([\\w._-]+)\"?")},
        {ConfigItem::PsnAccessToken, std::regex("^\\s*psn_access_token\\s*=\\s*\"?([\\w._-]+)\"?")},
        {ConfigItem::ConsolePIN, std::regex("^\\s*console_pin\\s*=\\s*\"?(\\d{4})\"?")},
        {ConfigItem::RpKey, std::regex("^\\s*rp_key\\s*=\\s*\"?([\\w/=+]+)\"?")},
        {ConfigItem::RpKeyType, std::regex("^\\s*rp_key_type\\s*=\\s*\"?(\\d)\"?")},
        {ConfigItem::RpRegistKey, std::regex("^\\s*rp_regist_key\\s*=\\s*\"?([\\w/=+]+)\"?")},
        {ConfigItem::VideoResolution, std::regex("^\\s*video_resolution\\s*=\\s*\"?(1080p|720p|540p|360p)\"?")},
        {ConfigItem::VideoFps, std::regex("^\\s*video_fps\\s*=\\s*\"?(60|30)\"?")},
        {ConfigItem::Target, std::regex("^\\s*target\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::Haptic, std::regex("^\\s*haptic\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::RemoteDuid, std::regex("^\\s*remote_duid\\s*=\\s*\"?([0-9a-fA-F]+)\"?")},
        {ConfigItem::CompanionPort, std::regex("^\\s*companion_port\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::PsnTokenExpiresAt, std::regex("^\\s*psn_token_expires_at\\s*=\\s*\"?(\\d+)\"?")},
        {ConfigItem::GlobalDuid, std::regex("^\\s*global_duid\\s*=\\s*\"?([0-9a-fA-F]+)\"?")}
    };

    auto parseLine = [&regexMap](const std::string& line, std::string& value) -> ConfigItem {
        std::smatch match;
        for (const auto& [item, regex] : regexMap) {
            if (std::regex_search(line, match, regex)) {
                value = match[1];
                return item;
            }
        }
        return ConfigItem::Unknown;
    };

    using PairingKeys = libchiaking::consoles::PairingKeys;
    static constexpr int64_t legacyProfileId = 1;

    std::map<std::string, toml::table> hosts;
    toml::array registrations;
    std::optional<toml::table> profile;
    int64_t nextConsoleId = 1;
    toml::table video;
    toml::table input;
    toml::table network;

    auto ensureProfile = [&profile]() -> toml::table& {
        if (!profile) {
            profile.emplace();
            profile->insert("profile_id", legacyProfileId);
        }
        return *profile;
    };

    std::string line;
    std::string value;
    toml::table* currentHost = nullptr;
    bool rpKeySet = false, rpRegistKeySet = false, rpKeyTypeSet = false;
    std::string legacyKeyText;
    std::string legacyRegistKeyText;
    std::array<std::byte, PairingKeys::registration_key_size> legacyRegistKey{};
    std::array<std::byte, PairingKeys::remote_play_key_size> legacyKey{};
    uint32_t legacyKeyType = 0;

    while (std::getline(configFile, line)) {
        ConfigItem item = parseLine(line, value);

        switch (item) {
            case ConfigItem::Unknown:
                break;
            case ConfigItem::HostName: {
                std::string cleanName = value;
                int64_t migratedType = 0;
                if (value.length() > 9 && value.substr(value.length() - 9) == " (Remote)") {
                    migratedType = 3;
                } else if (value.rfind("[Manual] ", 0) == 0) {
                    cleanName = value.substr(9);
                    migratedType = 2;
                } else if (value.rfind("[Auto] ", 0) == 0) {
                    cleanName = value.substr(7);
                    migratedType = 1;
                }
                if (cleanName != value) {
                    if (auto existing = hosts.find(cleanName); existing != hosts.end() &&
                        existing->second["host_type"].value_or(int64_t{0}) == 2 && migratedType != 2) {
                        log::info("Skipping {} - Manual host {} already exists", value, cleanName);
                        currentHost = nullptr;
                        break;
                    }
                }
                auto [entry, created] = hosts.try_emplace(cleanName);
                if (created) {
                    entry->second.insert("nickname", cleanName);
                    entry->second.insert("console_id", nextConsoleId++);
                }
                currentHost = &entry->second;
                currentHost->insert_or_assign("host_type", migratedType);
                rpKeySet = rpRegistKeySet = rpKeyTypeSet = false;
                break;
            }
            case ConfigItem::HostAddr:
                if (currentHost) currentHost->insert_or_assign("host_addr", value);
                break;
            case ConfigItem::PsnOnlineId:
                ensureProfile().insert_or_assign("online_id", value);
                break;
            case ConfigItem::PsnAccountId:
                ensureProfile().insert_or_assign("account_id", value);
                break;
            case ConfigItem::PsnRefreshToken:
            case ConfigItem::PsnAccessToken:
                break;
            case ConfigItem::ConsolePIN:
                if (currentHost) currentHost->insert_or_assign("console_pin", value);
                break;
            case ConfigItem::RpKey:
                if (currentHost) {
                    rpKeySet = akira::base64::decodeExactly(value, legacyKey);
                    legacyKeyText = value;
                }
                break;
            case ConfigItem::RpKeyType:
                if (currentHost) {
                    legacyKeyType = static_cast<uint32_t>(std::atoi(value.c_str()));
                    rpKeyTypeSet = true;
                }
                break;
            case ConfigItem::RpRegistKey:
                if (currentHost) {
                    rpRegistKeySet = akira::base64::decodeExactly(value, legacyRegistKey);
                    legacyRegistKeyText = value;
                }
                break;
            case ConfigItem::VideoResolution:
                table(video, "local").insert_or_assign("resolution", value);
                table(video, "remote").insert_or_assign("resolution", value);
                break;
            case ConfigItem::VideoFps:
                table(video, "local").insert_or_assign("fps", static_cast<int64_t>(std::atoi(value.c_str())));
                table(video, "remote").insert_or_assign("fps", static_cast<int64_t>(std::atoi(value.c_str())));
                break;
            case ConfigItem::Haptic: {
                const int64_t preset = value == "1" ? 1 : value == "2" ? 2 : 0;
                if (currentHost)
                    currentHost->insert_or_assign("haptic", preset);
                else
                    input.insert_or_assign("haptic", preset);
                break;
            }
            case ConfigItem::RemoteDuid:
                if (currentHost) currentHost->insert_or_assign("remote_duid", value);
                break;
            case ConfigItem::Target:
                if (currentHost) currentHost->insert_or_assign("target", static_cast<int64_t>(std::atoll(value.c_str())));
                break;
            case ConfigItem::CompanionPort: {
                int port = std::atoi(value.c_str());
                if (port <= 0 || port > 65535) port = 8080;
                network.insert_or_assign("companion_port", static_cast<int64_t>(port));
                break;
            }
            case ConfigItem::PsnTokenExpiresAt:
            case ConfigItem::GlobalDuid:
                break;
        }

        if (rpKeySet && rpRegistKeySet && rpKeyTypeSet && currentHost) {
            if (PairingKeys::create(legacyRegistKey, legacyKey, legacyKeyType)) {
                ensureProfile();
                toml::table row;
                row.insert("console_id", (*currentHost)["console_id"].value_or(int64_t{0}));
                row.insert("profile_id", legacyProfileId);
                row.insert("rp_key", legacyKeyText);
                row.insert("rp_regist_key", legacyRegistKeyText);
                row.insert("rp_key_type", static_cast<int64_t>(legacyKeyType));
                registrations.push_back(std::move(row));
            }
            rpKeySet = rpRegistKeySet = rpKeyTypeSet = false;
        }
    }

    configFile.close();
    libchiaking::memory::secure_zero(std::span<std::byte>{legacyRegistKey});
    libchiaking::memory::secure_zero(std::span<std::byte>{legacyKey});

    config = toml::table{};
    config.insert("version", migrations::buildSettingsMigrator().latest_version());
    table(video, "local").insert_or_assign("bitrate", int64_t{10000});
    table(video, "remote").insert_or_assign("bitrate", int64_t{10000});
    config.insert("video", std::move(video));
    if (!input.empty())
        config.insert("input", std::move(input));
    if (!network.empty())
        config.insert("network", std::move(network));
    if (profile) {
        toml::array profiles;
        profiles.push_back(std::move(*profile));
        config.insert("profiles", std::move(profiles));
        config.insert("active_profile_id", legacyProfileId);
    }
    toml::array consoles;
    for (auto& [name, row] : hosts)
        consoles.push_back(std::move(row));
    if (!consoles.empty())
        config.insert("consoles", std::move(consoles));
    if (!registrations.empty())
        config.insert("registrations", std::move(registrations));
    log::info("Loaded {} host(s) from legacy config", hosts.size());
}

void SettingsStore::finishLoading()
{
    if (rewrite) {
        if (migratedFrom) {
            log::info("Config migrated to current schema, rewriting");
            backup(*migratedFrom);
        }
        save();
    } else {
        owned = snapshot();
    }
    config = toml::table{};
}

void SettingsStore::backup(int64_t version) const
{
    const std::string source = file.string();
    const std::string backupPath = std::format("{}.v{}.bak", source, version);
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(backupPath, std::ios::binary | std::ios::trunc);
    if (input && output)
        output << input.rdbuf();
    output.flush();
    const auto written = output.good() ? static_cast<std::streamoff>(output.tellp()) : std::streamoff{-1};
    std::error_code error;
    const auto expected = std::filesystem::file_size(file, error);
    if (error || written < 0 || static_cast<std::uintmax_t>(written) != expected)
        log::error("Could not back up {} to {} before migrating", source, backupPath);
    else
        log::info("Backed up {} to {} before migrating", source, backupPath);
}

toml::table SettingsStore::snapshot() const
{
    toml::table snapshot;
    snapshot.insert("version", migrations::buildSettingsMigrator().latest_version());
    for (const Section* section : sections)
        section->write(snapshot);
    return snapshot;
}

void SettingsStore::save()
{
    static_cast<void>(trySave());
}

bool SettingsStore::trySave()
{
    std::lock_guard<std::mutex> writeLock(writeMutex);
    log::info("Writing config file: {}", file.string());
    ensureDirectory();
    owned = snapshot();
    return persist(owned);
}

bool SettingsStore::saveLive()
{
    std::lock_guard<std::mutex> writeLock(writeMutex);
    log::info("Writing the stores to {}", file.string());
    ensureDirectory();
    return persist(owned);
}

bool SettingsStore::persist(toml::table snapshot)
{
    for (const Section* section : liveSections)
        section->write(snapshot);

    const std::string target = file.string();
    const std::string tmpPath = std::format("{}.{}.tmp", target, writeSeq++);
    {
        std::ofstream configFile(tmpPath, std::ios::out | std::ios::trunc);
        if (!configFile.is_open()) {
            log::error("Failed to open config file for writing");
            return false;
        }
        configFile << snapshot;
        configFile.flush();
        configFile.close();
        if (configFile.fail()) {
            log::error("Failed to flush config to {}", tmpPath);
            std::remove(tmpPath.c_str());
            return false;
        }
    }

    std::remove(target.c_str());
    if (std::rename(tmpPath.c_str(), target.c_str()) != 0) {
        log::error("Failed to move config into place");
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

}
