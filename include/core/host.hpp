#ifndef AKIRA_HOST_HPP
#define AKIRA_HOST_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <chiaki/common.h>
#include <chiaki/discovery.h>
#include <chiaki/session.h>
#include <chiaki/regist.h>
#include <chiaki/opusdecoder.h>
#include <chiaki/log.h>
#include <chiaki/remote/holepunch.h>
#include <chiaki/remote/rudp.h>

#include "settings_manager.hpp"
#include "registration.hpp"

// Forward declarations
class SettingsManager;
class Session;

// Registration error codes
enum HostRegisterError {
    HOST_REGISTER_OK = 0,
    HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID = 1,
    HOST_REGISTER_ERROR_SETTING_PSNONLINEID = 2,
    HOST_REGISTER_ERROR_UNDEFINED_TARGET = 3,
    HOST_REGISTER_ERROR_HOLEPUNCH = 4
};

enum class HostType {
    Discovered = 0,
    Auto = 1,
    Manual = 2,
    Remote = 3
};

enum class HolepunchPhase {
    Negotiating,
    Punching
};

enum class ConsoleModel {
    PS4Fat,
    PS4Slim,
    PS4Pro,
    PS5Fat,
    PS5Slim,
    PS5Pro
};

enum class CouchPadState : uint8_t {
    Idle,
    Joining,
    NeedsProfile,
    Passcode,
    Announced,
    Confirmed,
    Failed
};

static constexpr uint8_t kCouchJoinTimedOut = 0xff;
static constexpr uint8_t kCouchPadDropped = 0xfe;
static constexpr int kCouchJoinTimeoutSeconds = 6;

class Host {
    friend class SettingsManager;
    friend class DiscoveryManager;

private:
    ChiakiLog* log = nullptr;
    SettingsManager* settings = nullptr;

    // Video/audio config
    ChiakiVideoResolutionPreset videoResolution = CHIAKI_VIDEO_RESOLUTION_PRESET_720p;
    ChiakiVideoFPSPreset videoFps = CHIAKI_VIDEO_FPS_PRESET_60;
    ChiakiConnectVideoProfile videoProfile;
    int haptic = -1;  // -1=inherit from global, 0=disabled, 1=weak, 2=strong

    // User info
    std::string psnOnlineId;
    std::string consolePIN;  // 4-digit login PIN for auto-login (optional)

    // Host identification
    std::string hostName;
    std::string hostId;
    std::string hostAddr;
    std::string serverNickname;

    // Access point info (from registration)
    std::string apSsid;
    std::string apKey;
    std::string apName;

    // Remote connection info
    std::string remoteDuid;  // DUID for matching remote hosts

    // Target console type
    ChiakiTarget target = CHIAKI_TARGET_PS4_UNKNOWN;
	ConsoleModel consoleModel = ConsoleModel::PS5Fat;
    ChiakiDiscoveryHostState state = CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN;

    // Registration data
    int64_t consoleId = 0;
    std::vector<Registration> registrations;

    bool discovered = false;
    bool sessionInit = false;
    bool registInit = false;
    bool psnRemotePlayDisabled = false;
    bool needsLinking = false;
    bool inConfig = false;
    HostType hostType = HostType::Discovered;

    ChiakiHolepunchSession holepunchSession = nullptr;

    // Session components
    ChiakiSession session;
    ChiakiOpusDecoder opusDecoder;
    ChiakiRegist regist;
    ChiakiRegistInfo registInfo;

    ChiakiRudp autoRegistRudp = nullptr;
    ChiakiHolepunchRegistInfo autoRegistHinfo{};
    bool autoRegistActive = false;
    ChiakiControllerState controllerState;
    ChiakiControllerState controllerStates[CHIAKI_COUCH_MAX_PADS];
    uint8_t couchPadCount = 1;
    std::atomic<bool> couchMode{false};
    CouchPadState couchPadStates[CHIAKI_COUCH_MAX_PADS] = {};
    uint8_t couchPadFailStatus[CHIAKI_COUCH_MAX_PADS] = {};
    std::chrono::steady_clock::time_point couchPadJoinAt[CHIAKI_COUCH_MAX_PADS] = {};
    std::array<std::string, CHIAKI_COUCH_MAX_PADS> couchAccountLabels;
    std::array<int64_t, CHIAKI_COUCH_MAX_PADS> couchProfileIds = {};
    std::map<uint32_t, int8_t> fingerIdTouchId;
    std::vector<std::string> transientPs4Nicknames;
    std::atomic<bool> transientPs4NicknameMismatch{false};

    std::atomic<bool> feedbackLoopRunning{false};
    std::thread feedbackThread;
    std::mutex feedbackLoopMutex;
    std::condition_variable feedbackLoopCv;
    std::atomic<bool> inputSuppressed{false};

    bool startFeedbackLoop();
    void stopFeedbackLoop();
    void feedbackLoop();

    // Callbacks
    std::function<void()> onConnected;
    std::function<void(bool)> onLoginPinRequest;
    std::function<void(ChiakiQuitEvent*)> onQuit;
    std::function<void(uint8_t, uint8_t, uint8_t)> onRumble;
    std::function<void(ChiakiControllerState*, std::map<uint32_t, int8_t>*)> onReadController;
    std::function<void(ChiakiControllerState*, uint8_t*)> onReadCouchSecondary;
    std::function<void()> onRegistCanceled;
    std::function<void()> onRegistFailed;
    std::function<void()> onRegistSuccess;
    std::function<void(int, int)> onRegistStage;
    std::function<void()> onMotionReset;
    std::function<void(const ChiakiTriggerEffectsEvent*)> onTriggerEffects;
    std::function<void(HolepunchPhase)> onHolepunchPhase;

    std::function<void(uint8_t, uint8_t)> onEffectIntensity;
    std::function<void(uint8_t, uint8_t, uint8_t)> onLedColor;
    std::function<void(uint8_t, uint8_t, uint8_t, uint8_t)> onPadConfirmed;
    std::function<void(uint8_t, bool)> onPadPasscodeRequest;
    std::function<void(uint8_t, uint8_t)> onPadJoinFailed;

public:
    struct CouchProfileChoice {
        int64_t profileId = 0;
        std::string label;
        std::string accountId;
        std::string avatarUrl;
    };

    struct CloudSessionConfig {
        ChiakiServiceType serviceType = CHIAKI_SERVICE_TYPE_REMOTE_PLAY;
        std::string host;
        uint16_t port = 0;
        std::string launchSpec;
        std::string handshakeKey;
        std::string sessionId;
        uint8_t psnWrapperType = 0;
        uint32_t mtuIn = 0;
        uint32_t mtuOut = 0;
        uint64_t rttUs = 0;
        std::string entitlementId;
        std::string platform;
    };

private:
    std::optional<CloudSessionConfig> cloudSession;

public:
    explicit Host(const std::string& name);
    ~Host();

    // Logger
    void setLogger(ChiakiLog* logger) { this->log = logger; }
    ChiakiLog* getLogger() const { return log; }

    // Host info getters
    std::string getHostName() const { return hostName; }
    std::string getHostAddr() const { return hostAddr; }
    std::string getHostId() const { return hostId; }
    ChiakiTarget getChiakiTarget() const { return target; }
    ChiakiDiscoveryHostState getState() const { return state; }

    // Host info setters
    void setHostAddr(const std::string& addr) { hostAddr = addr; }
    void setChiakiTarget(ChiakiTarget t) { target = t; }
    void setState(ChiakiDiscoveryHostState s) { state = s; }
	
	// Console model getters/setters
    ConsoleModel getConsoleModel() const { return consoleModel; }
    void setConsoleModel(ConsoleModel model) { consoleModel = model; }

    // Status checks
    bool isDiscovered() const { return discovered; }
    bool isRegistered() const { return activeRegistration() != nullptr; }
    bool hasRpKey() const { return activeRegistration() != nullptr; }
    bool isReady() const { return state == CHIAKI_DISCOVERY_HOST_STATE_READY; }
    bool isPS5() const;
    bool isPsnRemotePlayDisabled() const { return psnRemotePlayDisabled; }
    void setPsnRemotePlayDisabled(bool v) { psnRemotePlayDisabled = v; }
    bool isCloud() const { return cloudSession.has_value(); }
    bool isRemote() const { return hostType == HostType::Remote; }
    bool usesTransientPsnRegistration() const { return isRemote() && !isPS5(); }
    bool takeTransientPs4NicknameMismatch() { return transientPs4NicknameMismatch.exchange(false); }
    bool isManual() const { return hostType == HostType::Manual; }
    bool isAuto() const { return hostType == HostType::Auto; }
    HostType getHostType() const { return hostType; }
    void setHostType(HostType type) { hostType = type; }
    bool needsLink() const { return needsLinking; }
    void setNeedsLink(bool value) { needsLinking = value; }
    bool isInConfig() const { return inConfig; }
    void setInConfig(bool value) { inConfig = value; }

    // Console state helpers
    bool isStandby() const { return state == CHIAKI_DISCOVERY_HOST_STATE_STANDBY; }
    bool isAwake() const { return state == CHIAKI_DISCOVERY_HOST_STATE_READY; }

    // Console PIN for auto-login
    std::string getConsolePIN() const { return consolePIN; }
    void setConsolePIN(const std::string& pin) { consolePIN = pin; }

    // Remote DUID for matching remote hosts
    std::string getRemoteDuid() const { return remoteDuid; }
    void setRemoteDuid(const std::string& duid) { remoteDuid = duid; }

    // Per-host haptic setting (-1=inherit, 0=disabled, 1=weak, 2=strong)
    int getHapticRaw() const { return haptic; }
    void setHapticRaw(int value) { haptic = value; }
    void setCloudSessionConfig(const CloudSessionConfig& config) { cloudSession = config; }
    void clearCloudSessionConfig() { cloudSession.reset(); }
    const CloudSessionConfig* getCloudSessionConfig() const { return cloudSession ? &*cloudSession : nullptr; }

    // Send login PIN to active session
    void setLoginPIN(const std::string& pin);

    // Get display-friendly state string
    std::string getStateString() const;
    std::string getTargetString() const;

    // Video resolution helpers
    bool getVideoResolution(int* width, int* height) const;

    // Session management
    int initSession(Session* streamSession);
    int initSessionWithHolepunch(Session* streamSession, ChiakiHolepunchSession holepunch);
    void startSession();
    void stopSession();
    bool isSessionSocketHealthy() const;
    void gotoBed();
    int finiSession();
    int finiRegist();
    void sendFeedbackState();
    void sendFeedbackStateCouch();

    static constexpr uint8_t kCouchDualShock4TakionType = 2;
    void couchAddPlayer(uint8_t slot, uint8_t deviceType = kCouchDualShock4TakionType);
    void couchRemovePlayer(uint8_t slot);
    void couchSendPasscode(uint8_t slot, const std::string& passcode);
    void couchRetryPlayer(uint8_t slot);
    std::vector<CouchProfileChoice> couchProfileChoices(uint8_t slot) const;
    bool couchSelectProfile(uint8_t slot, int64_t profileId);
    void couchHandlePadDropped(uint8_t slot);
    bool isCouchMode() const { return couchMode.load(std::memory_order_acquire); }
    const std::string& couchAccountLabel(uint8_t slot) const {
        static const std::string empty;
        return slot < CHIAKI_COUCH_MAX_PADS ? couchAccountLabels[slot] : empty;
    }
    bool isCouchPadConfirmed(uint8_t slot) const { return couchPadState(slot) == CouchPadState::Confirmed; }
    CouchPadState couchPadState(uint8_t slot) const {
        return slot < CHIAKI_COUCH_MAX_PADS ? couchPadStates[slot] : CouchPadState::Idle;
    }
    uint8_t couchPadFailure(uint8_t slot) const {
        return slot < CHIAKI_COUCH_MAX_PADS ? couchPadFailStatus[slot] : 0;
    }
    void couchTickJoinTimeouts();
    bool isCouchPadSlotAvailable() const {
        if (!sessionInit)
            return false;
        for (uint8_t slot = 1; slot < CHIAKI_COUCH_MAX_PADS; slot++) {
            if (couchPadStates[slot] == CouchPadState::Idle)
                return true;
        }
        return false;
    }
    void setInputSuppressed(bool suppressed);

    // Holepunch connection for remote play
    ChiakiErrorCode initHolepunchSession();
    ChiakiErrorCode connectHolepunch();
    void cancelHolepunch();
    void cleanupHolepunch();
    ChiakiHolepunchSession getHolepunchSession() const { return holepunchSession; }

    // Wakeup and registration
    int wakeup();
    int registerHost(int pin);
    int registerHostAuto();
    bool canAutoRegister() const;
    bool resolveRemoteDuid();
    void applyRegistrationData(ChiakiRegisteredHost* regHost);
    void copyRegistrationFrom(const Host* other);
    const Registration* activeRegistration() const;
    void upsertRegistration(const Registration& reg);
    void removeRegistrationsForProfile(int64_t profileId);
    int64_t getConsoleId() const { return consoleId; }

    // Event callbacks from chiaki
    void connectionEventCallback(ChiakiEvent* event);
    void registCallback(ChiakiRegistEvent* event);

    // Callback setters
    void setOnConnected(std::function<void()> callback) { onConnected = std::move(callback); }
    void setOnLoginPinRequest(std::function<void(bool)> callback) { onLoginPinRequest = std::move(callback); }
    void setOnQuit(std::function<void(ChiakiQuitEvent*)> callback) { onQuit = std::move(callback); }
    void setOnRumble(std::function<void(uint8_t, uint8_t, uint8_t)> callback) { onRumble = std::move(callback); }
    void setOnReadController(std::function<void(ChiakiControllerState*, std::map<uint32_t, int8_t>*)> callback) {
        onReadController = std::move(callback);
    }
    void setOnReadCouchSecondary(std::function<void(ChiakiControllerState*, uint8_t*)> callback) {
        onReadCouchSecondary = std::move(callback);
    }
    void setCouchMode(bool on) { couchMode.store(on, std::memory_order_release); }
    void setOnRegistCanceled(std::function<void()> callback) { onRegistCanceled = std::move(callback); }
    void setOnRegistFailed(std::function<void()> callback) { onRegistFailed = std::move(callback); }
    void setOnRegistSuccess(std::function<void()> callback) { onRegistSuccess = std::move(callback); }
    void setOnRegistStage(std::function<void(int, int)> callback) { onRegistStage = std::move(callback); }
    void setOnHolepunchPhase(std::function<void(HolepunchPhase)> callback) { onHolepunchPhase = std::move(callback); }
    void setOnMotionReset(std::function<void()> callback) { onMotionReset = std::move(callback); }
    void setOnTriggerEffects(std::function<void(const ChiakiTriggerEffectsEvent*)> callback) {
        onTriggerEffects = std::move(callback);
    }
    void setOnEffectIntensity(std::function<void(uint8_t, uint8_t)> callback) {
        onEffectIntensity = std::move(callback);
    }
    void setOnPadConfirmed(std::function<void(uint8_t, uint8_t, uint8_t, uint8_t)> callback) {
        onPadConfirmed = std::move(callback);
    }
    void setOnPadPasscodeRequest(std::function<void(uint8_t, bool)> callback) {
        onPadPasscodeRequest = std::move(callback);
    }
    void setOnPadJoinFailed(std::function<void(uint8_t, uint8_t)> callback) {
        onPadJoinFailed = std::move(callback);
    }
    void setOnLedColor(std::function<void(uint8_t, uint8_t, uint8_t)> callback) {
        onLedColor = std::move(callback);
    }

    uint8_t hapticIntensity = 1;
    uint8_t triggerIntensity = 1;
};

#endif // AKIRA_HOST_HPP
