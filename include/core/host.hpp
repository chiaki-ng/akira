#ifndef AKIRA_HOST_HPP
#define AKIRA_HOST_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <libchiaking/consoles.hpp>

#include "cloud/cloud_launch.hpp"
#include "settings_manager.hpp"
#include "registration.hpp"

// Forward declarations
class SettingsManager;

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

class Host {
    friend class SettingsManager;
    friend class DiscoveryManager;

public:
    using Firmware = libchiaking::consoles::Firmware;
    using HardwareId = libchiaking::consoles::HardwareId;
    using Presence = libchiaking::consoles::Presence;
    using WakeRefusal = libchiaking::consoles::WakeRefusal;

    enum class PairingOutcome : uint8_t {
        Paired,
        Refused,
        Failed,
        Canceled
    };

    using PairingCallback = std::function<void(PairingOutcome)>;
    using PairingStageCallback = std::function<void(int index, int total)>;

private:
    class InternetPairingWatcher final : public libchiaking::consoles::PairingWatcher {
    public:
        InternetPairingWatcher(std::weak_ptr<std::atomic<uint64_t>> alive, uint64_t generation,
                               PairingStageCallback stage);
        void on_stage(libchiaking::consoles::InternetPairingStage stage) override;

    private:
        std::weak_ptr<std::atomic<uint64_t>> alive;
        uint64_t generation;
        PairingStageCallback stage;
    };

    static constexpr int internetPairingStages = 4;

    SettingsManager* settings = nullptr;

    int haptic = -1;  // -1=inherit from global, 0=disabled, 1=weak, 2=strong

    std::string consolePIN;  // 4-digit login PIN for auto-login (optional)

    // Host identification
    std::string hostName;
    std::optional<HardwareId> hardwareId;
    std::string hostAddr;
    std::string serverNickname;

    // Remote connection info
    std::string remoteDuid;  // DUID for matching remote hosts

    // Target console type
    std::optional<Firmware> firmware;
    Presence presence = Presence::unknown;

    int64_t consoleId = 0;
    std::vector<Registration> legacyRegistrations;
    std::shared_ptr<libchiaking::consoles::Console> console;

    bool discovered = false;
    bool psnRemotePlayDisabled = false;
    bool needsLinking = false;
    bool inConfig = false;
    HostType hostType = HostType::Discovered;

    std::optional<cloud::CloudLaunch> cloudLaunchRequest;

    std::shared_ptr<std::atomic<uint64_t>> pairingGeneration = std::make_shared<std::atomic<uint64_t>>(0);
    libchiaking::platform::Thread pairingThread;

    void finishPairing(PairingOutcome outcome, std::shared_ptr<libchiaking::consoles::Console> paired,
                       const PairingCallback& done);
    static void postStage(const std::weak_ptr<std::atomic<uint64_t>>& alive, uint64_t generation,
                          const PairingStageCallback& stage, int index);

public:
    explicit Host(const std::string& name);
    ~Host();

    // Host info getters
    std::string getHostName() const { return hostName; }
    std::string getHostAddr() const;
    std::string getHostId() const { return hardwareId ? hardwareId->text() : std::string{}; }
    std::optional<HardwareId> getHardwareId() const { return hardwareId; }
    std::optional<Firmware> getFirmware() const;
    Presence getPresence() const { return presence; }
    const std::shared_ptr<libchiaking::consoles::Console>& getConsole() const { return console; }

    // Host info setters
    void setHostAddr(const std::string& addr) { hostAddr = addr; }
    void setFirmware(std::optional<Firmware> value) { firmware = value; }
    void setPresence(Presence value) { presence = value; }
    void linkConsole(std::shared_ptr<libchiaking::consoles::Console> linked);

    // Status checks
    bool isDiscovered() const { return discovered; }
    bool isRegistered() const { return hasRpKey(); }
    bool hasRpKey() const;
    bool isReady() const { return presence == Presence::ready; }
    bool isPS5() const;
    bool needsAccountId() const;
    bool isPsnRemotePlayDisabled() const { return psnRemotePlayDisabled; }
    void setPsnRemotePlayDisabled(bool v) { psnRemotePlayDisabled = v; }
    bool isCloud() const { return cloudLaunchRequest.has_value(); }
    bool isRemote() const { return hostType == HostType::Remote; }
    bool usesInternetPairing() const { return isRemote() && !isPS5(); }
    bool isManual() const { return hostType == HostType::Manual; }
    bool isAuto() const { return hostType == HostType::Auto; }
    HostType getHostType() const { return hostType; }
    void setHostType(HostType type) { hostType = type; }
    bool needsLink() const { return needsLinking; }
    void setNeedsLink(bool value) { needsLinking = value; }
    bool isInConfig() const { return inConfig; }
    void setInConfig(bool value) { inConfig = value; }

    // Console state helpers
    bool isStandby() const { return presence == Presence::rest_mode; }
    bool isAwake() const { return presence == Presence::ready; }

    // Console PIN for auto-login
    std::string getConsolePIN() const { return consolePIN; }
    void setConsolePIN(const std::string& pin) { consolePIN = pin; }

    // Remote DUID for matching remote hosts
    std::string getRemoteDuid() const { return remoteDuid; }
    void setRemoteDuid(const std::string& duid) { remoteDuid = duid; }

    // Per-host haptic setting (-1=inherit, 0=disabled, 1=weak, 2=strong)
    int getHapticRaw() const { return haptic; }
    void setHapticRaw(int value) { haptic = value; }
    void setCloudLaunch(cloud::CloudLaunch launch) { cloudLaunchRequest.emplace(std::move(launch)); }
    const cloud::CloudLaunch* cloudLaunch() const { return cloudLaunchRequest ? &*cloudLaunchRequest : nullptr; }

    // Get display-friendly state string
    std::string getStateString() const;
    std::string getTargetString() const;

    [[nodiscard]] libchiaking::Expected<void, WakeRefusal> wake();
    libchiaking::consoles::HomeNetwork homeNetwork() const;
    [[nodiscard]] HostRegisterError registerHost(int pin, PairingCallback done);
    [[nodiscard]] HostRegisterError registerOverInternet(PairingStageCallback stage, PairingCallback done);
    bool canPairOverInternet() const;
    void cancelRegistration();
    void abandonRegistration();
    void releaseConsole();

    void copyRegistrationFrom(const Host* other);
    bool hasLegacyRegistration(int64_t profileId) const;
    void removeRegistrationsForProfile(int64_t profileId);
    int64_t getConsoleId() const { return consoleId; }
};

#endif // AKIRA_HOST_HPP
