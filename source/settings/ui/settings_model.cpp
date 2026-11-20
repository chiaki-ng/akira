#include "settings/ui/settings_model.hpp"

#include <algorithm>
#include <utility>

#include "app/app_settings.hpp"
#include "app/notifier.hpp"
#include "consoles/discovery_sweep.hpp"
#include "network/custom_dns.hpp"
#include "network/network_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"

namespace akira::settings::ui {

SettingsModel::SettingsModel(Parts parts)
    : held(std::move(parts))
{
}

SettingsModel::~SettingsModel()
{
    *alive = false;
}

void SettingsModel::commit()
{
    held.store.save();
    state.update([](Status& status) { ++status.revision; });
}

void SettingsModel::open(Detail detail, std::size_t pad)
{
    state.update([detail, pad](Status& status) {
        status.detail = detail;
        status.pad = pad;
    });
}

void SettingsModel::close()
{
    if (state.get().benchmarking)
        held.maintenance.stopBenchmark();
    state.write(&Status::benchmarking, false);
    open(Detail::None);
}

void SettingsModel::openDiscoveryLog()
{
    state.write(&Status::discovery, held.maintenance.discoveryLog());
    open(Detail::DiscoveryLog);
}

void SettingsModel::refreshDiscoveryLog()
{
    if (held.maintenance.discoveryLogVersion() == state.get().discovery.version)
        return;
    state.write(&Status::discovery, held.maintenance.discoveryLog());
}

void SettingsModel::clearDiscoveryLog()
{
    held.maintenance.clearDiscoveryLog();
    state.write(&Status::discovery, held.maintenance.discoveryLog());
}

void SettingsModel::runBenchmark()
{
    if (state.get().benchmarking)
        return;
    state.update([](Status& status) {
        status.benchmark.clear();
        status.benchmarking = true;
        status.detail = Detail::Benchmark;
    });
    held.maintenance.runBenchmark(
        [this, guard = alive](std::string line) {
            if (!*guard)
                return;
            state.update([&line](Status& status) {
                status.benchmark.push_back(std::move(line));
                if (status.benchmark.size() > benchmarkLines)
                    status.benchmark.erase(status.benchmark.begin());
            });
        },
        [this, guard = alive] {
            if (*guard)
                state.write(&Status::benchmarking, false);
        });
}

void SettingsModel::scanPads()
{
    std::vector<PadBench::Pad> found = held.pads.scan();
    held.store.save();
    state.write(&Status::pads, std::move(found));
}

auto SettingsModel::split(const std::string& text) -> std::vector<std::string>
{
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find_first_of(", \t\n", start);
        std::string token = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? text.size() : end + 1;
        if (!token.empty())
            out.push_back(std::move(token));
    }
    return out;
}

auto SettingsModel::join(const std::vector<std::string>& subnets) -> std::string
{
    std::string out;
    for (const std::string& subnet : subnets)
    {
        if (!out.empty())
            out += ", ";
        out += subnet;
    }
    return out;
}

auto SettingsModel::subnets() const -> std::vector<std::string>
{
    return split(held.network.discoverySubnets());
}

void SettingsModel::saveSubnets(const std::vector<std::string>& subnets)
{
    held.network.setDiscoverySubnets(join(subnets));
    commit();
    held.maintenance.restartDiscovery();
}

auto SettingsModel::addSubnet(const std::string& text) -> bool
{
    if (text.empty())
        return true;
    if (!consoles::isValidSweepCidr(text))
        return false;
    const std::string normalized = consoles::normalizeSweepCidr(text);
    std::vector<std::string> listed = subnets();
    if (std::ranges::contains(listed, normalized))
        return true;
    listed.push_back(normalized);
    saveSubnets(listed);
    return true;
}

void SettingsModel::removeSubnet(const std::string& subnet)
{
    std::vector<std::string> listed = subnets();
    const auto found = std::ranges::find(listed, subnet);
    if (found == listed.end())
        return;
    listed.erase(found);
    saveSubnets(listed);
}

auto SettingsModel::setDns(const std::string& text) -> bool
{
    auto servers = network::CustomDns::parseServers(text);
    if (!servers)
        return false;
    held.network.setCustomDnsServers(std::move(*servers));
    commit();
    return true;
}

auto SettingsModel::tapVersion() -> bool
{
    const auto now = std::chrono::steady_clock::now();
    if (now - lastTap > tapWindow)
        taps = 0;
    lastTap = now;
    if (++taps < unlockTaps || held.developer.powerUserMenuUnlocked())
        return false;
    taps = 0;
    held.developer.setPowerUserMenuUnlocked(true);
    commit();
    return true;
}

void SettingsModel::ask(Entry entry)
{
    state.update([entry](Status& status) {
        status.entry = entry;
        status.rejected = Entry::None;
    });
}

auto SettingsModel::initial(Entry entry) const -> std::string
{
    switch (entry)
    {
        case Entry::Dns: return network::CustomDns::formatServers(held.network.customDnsServers());
        case Entry::UpdateServer: return held.developer.updateServer();
        case Entry::Subnet:
        case Entry::None: break;
    }
    return {};
}

void SettingsModel::submit(const std::string& text)
{
    const Entry entry = state.get().entry;
    bool accepted = true;
    switch (entry)
    {
        case Entry::Subnet: accepted = addSubnet(text); break;
        case Entry::Dns: accepted = setDns(text); break;
        case Entry::UpdateServer: simulateUpdate(text); break;
        case Entry::None: break;
    }
    state.update([entry, accepted](Status& status) {
        status.entry = Entry::None;
        status.rejected = accepted ? Entry::None : entry;
    });
}

void SettingsModel::dismissEntry()
{
    state.write(&Status::entry, Entry::None);
}

void SettingsModel::dismissRejection()
{
    state.write(&Status::rejected, Entry::None);
}

void SettingsModel::probeTls()
{
    held.maintenance.probeTls([this, guard = alive](std::string report) {
        if (*guard)
            state.write(&Status::tls, std::move(report));
    });
}

void SettingsModel::offer(Maintenance::UpdateCheck check)
{
    state.update([&check](Status& status) {
        status.checking = false;
        status.offered = check.outcome == Maintenance::UpdateCheck::Outcome::Available;
        status.update = std::move(check);
    });
}

void SettingsModel::checkForUpdate()
{
    if (state.get().checking)
        return;
    state.write(&Status::checking, true);
    held.maintenance.checkForUpdate(held.app.updateChannel(), [this, guard = alive](Maintenance::UpdateCheck check) {
        if (*guard)
            offer(std::move(check));
    });
}

void SettingsModel::simulateUpdate(const std::string& server)
{
    if (server.empty())
        return;
    held.developer.setUpdateServer(server);
    commit();
    held.maintenance.simulateUpdate(server, [this, guard = alive](Maintenance::UpdateCheck check) {
        if (*guard)
            offer(std::move(check));
    });
}

void SettingsModel::installUpdate()
{
    state.update([](Status& status) {
        status.offered = false;
        status.install = Maintenance::UpdateProgress{};
    });
    held.maintenance.installUpdate([this, guard = alive](Maintenance::UpdateProgress progress) {
        if (*guard)
            state.write(&Status::install, std::optional<Maintenance::UpdateProgress>(std::move(progress)));
    });
}

void SettingsModel::dismissOffer()
{
    state.write(&Status::offered, false);
}

void SettingsModel::dismissInstall()
{
    const std::optional<Maintenance::UpdateProgress> current = state.get().install;
    if (current && current->stage != Maintenance::UpdateProgress::Stage::Failed
        && current->stage != Maintenance::UpdateProgress::Stage::Installed)
        held.maintenance.cancelUpdate();
    const bool installed = current && current->stage == Maintenance::UpdateProgress::Stage::Installed;
    state.write(&Status::install, std::optional<Maintenance::UpdateProgress>());
    if (installed)
        held.maintenance.quit();
}

}
