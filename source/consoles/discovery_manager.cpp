#include "consoles/discovery_manager.hpp"
#include "play/chiaki_core.hpp"
#include "accounts/account_settings.hpp"
#include "network/network_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"
#include "network/wireguard_manager.hpp"
#include "log/log.hpp"
#include "app/crash_report.hpp"

#include <ctime>
#include <format>

#include <algorithm>
#include <utility>
#include <atomic>
#include <cstdint>
#include <deque>
#include <vector>
#include <switch.h>

#include <arpa/inet.h>
#include <sys/socket.h>

namespace akira::consoles {

static std::mutex g_discoveryLogMutex;
static std::deque<std::string> g_discoveryLogLines;
static std::atomic<uint64_t> g_discoveryLogVersion{0};
static constexpr size_t DISCOVERY_LOG_MAX_LINES = 300;

void DiscoveryManager::appendDiscoveryLog(const std::string& line)
{
    std::lock_guard<std::mutex> lock(g_discoveryLogMutex);
    g_discoveryLogLines.push_back(line);
    while (g_discoveryLogLines.size() > DISCOVERY_LOG_MAX_LINES)
        g_discoveryLogLines.pop_front();
    g_discoveryLogVersion.fetch_add(1, std::memory_order_relaxed);
}

uint64_t DiscoveryManager::getDiscoveryLogVersion()
{
    return g_discoveryLogVersion.load(std::memory_order_relaxed);
}

std::vector<std::string> DiscoveryManager::getDiscoveryLogSnapshot()
{
    std::lock_guard<std::mutex> lock(g_discoveryLogMutex);
    return std::vector<std::string>(g_discoveryLogLines.begin(), g_discoveryLogLines.end());
}

void DiscoveryManager::clearDiscoveryLog()
{
    std::lock_guard<std::mutex> lock(g_discoveryLogMutex);
    g_discoveryLogLines.clear();
    g_discoveryLogVersion.fetch_add(1, std::memory_order_relaxed);
}


namespace consoles = libchiaking::consoles;
namespace platform = libchiaking::platform;

DiscoveryManager::DiscoveryManager(HostCatalog& hostCatalog, settings::SettingsStore& settingsStore,
                                   const network::NetworkSettings& networkSettings,
                                   const settings::DeveloperSettings& developerSettings,
                                   const accounts::AccountSettings& accountSettings, network::WireGuardManager& wireGuard,
                                   app::UiDispatcher& dispatcher, app::UiWindow& window, play::ChiakiCore& core)
    : hostCatalog(hostCatalog), settingsStore(settingsStore), networkSettings(networkSettings),
      developerSettings(developerSettings), accountSettings(accountSettings), wireGuard(wireGuard),
      dispatcher(dispatcher), window(window), core(core)
{
    backgroundSubscription = app::BackgroundWork::subscribe([this](bool paused) { followBackgroundWork(paused); });
}

void DiscoveryManager::followBackgroundWork(bool paused)
{
    if (paused)
    {
        const bool wasEnabled = serviceEnabled;
        if (wasEnabled)
        {
            log::info("Discovery: paused for the stream");
            setServiceEnabled(false);
        }
        resumeAfterPause = wasEnabled;
        return;
    }
    if (std::exchange(resumeAfterPause, false))
    {
        log::info("Discovery: resumed after the stream");
        setServiceEnabled(true);
    }
}

void DiscoveryManager::ensureFocusSubscription()
{
    if (focusWatch)
        return;

    focusWatch = window.watchFocus([this](bool focused) {
        if (!focused || !serviceEnabled)
            return;

        log::info("Discovery: applet back in focus, restarting the search on fresh sockets");
        setServiceEnabled(false);
        setServiceEnabled(true);
    });
}

void DiscoveryManager::setServiceEnabled(bool enable)
{
    log::info("DiscoveryManager::setServiceEnabled({})", enable);

    if (serviceEnabled == enable)
    {
        log::info("Discovery service already in requested state");
        return;
    }

    if (enable && app::BackgroundWork::paused())
    {
        log::info("Discovery: held until the stream ends");
        resumeAfterPause = true;
        return;
    }

    if (!enable)
    {
        resumeAfterPause = false;
        serviceEnabled = false;
        stopSearch();
        stopRemoteRefresh();
        return;
    }

    if (!core.opened())
    {
        log::error("Discovery: the core is not open, nothing to search with");
        return;
    }

    serviceEnabled = true;
    app::CrashReport::stage("discovery link saved");
    linkSavedConsoles();
    app::CrashReport::stage("discovery search");
    startSearch();
    app::CrashReport::stage("discovery focus");
    ensureFocusSubscription();
}

void DiscoveryManager::linkSavedConsoles()
{
    auto& known = core.chiaki().consoles();
    auto* hostsMap = &hostCatalog.entries();
    bool changed = false;

    for (auto& [name, host] : *hostsMap)
    {
        if (!host || host->console || !host->hardwareId || host->isRemote())
            continue;
        if (auto console = known.find(*host->hardwareId))
        {
            host->linkConsole(std::move(console));
            changed = true;
        }
    }

    app::CrashReport::stage("link hosts matched");
    for (const auto& console : known.everything())
    {
        if (!console->is_saved())
            continue;
        if (hostFor(*console))
            continue;
        app::CrashReport::stage("link host create");
        Host* host = hostCatalog.getOrCreate(console->name());
        host->setHostType(HostType::Auto);
        host->linkConsole(console);
        changed = true;
    }

    app::CrashReport::stage(changed ? "link save" : "link unchanged");
    if (changed)
        settingsStore.save();
    app::CrashReport::stage("link done");
}

auto DiscoveryManager::searchRanges() -> consoles::SearchRanges
{
    std::vector<std::string> cidrs;
    const std::string local = getLocalSubnetCidr();
    const auto localSubnet = consoles::Subnet::parse(local);

    const std::string configured = networkSettings.discoverySubnets();
    size_t start = 0;
    while (start < configured.size())
    {
        const size_t end = configured.find_first_of(", \t\n", start);
        std::string token = configured.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? configured.size() : end + 1;
        if (token.empty())
            continue;
        const auto subnet = consoles::Subnet::parse(token);
        if (!subnet)
        {
            log::warning("Discovery: ignoring subnet {}", token);
            continue;
        }
        if (localSubnet && *subnet == *localSubnet)
        {
            log::info("Discovery: subnet {} is local, covered by broadcast", token);
            continue;
        }
        cidrs.push_back(subnet->text());
    }

    std::vector<std::string> tunnelled;
    for (auto& [name, host] : hostCatalog.entries())
    {
        if (!host || !host->isManual())
            continue;
        const auto address = platform::parse_ip_address(host->getHostAddr());
        if (!address || !address->is_ipv4())
            continue;
        const std::string text = platform::to_string(*address);
        cidrs.push_back(text + "/32");
        if (wireGuard.tunnels(text))
        {
            tunnelled.push_back(text + "/32");
            log::info("Discovery: added manual unicast target {} through the WireGuard tunnel", text);
        }
        else
        {
            log::info("Discovery: added manual unicast target {}", text);
        }
    }

    for (const auto& address : wireGuard.allowedHosts())
    {
        const std::string text = platform::to_string(address);
        cidrs.push_back(text + "/32");
        tunnelled.push_back(text + "/32");
        log::info("Discovery: added WireGuard host {} through the tunnel", text);
    }

    auto ranges = consoles::SearchRanges::parse(cidrs, true);
    if (!ranges)
    {
        log::error("Discovery: the extra subnets are not searchable (reason {}), broadcasting only",
            static_cast<int>(ranges.error()));
        return consoles::SearchRanges{};
    }
    if (tunnelled.empty())
        return std::move(ranges).value();

    const auto tunnel = wireGuard.discoveryNetwork();
    if (!tunnel.network())
    {
        log::error("Discovery: WireGuard has no tunnel network; its hosts are probed on the local network");
        return std::move(ranges).value();
    }
    auto searched = std::move(ranges).value();
    const auto subnets = searched.subnets();
    for (const auto& subnet : subnets)
    {
        if (std::ranges::find(tunnelled, subnet.text()) == tunnelled.end())
            continue;
        auto routed = searched.over(subnet, tunnel);
        if (!routed)
        {
            log::error("Discovery: {} cannot be probed through the tunnel (reason {})", subnet.text(),
                static_cast<int>(routed.error()));
            continue;
        }
        searched = std::move(routed).value();
    }
    return searched;
}

void DiscoveryManager::restartSearch()
{
    if (!serviceEnabled)
        return;
    stopSearch();
    startSearch();
}

void DiscoveryManager::startSearch()
{
    const uint64_t generation = ++searchGeneration;
    auto ranges = searchRanges();
    {
        std::lock_guard<std::mutex> lock(sweepMutex);
        sweep = SweepStatus{};
        sweep.serviceRunning = true;
        sweep.sweepActive = !ranges.subnets().empty();
        for (const auto& subnet : ranges.subnets())
            sweep.subnets.push_back(subnet.text());
    }

    auto started = platform::Thread::start(core.threading(), "discovery", platform::ThreadRole::discovery,
        [this, ranges = std::move(ranges), generation](platform::StopToken stop) {
            Lookout lookout{*this, generation};
            const auto searched = core.chiaki().consoles().find_on_my_network(ranges, lookout, stop);
            if (!searched)
                log::error("Discovery: the search ended (reason {})", static_cast<int>(searched.error()));
        });
    if (!started)
    {
        log::error("Discovery: the search thread could not start");
        serviceEnabled = false;
        std::lock_guard<std::mutex> lock(sweepMutex);
        sweep.serviceRunning = false;
        return;
    }
    searchThread = std::move(started).value();
    log::info("Discovery: searching");
}

void DiscoveryManager::stopSearch()
{
    ++searchGeneration;
    searchThread = platform::Thread{};
    std::lock_guard<std::mutex> lock(sweepMutex);
    sweep = SweepStatus{};
}

void DiscoveryManager::Lookout::on_view(const consoles::NetworkView& view)
{
    SeenNetwork seen;
    seen.nearby = view.nearby;
    seen.targets = view.targets;
    seen.visited = view.visited;
    for (const auto& subnet : view.unicast_ranges)
        seen.unicastRanges.push_back(subnet.text());

    DiscoveryManager& owner = manager;
    const uint64_t viewGeneration = generation;
    owner.dispatcher.post([&owner, viewGeneration, seen = std::move(seen)]() {
        owner.applyView(viewGeneration, seen);
    });
}

void DiscoveryManager::applyView(uint64_t generation, const SeenNetwork& seen)
{
    if (generation != searchGeneration)
        return;

    {
        std::lock_guard<std::mutex> lock(sweepMutex);
        sweep.sweepActive = !seen.unicastRanges.empty();
        sweep.subnets = seen.unicastRanges;
        sweep.currentTarget = seen.targets > 0 ? std::format("{}/{}", seen.visited, seen.targets) : std::string{};
    }

    const bool logging = developerSettings.discoveryLog();
    bool adopted = false;
    for (const auto& console : seen.nearby)
    {
        Host* host = hostFor(*console);
        if (!host)
        {
            host = hostCatalog.getOrCreate(console->name());
            host->setHostType(HostType::Auto);
        }

        host->linkConsole(console);
        host->presence = console->presence();
        host->discovered = true;
        adopted = hostCatalog.adoptLegacyRegistrations(*host) || adopted;

        if (logging)
        {
            appendDiscoveryLog(std::format("{} {} at {} ({})", console->hardware_id().text(), console->name(),
                host->getHostAddr(), host->getStateString()));
        }

        if (onHostDiscovered)
            onHostDiscovered(host);
    }

    if (adopted)
    {
        auto remembered = core.chiaki().consoles().remember_saved();
        if (remembered)
            log::info("Discovery: adopted saved registrations, {} console(s) remembered",
                remembered->consoles.size());
        else
            log::error("Discovery: adopted registrations could not be remembered");
    }

    reconcile(seen);
}

Host* DiscoveryManager::hostFor(const consoles::Console& console)
{
    auto* hostsMap = &hostCatalog.entries();
    const auto hardwareId = console.hardware_id();
    for (auto& [name, host] : *hostsMap)
    {
        if (host && !host->isRemote() && host->hardwareId == hardwareId)
            return host.get();
    }

    const std::string name = console.name();
    if (auto it = hostsMap->find(name); it != hostsMap->end() && it->second && !it->second->hardwareId)
        return it->second.get();

    if (const auto address = console.last_address())
    {
        for (auto& [hostName, host] : *hostsMap)
        {
            if (host && !host->isRemote() && !host->hardwareId && host->getHostAddr() == address->text())
                return host.get();
        }
    }
    return nullptr;
}

void DiscoveryManager::reconcile(const SeenNetwork& seen)
{
    bool changed = false;
    for (auto& [name, host] : hostCatalog.entries())
    {
        if (!host || !host->hardwareId || host->isRemote())
            continue;

        const bool present = std::ranges::any_of(seen.nearby, [&](const auto& console) {
            return console->hardware_id() == *host->hardwareId;
        });

        if (!present && host->presence != Host::Presence::unknown)
        {
            host->presence = Host::Presence::unknown;
            changed = true;
        }

        if (host->hasRpKey() || host->isManual() || !host->isDiscovered())
            continue;

        if (!present)
        {
            host->discovered = false;
            changed = true;
        }
    }

    if (changed && onHostsChanged)
        onHostsChanged();
}

DiscoveryManager::SweepStatus DiscoveryManager::getSweepStatus()
{
    std::lock_guard<std::mutex> lock(sweepMutex);
    SweepStatus status = sweep;
    status.serviceRunning = serviceEnabled;
    return status;
}

accounts::ActionStatus DiscoveryManager::getRemoteRefreshStatus() const
{
    std::lock_guard<std::mutex> lock(remoteRefreshMutex);
    if (remoteRefreshInFlight)
        return accounts::ActionStatus{accounts::ActionState::Busy, 0};
    const uint64_t now = core.clock().now_microseconds();
    if (now < remoteRefreshReadyAtMicroseconds)
        return accounts::ActionStatus{accounts::ActionState::CoolingDown,
            static_cast<int>((remoteRefreshReadyAtMicroseconds - now + 999'999) / 1'000'000)};
    return accounts::ActionStatus{};
}

void DiscoveryManager::refreshRemoteDevices(RemoteRefreshCallback onComplete, bool userInitiated)
{
    log::info("DiscoveryManager::refreshRemoteDevices() called");

    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(remoteRefreshMutex);

        if (onComplete)
            remoteRefreshWaiters.push_back(std::move(onComplete));

        if (userInitiated)
            remoteRefreshUserRequested = true;

        if (remoteRefreshInFlight)
        {
            log::info("Remote device refresh already running, joining in-flight request");
            return;
        }

        remoteRefreshInFlight = true;
        generation = ++remoteGeneration;
    }

    if (!core.opened())
    {
        finishRemoteRefresh(generation, {false, "core unavailable"});
        return;
    }

    remoteThread = platform::Thread{};
    auto started = platform::Thread::start(core.threading(), "psn-consoles", platform::ThreadRole::unspecified,
        [this, generation](platform::StopToken stop) { runRemoteRefresh(stop, generation); });
    if (!started)
    {
        finishRemoteRefresh(generation, {false, "thread unavailable"});
        return;
    }
    remoteThread = std::move(started).value();
}

void DiscoveryManager::runRemoteRefresh(const platform::StopToken& stop, uint64_t generation)
{
    auto* accounts = core.accounts();
    if (accountSettings.isLegacyProfileActive() || !accounts)
    {
        log::info("Legacy profile: skipping PSN remote device discovery");
        finishRemoteRefresh(generation, {true, ""});
        return;
    }

    if (!accounts->staySignedIn(stop))
    {
        log::warning("Cannot discover remote devices: the PSN sign-in is not usable");
        finishRemoteRefresh(generation, {false, "PSN sign-in expired"});
        return;
    }

    const auto user = accounts->activeUser();
    if (!user)
    {
        finishRemoteRefresh(generation, {false, "no PSN account"});
        return;
    }

    log::info("Querying PSN for remote devices...");
    auto listed = core.chiaki().consoles().consoles_on_account(*user, consoles::Generation::ps5, stop);
    if (!listed)
    {
        log::error("Failed to list PS5 devices (reason {})", static_cast<int>(listed.error().reason));
        finishRemoteRefresh(generation, {false, std::format("PSN refused the console list ({})",
            listed.error().http_status)});
        return;
    }

    std::vector<SeenAccount> seen;
    for (const auto& console : *listed)
        seen.push_back({console.name(), console.device_uid().text(), console.generation(), console.remote_play_enabled()});
    log::info("Found {} PS5 remote device(s)", seen.size());

    dispatcher.post([this, generation, seen = std::move(seen)]() { applyAccountConsoles(generation, seen); });
    finishRemoteRefresh(generation, {true, ""});
}

void DiscoveryManager::applyAccountConsoles(uint64_t generation, const std::vector<SeenAccount>& seen)
{
    {
        std::lock_guard<std::mutex> lock(remoteRefreshMutex);
        if (generation <= remoteCancelledThrough)
            return;
    }
    for (const SeenAccount& device : seen)
        processRemoteDevice(device);

    /*
     * Sony does not expose PS4s through the device-list endpoint. PS4 PSN
     * remote play always targets the account's main PS4, and the holepunch
     * library resolves its real DUID after the console joins the session.
     * Mirror chiaki-ng's desktop frontend by offering one synthetic remote
     * host whenever this profile has a locally registered PS4.
     */
    processMainPs4Remote();
}

void DiscoveryManager::processMainPs4Remote()
{
    auto* hostsMap = &hostCatalog.entries();
    if (!hostsMap)
        return;

    std::vector<Host*> registeredPs4s;
    for (auto& entry : *hostsMap)
    {
        Host* host = entry.second.get();
        if (host && !host->isRemote() && !host->isCloud() &&
            !host->isPS5() && host->hasRpKey())
        {
            registeredPs4s.push_back(host);
        }
    }

    static const std::string mainPs4RemoteName = std::string(consoles::AccountConsole::main_ps4_name) + " (Remote)";

    auto existing = hostsMap->find(mainPs4RemoteName);
    if (registeredPs4s.empty())
    {
        if (existing != hostsMap->end())
        {
            Host* remote = existing->second.get();
            remote->discovered = false;
            remote->setNeedsLink(false);
            // Remove keys written by older Akira builds. The PS4 PSN flow
            // obtains fresh session-only keys from chiaki-ng instead.
            remote->legacyRegistrations.clear();
            settingsStore.save();
            if (onHostsChanged)
                onHostsChanged();
        }

        log::info("No registered PS4 for the active profile; hiding main PS4 remote host");
        return;
    }

    Host* remote = hostCatalog.getOrCreate(mainPs4RemoteName);
    remote->setHostType(HostType::Remote);
    remote->setFirmware(Host::Firmware::newest(consoles::Generation::ps4));
    remote->setRemoteDuid(consoles::DeviceUid::main_ps4().text());
    remote->setPsnRemotePlayDisabled(false);
    remote->discovered = true;
    remote->presence = Host::Presence::unknown;
    remote->setNeedsLink(false);
    // PS4-over-PSN always transiently registers during session startup.
    // Never copy or persist a local console's Remote Play keys here.
    remote->legacyRegistrations.clear();

    settingsStore.save();

    if (onHostDiscovered)
        onHostDiscovered(remote);
}

void DiscoveryManager::processRemoteDevice(const SeenAccount& device)
{
    log::info("Remote device: name='{}', uid='{}', type={}, remoteplay={}",
        device.name, device.deviceUid,
        device.generation == consoles::Generation::ps5 ? "PS5" : "PS4",
        device.remotePlayEnabled ? "enabled" : "disabled");

    auto* hostsMap = &hostCatalog.entries();
    for (auto& entry : *hostsMap)
    {
        Host* h = entry.second.get();
        if (h && (h->getHostName() == device.name || h->getRemoteDuid() == device.deviceUid))
            h->setPsnRemotePlayDisabled(!device.remotePlayEnabled);
    }

    if (!device.remotePlayEnabled)
    {
        log::info("Skipping device '{}' - remote play not enabled", device.name);
        return;
    }

    Host* localHost = nullptr;
    auto it = hostsMap->find(device.name);
    if (it != hostsMap->end() && it->second->hasRpKey() && !it->second->isRemote())
    {
        localHost = it->second.get();
        if (localHost->getRemoteDuid().empty())
        {
            localHost->setRemoteDuid(device.deviceUid);
            log::info("Updated local host '{}' with remote DUID", device.name);
        }
    }

    Host* host = hostCatalog.getOrCreate(device.name + " (Remote)");
    host->setHostType(HostType::Remote);
    host->discovered = true;
    host->setRemoteDuid(device.deviceUid);
    host->setFirmware(Host::Firmware::newest(device.generation));

    if (localHost)
    {
        host->copyRegistrationFrom(localHost);
        host->setNeedsLink(false);
    }
    else if (!host->hasRpKey())
    {
        host->setNeedsLink(true);
        log::info("Remote device '{}' needs linking to a local host", device.name);
    }
    else
    {
        host->setNeedsLink(false);
    }

    host->presence = Host::Presence::unknown;

    settingsStore.save();

    if (onHostDiscovered)
        onHostDiscovered(host);
}

void DiscoveryManager::finishRemoteRefresh(uint64_t generation, const RemoteRefreshResult& result)
{
    std::vector<RemoteRefreshCallback> waiters;
    {
        std::lock_guard<std::mutex> lock(remoteRefreshMutex);
        if (generation != remoteGeneration)
            return;
        waiters.swap(remoteRefreshWaiters);
        remoteRefreshInFlight = false;

        if (remoteRefreshUserRequested)
        {
            remoteRefreshReadyAtMicroseconds = core.clock().now_microseconds() +
                static_cast<uint64_t>(result.success ? PSN_REMOTE_COOLDOWN_S : PSN_FAILED_COOLDOWN_S) * 1'000'000;
            remoteRefreshUserRequested = false;
        }
    }

    if (waiters.empty())
        return;

    dispatcher.post([waiters = std::move(waiters), result]() {
        for (const auto& waiter : waiters)
            waiter(result);
    });
}

void DiscoveryManager::stopRemoteRefresh()
{
    {
        std::lock_guard<std::mutex> lock(remoteRefreshMutex);
        remoteCancelledThrough = ++remoteGeneration;
        remoteRefreshInFlight = false;
        remoteRefreshUserRequested = false;
        remoteRefreshWaiters.clear();
    }
    remoteThread = platform::Thread{};
}

std::string DiscoveryManager::getLocalSubnetCidr()
{
    uint32_t current_addr = 0;
    uint32_t subnet_mask = 0;

    Result rc = nifmInitialize(NifmServiceType_User);
    if (R_SUCCEEDED(rc))
    {
        nifmGetCurrentIpConfigInfo(&current_addr, &subnet_mask, NULL, NULL, NULL);
        nifmExit();
    }
    else
    {
        return "";
    }

    if (current_addr == 0)
        return "";

    uint32_t network = current_addr & subnet_mask;
    int prefix = __builtin_popcount(subnet_mask);

    struct in_addr netAddr = {};
    netAddr.s_addr = network;
    char buf[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &netAddr, buf, sizeof(buf)))
        return "";

    return std::string(buf) + "/" + std::to_string(prefix);
}

}
