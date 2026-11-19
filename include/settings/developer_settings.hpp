#pragma once

#include <array>
#include <string>
#include <string_view>

#include "log/log.hpp"
#include "settings/chiaki_log_verbosity.hpp"
#include "settings/section.hpp"
#include "settings/takion_target.hpp"

namespace akira::log {
class CoreLog;
}

namespace akira::settings {

class SettingsStore;

class DeveloperSettings final : public Section {
public:
    explicit DeveloperSettings(SettingsStore& store);
    ~DeveloperSettings() override;

    DeveloperSettings(const DeveloperSettings&) = delete;
    DeveloperSettings& operator=(const DeveloperSettings&) = delete;

    void write(toml::table& config) const override;

    int takionVersion(TakionTarget target) const;
    void setTakionVersion(TakionTarget target, int version);

    bool powerUserMenuUnlocked() const { return powerUserUnlocked; }
    void setPowerUserMenuUnlocked(bool unlocked) { powerUserUnlocked = unlocked; }
    bool ipcStatsEnabled() const { return ipcStats; }
    void setIpcStatsEnabled(bool enabled) { ipcStats = enabled; }
    bool fakeHosts() const { return devFakeHosts; }
    void setFakeHosts(bool enabled) { devFakeHosts = enabled; }
    const std::string& updateServer() const { return devUpdateServer; }
    void setUpdateServer(const std::string& server) { devUpdateServer = server; }

    bool fileLogging() const { return enableFileLogging && !silenced; }
    void setFileLogging(bool enabled) { enableFileLogging = enabled; }
    bool silenceAllLogs() const { return silenced; }
    void setSilenceAllLogs(bool silence);

    ChiakiLogVerbosity chiakiLogVerbosity() const { return verbosity; }
    void setChiakiLogVerbosity(ChiakiLogVerbosity value);
    void applyChiakiLogVerbosity();
    bool chiakiDebugLogging() const;

    bool lwipLog() const { return debugLwipLog; }
    void setLwipLog(bool enabled) { debugLwipLog = enabled; }
    bool wireguardLog() const { return debugWireguardLog; }
    void setWireguardLog(bool enabled) { debugWireguardLog = enabled; }
    bool renderLog() const { return debugRenderLog; }
    void setRenderLog(bool enabled) { debugRenderLog = enabled; }
    bool discoveryLog() const { return debugDiscoveryLog; }
    void setDiscoveryLog(bool enabled) { debugDiscoveryLog = enabled; }
    bool ffmpegLog() const { return debugFfmpegLog; }
    void setFfmpegLog(bool enabled) { debugFfmpegLog = enabled; }

    void setCoreLog(log::CoreLog* log);
    void followStream(bool streaming);

private:
    static constexpr std::array targets{TakionTarget::PS4, TakionTarget::PS5, TakionTarget::PS5Cloud, TakionTarget::PSNow};

    static std::string_view targetKey(TakionTarget target);
    static ChiakiLogVerbosity verbosityOf(const std::string& value);
    static const char* textOf(ChiakiLogVerbosity value);
    static log::Level appLevel(ChiakiLogVerbosity value);

    void read(const toml::table& config);
    void updateCoreLog();

    SettingsStore& store;
    log::CoreLog* coreLog = nullptr;
    bool streamActive = false;

    std::array<int, 4> takionVersions{};
    bool powerUserUnlocked = false;
    bool ipcStats = false;
    bool devFakeHosts = false;
    std::string devUpdateServer;
    bool enableFileLogging = false;
    bool silenced = false;
    ChiakiLogVerbosity verbosity = ChiakiLogVerbosity::Normal;
    bool debugLwipLog = false;
    bool debugWireguardLog = false;
    bool debugRenderLog = false;
    bool debugDiscoveryLog = false;
    bool debugFfmpegLog = false;
};

}
