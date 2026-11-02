#ifndef AKIRA_DISCOVERY_MANAGER_HPP
#define AKIRA_DISCOVERY_MANAGER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <borealis.hpp>

#include <libchiaking/consoles.hpp>

#include "background_work.hpp"
#include "host.hpp"
#include "psn/accounts.hpp"

class SettingsManager;

class DiscoveryManager {
public:
    struct RemoteRefreshResult {
        bool success = false;
        std::string message;
    };

    using HostDiscoveredCallback = std::function<void(Host*)>;
    using RemoteRefreshCallback = std::function<void(const RemoteRefreshResult&)>;
    using HostsChangedCallback = std::function<void()>;

    struct SweepStatus {
        bool serviceRunning = false;
        bool sweepActive = false;
        std::vector<std::string> subnets;
        std::string currentTarget;
    };

    static DiscoveryManager* getInstance();

    static void appendDiscoveryLog(const std::string& line);
    static std::vector<std::string> getDiscoveryLogSnapshot();
    static uint64_t getDiscoveryLogVersion();
    static void clearDiscoveryLog();

    void setServiceEnabled(bool enable);
    bool isServiceEnabled() const { return serviceEnabled; }
    void restartSearch();

    std::string getLocalSubnetCidr();

    void setOnHostDiscovered(HostDiscoveredCallback callback) { onHostDiscovered = std::move(callback); }
    void setOnHostsChanged(HostsChangedCallback callback) { onHostsChanged = std::move(callback); }

    SweepStatus getSweepStatus();

    void refreshRemoteDevices(RemoteRefreshCallback onComplete = nullptr, bool userInitiated = false);
    psn::ActionStatus getRemoteRefreshStatus() const;

private:
    struct SeenNetwork {
        std::vector<std::shared_ptr<libchiaking::consoles::Console>> nearby;
        std::size_t targets = 0;
        std::size_t visited = 0;
        std::vector<std::string> unicastRanges;
    };

    struct SeenAccount {
        std::string name;
        std::string deviceUid;
        libchiaking::consoles::Generation generation;
        bool remotePlayEnabled = false;
    };

    class Lookout final : public libchiaking::consoles::NetworkWatcher {
    public:
        Lookout(DiscoveryManager& manager, uint64_t generation) : manager(manager), generation(generation) {}
        void on_view(const libchiaking::consoles::NetworkView& view) override;

    private:
        DiscoveryManager& manager;
        uint64_t generation;
    };

    static constexpr int PSN_REMOTE_COOLDOWN_S = 15;
    static constexpr int PSN_FAILED_COOLDOWN_S = 5;

    DiscoveryManager();

    void startSearch();
    void stopSearch();
    void stopRemoteRefresh();
    void ensureFocusSubscription();
    void followBackgroundWork(bool paused);
    void linkSavedConsoles();
    auto searchRanges() -> libchiaking::consoles::SearchRanges;

    void applyView(uint64_t generation, const SeenNetwork& seen);
    Host* hostFor(const libchiaking::consoles::Console& console);
    void reconcile(const SeenNetwork& seen);

    void runRemoteRefresh(const libchiaking::platform::StopToken& stop, uint64_t generation);
    void applyAccountConsoles(uint64_t generation, const std::vector<SeenAccount>& seen);
    void processRemoteDevice(const SeenAccount& device);
    void processMainPs4Remote();
    void finishRemoteRefresh(uint64_t generation, const RemoteRefreshResult& result);

    SettingsManager* settings = nullptr;
    bool serviceEnabled = false;

    brls::Event<bool>::Subscription focusSubscription;
    bool focusSubscribed = false;
    bool resumeAfterPause = false;
    akira::BackgroundWork::Subscription backgroundSubscription;

    HostDiscoveredCallback onHostDiscovered;
    HostsChangedCallback onHostsChanged;

    uint64_t searchGeneration = 0;
    libchiaking::platform::Thread searchThread;
    mutable std::mutex sweepMutex;
    SweepStatus sweep;

    mutable std::mutex remoteRefreshMutex;
    uint64_t remoteGeneration = 0;
    uint64_t remoteCancelledThrough = 0;
    bool remoteRefreshInFlight = false;
    bool remoteRefreshUserRequested = false;
    std::vector<RemoteRefreshCallback> remoteRefreshWaiters;
    uint64_t remoteRefreshReadyAtMicroseconds = 0;
    libchiaking::platform::Thread remoteThread;
};

#endif // AKIRA_DISCOVERY_MANAGER_HPP
