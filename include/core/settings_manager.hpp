#ifndef AKIRA_SETTINGS_MANAGER_HPP
#define AKIRA_SETTINGS_MANAGER_HPP

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <libchiaking/platform/platform.hpp>

#include "cloud/models.hpp"
#include "core/pairing_store.hpp"
#include "core/sign_in_store.hpp"
#include "profile.hpp"
#include "registration.hpp"

// Forward declaration
#include "input/rumble_profile.hpp"

class Host;

namespace akira {
class CoreLog;
}

enum class VideoResolution : uint32_t {
    P360 = 360,
    P540 = 540,
    P720 = 720,
    P1080 = 1080,
    P1440 = 1440,
    P2160 = 2160
};

enum class VideoFps : uint32_t {
    Fps30 = 30,
    Fps60 = 60
};

enum class HapticPreset {
    Disabled = 0,
    Weak = 1,
    Strong = 2,

    ConsoleRumble = 3
};

enum class GyroSource {
    Auto = 0,
    Left = 1,
    Right = 2
};

enum class TakionTarget : uint8_t {
    PS4,
    PS5,
    PS5Cloud,
    PSNow,
};

enum class ChiakiLogVerbosity {
    Normal = 0,
    Info = 1,
    Debug = 2,
    Trace = 3
};

using ButtonMapping = std::map<uint32_t, std::vector<uint64_t>>;

class SettingsManager {
protected:
    SettingsManager();

public:
    enum class StreamProfile {
        Local = 0,
        Remote = 1,
        Vpn = 2,
        Cloud = 3
    };

private:
    static constexpr const char* CONFIG_DIR = "sdmc:/switch/akira";
    static constexpr const char* TOML_CONFIG_FILE = "sdmc:/switch/akira/akira.toml";
    static constexpr const char* LEGACY_CONFIG_FILE = "sdmc:/switch/akira/akira.conf";

    akira::CoreLog* coreLog = nullptr;
    std::map<std::string, std::unique_ptr<Host>> hosts;

    // Global settings (defaults)
    VideoResolution localVideoResolution = VideoResolution::P720;
    VideoResolution remoteVideoResolution = VideoResolution::P720;
    VideoFps localVideoFPS = VideoFps::Fps60;
    VideoFps remoteVideoFPS = VideoFps::Fps60;
    std::vector<Profile> profiles;
    int64_t activeProfileId = 0;
    int64_t nextProfileId = 1;
    mutable std::recursive_mutex profileMutex;
    int64_t nextConsoleId = 1;
    HapticPreset globalHaptic = HapticPreset::Disabled;
    bool  directRumbleInStream  = false;
    bool  directHapticsInStream = false;


    float rumbleFreqLow = 140.0f;
    float rumbleFreqHigh = 185.0f;
    float rumbleEnvelopeDecay = 0.85f;
    float rumbleEnvelopeAttack = 0.60f;

    std::map<std::string, akira::input::RumbleProfile> rumbleProfiles;
    int localVideoBitrate = 10000;
    int remoteVideoBitrate = 10000;
    int vpnVideoBitrate = 5000;
    int cloudVideoBitratePscloud = 10000;
    int cloudVideoBitratePsnow = 10000;
    int cloudVideoResolutionPscloud = 1080;
    int cloudVideoResolutionPsnow = 1080;
    bool cloudFsrEnabledPscloud = false;
    bool cloudFsrEnabledPsnow = false;
    std::string cloudDuid;
    mutable std::mutex datacenterMutex;
    std::vector<cloud::Datacenter> cloudDatacentersPscloud;
    std::vector<cloud::Datacenter> cloudDatacentersPsnow;
    int cloudSortState = 0;

    std::string cloudFilter;
    VideoResolution vpnVideoResolution = VideoResolution::P720;
    VideoFps vpnVideoFPS = VideoFps::Fps30;
    bool holepunchRetry = false;
    bool connectionShowStages = true;
    bool portGuessing = true;
    int portGuessingCount = 75;
    int portGuessingSocks = 120;
    bool powerUserMenuUnlocked = false;
    bool ipcStatsEnabled = false;
    bool unlockBitrateMax = false;
    bool autoReconnect = true;
    bool devFakeHosts = false;
    bool hideAccountName = false;
    std::string updateChannel = "stable";
    std::string uiTheme = "playstation";
    std::string discoverySubnets;
    bool autoCheckUpdates = true;
    int64_t lastUpdateCheck = 0;
    std::string updateInstallPath;
    std::string devUpdateServer;
    GyroSource globalGyroSource = GyroSource::Auto;
    bool sleepOnExit = false;
    bool requestIdrOnFecFailure = true;
    std::array<int, 4> takionVersions{};
    float packetLossMax = 0.05f;
    bool enableFileLogging = false;
    bool silenceAllLogs = false;
    std::string debugLocale;

    // Picture adjustments
    bool enableDithering = false;
    float ditheringStrength = 3.0f;
    float statsOverlayX = -1.0f;
    float statsOverlayY = -1.0f;

    bool localFsrEnabled = false;
    bool remoteFsrEnabled = false;
    bool vpnFsrEnabled = false;
    bool rcasEnabled = false;
    float rcasSharpness = 0.2f;

    // Debug logging settings
    bool debugLwipLog = false;
    bool debugWireguardLog = false;
    bool debugRenderLog = false;
    ChiakiLogVerbosity chiakiLogVerbosity = ChiakiLogVerbosity::Normal;
    bool debugDiscoveryLog = false;
    bool debugFfmpegLog = false;

    // Runtime state (not persisted)
    bool streamingActive = false;
    StreamProfile activeStreamProfile = StreamProfile::Local;
    bool activeCloudPscloud = true;

    // Companion server settings
    int companionPort = 8080;

    ButtonMapping buttonMapping;

    bool touchpadEnabled = true;
    bool swipeUpEnabled = true;
    bool swipeDownEnabled = true;
    bool swipeLeftEnabled = true;
    bool swipeRightEnabled = true;

    akira::SignInStore signInStore{*this};
    akira::PairingStore pairingStore{*this};
    toml::table owned;

    toml::table ownedSettings();
    int persist(toml::table config);

    void parseTomlFile();
    void parseLegacyFile();
    void backupConfig(int64_t version);
    static std::optional<libchiaking::consoles::Firmware> firmwareFromTarget(int64_t target);
    static int64_t targetOf(const std::optional<libchiaking::consoles::Firmware>& firmware);
    static std::optional<Registration> registrationOf(const toml::table& row);
    static bool fileExists(const char* path);
    void updateChiakiLogMask();

public:
    SettingsManager(const SettingsManager&) = delete;
    void operator=(const SettingsManager&) = delete;
    static SettingsManager* getInstance();

    void setCoreLog(akira::CoreLog* log);
    void ensureCloudDuid(libchiaking::platform::SecureRandomProvider& random);
    akira::SignInStore& signIns() { return signInStore; }
    akira::PairingStore& pairings() { return pairingStore; }

    void parseFile();
    int writeFile();
    int writeStores();
    static std::mutex writeMutex;
    static std::atomic<uint64_t> writeSeq;
    void ensureConfigDir();

    std::map<std::string, std::unique_ptr<Host>>* getHostsMap();
    Host* getOrCreateHost(const std::string& hostName);
    Host* findHostByDuid(const std::string& duid);
    void removeHost(const std::string& hostName);
    void removeActiveProfileRegistration(const std::string& hostName);
    bool adoptLegacyRegistrations(Host& host);
    void renameHost(const std::string& oldName, const std::string& newName);

    static std::string resolutionToString(VideoResolution resolution);
    static int resolutionToInt(VideoResolution resolution);
    static VideoResolution intToResolution(int height);
    static VideoResolution stringToResolution(const std::string& value);

    static std::string fpsToString(VideoFps fps);
    static int fpsToInt(VideoFps fps);
    static VideoFps stringToFps(const std::string& value);

    static int getDefaultBitrateForResolution(VideoResolution res);
    int getMaxBitrateForResolution(VideoResolution res) const;

    static bool isValidIPv4(const std::string& addr);
    static bool isValidFQDN(const std::string& addr);
    static bool isValidHostAddress(const std::string& addr);

    std::string getHostName(Host* host);
    std::string getHostAddr(Host* host);
    void setHostAddr(Host* host, const std::string& addr);
    void setDiscovered(Host* host, bool value);

    std::string getPsnOnlineId(Host* host);
    void setPsnOnlineId(Host* host, const std::string& id);

    std::string getPsnAccountId(Host* host);
    void setPsnAccountId(Host* host, const std::string& id);


    const std::string& getCloudDuid() const { return cloudDuid; }

    const std::vector<Profile>& getProfiles() const;
    std::vector<Profile> profilesSnapshot() const;
    Profile* findProfile(int64_t id);
    const Profile* findProfile(int64_t id) const;
    Profile* getActiveProfile();
    const Profile* getActiveProfile() const;
    Profile* ensureActiveProfile();
    int64_t getActiveProfileId() const;
    void setActiveProfileId(int64_t id);
    void logProfileCensus(const char* stage);
    bool readProfile(int64_t id, const std::function<void(const Profile&)>& fn) const;
    bool updateProfile(int64_t id, const std::function<void(Profile&)>& fn);
    bool getActiveProfileTrophiesEnabled() const;
    void setActiveProfileTrophiesEnabled(bool enabled);
    int64_t addProfile(const Profile& profile);
    void removeProfile(int64_t id);

    /*
     * Legacy is a property of the active profile, not of the install: a household can
     * hold one of each. Everything PSN already reads through getActiveProfile(), so this
     * is the grain the rest of the code already has.
     */
    bool isLegacyProfileActive() const;


    /*
     * Mirrors isLegacyProfileActive() into the HTTP layer. Must be called after anything
     * that changes which profile is active or what credentials it holds - not only on a
     * profile switch - or the gate goes stale in either direction.
     */
    void refreshLegacyGate();

    std::string getConsolePIN(Host* host);
    void setConsolePIN(Host* host, const std::string& pin);

    VideoResolution getVideoResolution(Host* host);
    VideoResolution getLocalVideoResolution() const;
    void setLocalVideoResolution(VideoResolution value);
    VideoResolution getRemoteVideoResolution() const;
    void setRemoteVideoResolution(VideoResolution value);

    VideoFps getVideoFPS(Host* host);
    VideoFps getLocalVideoFPS() const;
    void setLocalVideoFPS(VideoFps value);
    VideoFps getRemoteVideoFPS() const;
    void setRemoteVideoFPS(VideoFps value);

    int getVideoBitrate(Host* host) const;
    int getLocalVideoBitrate() const;
    void setLocalVideoBitrate(int value);
    int getRemoteVideoBitrate() const;
    void setRemoteVideoBitrate(int value);
    int getVpnVideoBitrate() const;
    void setVpnVideoBitrate(int value);
    int getCloudVideoBitrate(bool pscloud) const;
    void setCloudVideoBitrate(bool pscloud, int value);
    int getCloudVideoResolution(bool pscloud) const;
    void setCloudVideoResolution(bool pscloud, int value);
    bool getCloudFsrEnabled(bool pscloud) const;
    void setCloudFsrEnabled(bool pscloud, bool enabled);
    std::vector<cloud::Datacenter> getCloudDatacenters(bool pscloud) const;
    void setCloudDatacenters(bool pscloud, std::vector<cloud::Datacenter> datacenters);
    int getCloudSortState() const;
    void setCloudSortState(int value);

    const std::string& getCloudFilter() const;
    void setCloudFilter(std::string filter);
    const std::vector<cloud::Game>& getCloudShortcuts() const;
    void setCloudShortcuts(std::vector<cloud::Game> shortcuts);
    VideoResolution getVpnVideoResolution() const;
    void setVpnVideoResolution(VideoResolution value);
    VideoFps getVpnVideoFPS() const;
    void setVpnVideoFPS(VideoFps value);

    HapticPreset getHaptic(Host* host);
    void setHaptic(Host* host, HapticPreset value);
    void setHaptic(Host* host, const std::string& value);

    bool getDirectRumbleInStream() const;
    void setDirectRumbleInStream(bool value);
    bool getDirectHapticsInStream() const;
    void setDirectHapticsInStream(bool value);


    akira::input::RumbleProfile resolveRumbleProfile(uint16_t vendorId, uint16_t productId,
                                                     const uint8_t* address,
                                                     bool switchNative, bool joycon) const;

    std::string resolveRumbleKey(uint16_t vendorId, uint16_t productId,
                                 const uint8_t* address, bool switchNative,
                                 bool joycon) const;

    akira::input::RumbleProfile getRumbleProfile(const std::string& key) const;
    void setRumbleProfile(const std::string& key, const akira::input::RumbleProfile& profile);
    bool hasRumbleProfile(const std::string& key) const;

    bool seedRumbleProfile(const std::string& key,
                           const akira::input::RumbleProfile* inheritFrom = nullptr);

    void resetRumbleProfile(const std::string& key);

    int getCompanionPort() const;
    void setCompanionPort(int port);

    GyroSource getGyroSource() const;
    void setGyroSource(GyroSource source);

    bool getHolepunchRetry() const;

    bool getConnectionShowStages() const;
    void setConnectionShowStages(bool show);
    void setHolepunchRetry(bool retry);

    bool getPortGuessing() const;
    void setPortGuessing(bool enabled);
    int getPortGuessingCount() const;
    void setPortGuessingCount(int count);
    int getPortGuessingSocks() const;
    void setPortGuessingSocks(int count);

    bool getPowerUserMenuUnlocked() const;
    void setPowerUserMenuUnlocked(bool unlocked);
    bool getIpcStatsEnabled() const;
    void setIpcStatsEnabled(bool enabled);
    bool getUnlockBitrateMax() const;
    void setUnlockBitrateMax(bool enabled);
    bool getAutoReconnect() const;
    void setAutoReconnect(bool enabled);
    bool getDevFakeHosts() const;
    void setDevFakeHosts(bool enabled);

    bool getHideAccountName() const;
    void setHideAccountName(bool enabled);
    std::string maskAccountName(const std::string& name) const;

    std::string getUpdateChannel() const;
    void setUpdateChannel(const std::string& channel);

    std::string getUiTheme() const;
    void setUiTheme(const std::string& id);

    std::string getDiscoverySubnets() const;
    void setDiscoverySubnets(const std::string& subnets);
    bool getAutoCheckUpdates() const;
    void setAutoCheckUpdates(bool enabled);
    int64_t getLastUpdateCheck() const;
    void setLastUpdateCheck(int64_t epochSeconds);
    std::string getUpdateInstallPath() const;
    void setUpdateInstallPath(const std::string& path);
    std::string getDevUpdateServer() const;
    void setDevUpdateServer(const std::string& server);
    int getMinBitrateForResolution(VideoResolution res) const;


    bool getSleepOnExit() const;
    void setSleepOnExit(bool enabled);

    bool getRequestIdrOnFecFailure() const;
    void setRequestIdrOnFecFailure(bool enabled);

    int getTakionVersion(TakionTarget target) const;
    void setTakionVersion(TakionTarget target, int version);
    static auto takionTargetKey(TakionTarget target) -> std::string_view;

    float getPacketLossMax() const;
    void setPacketLossMax(float value);

    bool getEnableFileLogging() const;
    void setEnableFileLogging(bool enabled);
    bool getSilenceAllLogs() const;
    void setSilenceAllLogs(bool silenced);


    bool getEnableDithering() const;
    void setEnableDithering(bool enabled);

    float getDitheringStrength() const;
    void setDitheringStrength(float value);

    float getStatsOverlayX() const;
    float getStatsOverlayY() const;
    void setStatsOverlayPosition(float x, float y);

    std::string getDebugLocale() const;
    void setDebugLocale(const std::string& locale);

    // Debug logging
    bool getDebugLwipLog() const;
    void setDebugLwipLog(bool enabled);
    bool getDebugWireguardLog() const;
    void setDebugWireguardLog(bool enabled);
    bool getDebugRenderLog() const;
    void setDebugRenderLog(bool enabled);
    ChiakiLogVerbosity getChiakiLogVerbosity() const;
    void setChiakiLogVerbosity(ChiakiLogVerbosity verbosity);
    void applyChiakiLogVerbosity();
    bool getDebugChiakiLog() const;
    bool getDebugDiscoveryLog() const;
    void setDebugDiscoveryLog(bool enabled);
    bool getDebugFfmpegLog() const;
    void setDebugFfmpegLog(bool enabled);

    const ButtonMapping& getButtonMapping() const;
    void setButtonMapping(const ButtonMapping& mapping);
    ButtonMapping getDefaultButtonMapping() const;

    bool getTouchpadEnabled() const;
    void setTouchpadEnabled(bool enabled);
    bool getSwipeUpEnabled() const;
    void setSwipeUpEnabled(bool enabled);
    bool getSwipeDownEnabled() const;
    void setSwipeDownEnabled(bool enabled);
    bool getSwipeLeftEnabled() const;
    void setSwipeLeftEnabled(bool enabled);
    bool getSwipeRightEnabled() const;
    void setSwipeRightEnabled(bool enabled);
    bool isButtonEnabled(uint32_t chiakiButton) const;

    bool getEasuEnabled() const;
    int getEasuTargetHeight() const;
    bool getLocalFsrEnabled() const;
    void setLocalFsrEnabled(bool enabled);
    bool getRemoteFsrEnabled() const;
    void setRemoteFsrEnabled(bool enabled);
    bool getVpnFsrEnabled() const;
    void setVpnFsrEnabled(bool enabled);
    bool getRcasEnabled() const;
    void setRcasEnabled(bool enabled);
    float getRcasSharpness() const;
    void setRcasSharpness(float sharpness);
    StreamProfile getActiveStreamProfile() const;
    void setActiveStreamProfile(StreamProfile profile);
    bool getActiveCloudPscloud() const;
    void setActiveCloudPscloud(bool pscloud);

    bool isStreamingActive() const;
    void setStreamingActive(bool active);

    static constexpr const char* LOG_DIR = "sdmc:/switch/akira/logs";
    static std::string getLogFilePath();
    static std::string getConnectionLogFilePath(const std::string& connType);
};

#endif // AKIRA_SETTINGS_MANAGER_HPP
