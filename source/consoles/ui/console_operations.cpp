#include "consoles/ui/console_operations.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "accounts/account_settings.hpp"
#include "accounts/accounts.hpp"
#include "app/crash_report.hpp"
#include "app/notifier.hpp"
#include "app/services.hpp"
#include "app/strings.hpp"
#include "app/ui_dispatcher.hpp"
#include "consoles/debug_consoles.hpp"
#include "consoles/discovery_gate.hpp"
#include "consoles/discovery_manager.hpp"
#include "consoles/host.hpp"
#include "consoles/host_catalog.hpp"
#include "consoles/remote_lookup.hpp"
#include "log/log.hpp"
#include "play/chiaki_core.hpp"
#include "settings/developer_settings.hpp"
#include "util/http.hpp"
#include "util/http_pool.hpp"

namespace akira::consoles::ui {

ConsoleOperations::ConsoleOperations(app::Services& services, ConsoleShelf& shelf, RecentShelf& recent,
    app::Notifier& notifier, Connect connect, SetUp setUp)
    : services(services)
    , shelf(shelf)
    , recent(recent)
    , notifier(notifier)
    , connect(std::move(connect))
    , setUp(std::move(setUp))
{
}

void ConsoleOperations::setUpAccount()
{
    if (setUp)
        setUp();
}

ConsoleOperations::~ConsoleOperations()
{
    *alive = false;
    static_cast<void>(recentStop.request_stop());
    services.discovery.setOnHostDiscovered(nullptr);
    services.discovery.setOnHostsChanged(nullptr);
}

void ConsoleOperations::follow()
{
    app::CrashReport::stage("arisa discovery gate");
    services.discoveryGate.appeared();
    app::CrashReport::stage("arisa discovery callbacks");
    services.discovery.setOnHostDiscovered([this](Host* found) {
        if (services.discoveryGate.discovered(found != nullptr, true) != DiscoveryGate::Discovered::Update)
            return;
        services.dispatcher.post([this] { refresh(); });
    });
    services.discovery.setOnHostsChanged([this] {
        if (services.discoveryGate.hostsChanged(true))
            services.dispatcher.post([this] { refresh(); });
    });
    app::CrashReport::stage("arisa discovery enable");
    if (!services.discovery.isServiceEnabled())
        services.discovery.setServiceEnabled(true);
    app::CrashReport::stage("arisa shelf refresh");
    refresh();
}

void ConsoleOperations::refresh()
{
    shelf.offerAccount(findsRemote());
    shelf.show(list());
    if (shelf.view().get().adding)
        publishScan();
    refreshRecent();
}

auto ConsoleOperations::recentHost() const -> Host*
{
    for (const auto& [name, entry] : services.hosts.entries())
        if (entry && entry->isPS5() && entry->isRegistered() && entry->getConsole())
            return entry.get();
    return nullptr;
}

void ConsoleOperations::refreshRecent()
{
    Host* target = recentHost();
    accounts::Accounts* accounts = services.core.accounts();
    accounts::Accounts::User user = accounts ? accounts->activeUser() : nullptr;
    if (!target || !user || !user->has_psn_account())
    {
        recentFor.clear();
        recentUntil.reset();
        installed.clear();
        recent.clear();
        return;
    }
    const std::string name = target->getHostName();
    if (name == recentFor && recentUntil && Clock::now() < *recentUntil)
        return;
    recentFor = name;
    recentUntil = Clock::now() + recentFreshFor;
    static_cast<void>(recentStop.request_stop());
    recentStop = libchiaking::platform::StopSource{};
    services.http.submit([this, guard = alive, name, console = target->getConsole(), user = std::move(user),
                             stop = recentStop.token()](HttpSession&) {
        auto titles = console->titles(*user, stop);
        if (!titles)
        {
            log::warning("[CONSOLES] recently played on {} unavailable: reason {}", name,
                static_cast<int>(titles.error().reason));
            return;
        }
        log::info("[CONSOLES] {} installed titles on {}", titles->size(), name);
        services.dispatcher.post([this, guard, name, listed = std::move(*titles)]() mutable {
            if (*guard)
                showRecent(name, std::move(listed));
        });
    });
}

auto ConsoleOperations::played(const Installed& titles) -> std::vector<RecentPlay::Title>
{
    std::vector<RecentPlay::Title> made;
    made.reserve(titles.size());
    for (const libchiaking::consoles::InstalledTitle& title : titles)
    {
        std::optional<RecentPlay::Clock::time_point> used;
        if (title.last_used())
            used = title.last_used()->at;
        made.push_back(RecentPlay::Title{ .titleId = title.title_id(),
            .name = title.name(),
            .coverUrl = title.cover_url(),
            .ps5 = title.platform() == libchiaking::consoles::Generation::ps5,
            .lastUsed = used,
            .recentlyPlayed = title.recently_played() });
    }
    return made;
}

void ConsoleOperations::showRecent(const std::string& catalogName, Installed titles)
{
    if (catalogName != recentFor)
        return;
    Host* target = host(catalogName);
    if (!target)
        return;
    installed = std::move(titles);
    recent.show(catalogName, target->getHostName(), RecentPlay(played(installed)));
}

void ConsoleOperations::launchTitle(const std::string& catalogName, const std::string& titleId)
{
    Host* target = host(catalogName);
    const auto title = std::ranges::find_if(installed,
        [&titleId](const libchiaking::consoles::InstalledTitle& listed) { return listed.title_id() == titleId; });
    accounts::Accounts* accounts = services.core.accounts();
    accounts::Accounts::User user = accounts ? accounts->activeUser() : nullptr;
    if (!target || title == installed.end() || !user || !target->getConsole())
        return;
    log::info("[CONSOLES] starting {} on {}", title->name(), catalogName);
    notifier.notify(services.strings.format("akira/hosts/recent_starting", title->name()));
    services.http.submit([this, guard = alive, catalogName, chosen = *title, console = target->getConsole(),
                             user = std::move(user), stop = recentStop.token()](HttpSession&) {
        auto started = console->start(chosen, *user, nullptr, stop);
        services.dispatcher.post([this, guard, catalogName, name = chosen.name(), started = std::move(started)]() mutable {
            if (*guard)
                titleStarted(catalogName, name, std::move(started));
        });
    });
}

void ConsoleOperations::titleStarted(const std::string& catalogName, const std::string& name,
    libchiaking::Expected<libchiaking::consoles::StartState, libchiaking::consoles::StartFailure> started)
{
    if (!started || *started == libchiaking::consoles::StartState::failed)
    {
        log::warning("[CONSOLES] {} did not start on {}: reason {}", name, catalogName,
            started ? -1 : static_cast<int>(started.error().reason));
        notifier.notify(services.strings.format("akira/hosts/recent_start_failed", name));
        return;
    }
    if (Host* target = host(catalogName))
        play(*target);
}

void ConsoleOperations::streamEnded()
{
    services.discoveryGate.connectionEnded();
    refresh();
}

auto ConsoleOperations::host(const std::string& catalogName) const -> Host*
{
    const auto& entries = services.hosts.entries();
    const auto found = entries.find(catalogName);
    return found == entries.end() ? nullptr : found->second.get();
}

auto ConsoleOperations::actions() const -> ConsoleActions
{
    return ConsoleActions(services.hosts, services.store, services.accounts, services.developer, services.wireGuard,
        services.core);
}

auto ConsoleOperations::list() const -> ConsoleList
{
    std::vector<ConsoleFacts> known = services.hosts.facts();
    if (services.developer.fakeHosts())
        std::ranges::move(DebugConsoles::facts(), std::back_inserter(known));
    return ConsoleList(std::move(known),
        ConsoleList::profileFor(!services.accounts.profiles().empty(), services.accounts.isLegacyProfileActive()));
}

auto ConsoleOperations::identityMissing(const Host& target, bool registering) -> bool
{
    const auto missing = actions().missingIdentity(target);
    if (!missing)
        return false;
    const char* key = nullptr;
    if (*missing == PsnIdentity::Missing::AccountId)
        key = registering ? "akira/hosts/psn_account_id_required_register" : "akira/hosts/psn_account_id_required";
    else
        key = registering ? "akira/hosts/psn_online_id_required_register" : "akira/hosts/psn_online_id_required";
    shelf.ask(ConsoleShelf::Prompt{
        .ask = ConsoleShelf::Ask::Notice, .catalogName = target.getHostName(), .text = services.strings.text(key) });
    return true;
}

void ConsoleOperations::primary(const std::string& catalogName)
{
    Host* target = host(catalogName);
    if (!target)
        return;
    const auto action = list().entry(target->facts()).primaryAction();
    if (!action)
    {
        log::warning("[CONSOLES] no primary action for {}", catalogName);
        return;
    }
    switch (*action)
    {
        case ConsoleEntry::Action::Connect:
        case ConsoleEntry::Action::Wake: play(*target); break;
        case ConsoleEntry::Action::Register: beginRegistration(*target); break;
        case ConsoleEntry::Action::Link: offerLink(*target); break;
    }
}

void ConsoleOperations::play(Host& target)
{
    log::info("[CONSOLES] play on {}", target.getHostName());
    if (target.isPsnRemotePlayDisabled())
        notifier.notify(services.strings.text("akira/connection/fail_rp_disabled"));
    if (identityMissing(target, false))
        return;
    services.discoveryGate.connectionStarted();
    connect(target);
}

void ConsoleOperations::beginRegistration(Host& target)
{
    if (services.registration.registering())
        return;
    if (identityMissing(target, true))
        return;
    if (target.canPairOverInternet())
    {
        shelf.ask(ConsoleShelf::Prompt{ .ask = ConsoleShelf::Ask::RegisterHow, .catalogName = target.getHostName() });
        return;
    }
    askPin(target.getHostName());
}

void ConsoleOperations::offerLink(Host& target)
{
    const std::vector<std::string> targets = list().linkTargets();
    if (targets.empty())
    {
        notifier.notify(services.strings.text("akira/hosts/no_registered_hosts"));
        return;
    }
    shelf.ask(ConsoleShelf::Prompt{
        .ask = ConsoleShelf::Ask::Link, .catalogName = target.getHostName(), .choices = targets });
}

void ConsoleOperations::edit(const std::string& catalogName)
{
    Host* target = host(catalogName);
    if (!target)
        return;
    const ConsoleEntry entry = list().entry(target->facts());
    const std::vector<ConsoleEntry::Option> options = entry.options();
    const auto offers = [&options](ConsoleEntry::Option option) { return std::ranges::contains(options, option); };
    ConsoleShelf::Editing editing{ .catalogName = catalogName,
        .name = entry.displayName(),
        .ps5 = target->isPS5(),
        .values = ConsoleShelf::Edit{ .address = target->getHostAddr(), .pin = target->getConsolePIN() },
        .remote = target->facts().type == HostType::Remote,
        .address = offers(ConsoleEntry::Option::SetAddress) || offers(ConsoleEntry::Option::AddAddress),
        .pin = offers(ConsoleEntry::Option::ConsolePin),
        .deletable = offers(ConsoleEntry::Option::Delete) };
    if (offers(ConsoleEntry::Option::TakionVersion))
    {
        editing.takion = takionLabels(*target);
        editing.values.takion = actions().takionPins(*target).selected();
    }
    shelf.edit(std::move(editing));
}

auto ConsoleOperations::takionLabels(Host& target) -> std::vector<std::string>
{
    const TakionPinChoice choice = actions().takionPins(target);
    const auto configured = choice.configuredVersion();
    const std::string fallback =
        configured ? "v" + std::to_string(*configured) : services.strings.text("akira/settings/dev_takion_version_auto");
    std::vector<std::string> labels{ services.strings.format("akira/hosts/takion_follow_settings", fallback) };
    std::ranges::transform(choice.versions(), std::back_inserter(labels),
        [](std::uint32_t version) { return "v" + std::to_string(version); });
    return labels;
}

void ConsoleOperations::saveEdit(const std::string& catalogName, const ConsoleShelf::Edit& values)
{
    Host* target = host(catalogName);
    const std::optional<ConsoleShelf::Editing> editing = shelf.view().get().editing;
    if (!target || !editing || editing->catalogName != catalogName)
        return;
    const ConsoleShelf::Edit& before = editing->values;
    std::optional<std::string> refused;
    if (editing->address && values.address != before.address)
        refused = saveAddress(*target, values.address);
    if (!refused && editing->pin && values.pin != before.pin)
        refused = saveConsolePin(*target, values.pin);
    if (refused)
    {
        notifier.notify(*refused);
        return;
    }
    if (!editing->takion.empty() && values.takion != before.takion)
        saveTakion(*target, values.takion);
    shelf.edit(std::nullopt);
    notifier.notify(services.strings.text("akira/hosts/edit_saved"));
    refresh();
}

void ConsoleOperations::askDelete(const std::string& catalogName)
{
    Host* target = host(catalogName);
    if (!target)
        return;
    shelf.edit(std::nullopt);
    shelf.ask(ConsoleShelf::Prompt{ .ask = ConsoleShelf::Ask::Delete,
        .catalogName = catalogName,
        .text = services.strings.format("akira/hosts/delete_confirm", target->getHostName()) });
}

void ConsoleOperations::askPin(const std::string& catalogName)
{
    if (services.registration.registering() || !host(catalogName))
        return;
    shelf.ask(ConsoleShelf::Prompt{ .ask = ConsoleShelf::Ask::Pin, .catalogName = catalogName });
}

void ConsoleOperations::registerWithPin(const std::string& catalogName, const std::string& pin)
{
    Host* target = host(catalogName);
    shelf.dismiss();
    if (!target)
        return;
    services.registration.beginPin();
    const HostRegisterError result = actions().registerWithPin(*target, pin, [this, catalogName](Host::PairingOutcome outcome) {
        services.dispatcher.post([this, catalogName, outcome] { pinEnded(catalogName, outcome); });
    });
    if (const auto refusal = services.registration.pinRefused(result))
        notifier.notify(refusalText(*refusal));
}

void ConsoleOperations::pinEnded(const std::string& catalogName, Host::PairingOutcome outcome)
{
    switch (services.registration.pinEnded(outcome))
    {
        case RegisterFlow::Notice::Success:
            log::info("[CONSOLES] registered {}", catalogName);
            notifier.notify(services.strings.text("akira/hosts/registration_success"));
            break;
        case RegisterFlow::Notice::Canceled:
            notifier.notify(services.strings.text("akira/hosts/registration_canceled"));
            break;
        case RegisterFlow::Notice::Failed:
            notifier.notify(services.strings.text("akira/hosts/registration_failed"));
            break;
    }
    refresh();
}

void ConsoleOperations::registerOverInternet(const std::string& catalogName)
{
    Host* target = host(catalogName);
    shelf.dismiss();
    if (!target || !services.registration.beginInternet())
        return;
    internetStage(catalogName, 1, 4);
    const HostRegisterError result = actions().registerOverInternet(*target,
        [this, catalogName](int index, int total) {
            services.dispatcher.post([this, catalogName, index, total] { internetStage(catalogName, index, total); });
        },
        [this, catalogName](Host::PairingOutcome outcome) {
            services.dispatcher.post([this, catalogName, outcome] { internetEnded(catalogName, outcome); });
        });
    if (!services.registration.internetStarted(result))
    {
        shelf.registering(std::nullopt);
        notifier.notify(services.strings.text("akira/hosts/auto_register_fallback"));
        askPin(catalogName);
    }
}

void ConsoleOperations::internetStage(const std::string& catalogName, int index, int total)
{
    if (!services.registration.registering())
        return;
    const char* key = nullptr;
    switch (RegisterFlow::stage(index))
    {
        case RegisterFlow::Stage::Account: key = "akira/hosts/register_stage_account"; break;
        case RegisterFlow::Stage::Console: key = "akira/hosts/register_stage_console"; break;
        case RegisterFlow::Stage::Connect: key = "akira/hosts/register_stage_connect"; break;
        case RegisterFlow::Stage::Finish: key = "akira/hosts/register_stage_finish"; break;
    }
    shelf.registering(ConsoleShelf::Registering{ .catalogName = catalogName,
        .stage = services.strings.format("akira/hosts/register_stage_fmt", index, total, services.strings.text(key)) });
}

void ConsoleOperations::internetEnded(const std::string& catalogName, Host::PairingOutcome outcome)
{
    const RegisterFlow::InternetEnd end = services.registration.internetEnded(outcome);
    shelf.registering(std::nullopt);
    switch (end)
    {
        case RegisterFlow::InternetEnd::Success:
            notifier.notify(services.strings.text("akira/hosts/registration_success"));
            refresh();
            return;
        case RegisterFlow::InternetEnd::Quiet:
            return;
        case RegisterFlow::InternetEnd::FallBackToPin:
            break;
    }
    notifier.notify(services.strings.text("akira/hosts/auto_register_fallback"));
    askPin(catalogName);
}

void ConsoleOperations::cancelRegistration(const std::string& catalogName)
{
    Host* target = host(catalogName);
    const bool internet = shelf.view().get().registering.has_value();
    if (internet)
        services.registration.userCanceled();
    if (target)
        actions().cancelRegistration(*target);
    services.registration.stopped();
    shelf.registering(std::nullopt);
    shelf.dismiss();
    log::info("[CONSOLES] registration of {} cancelled", catalogName);
}

void ConsoleOperations::link(const std::string& catalogName, const std::string& localName)
{
    Host* remote = host(catalogName);
    shelf.dismiss();
    if (!remote)
        return;
    const auto linked = actions().link(*remote, localName);
    if (!linked)
    {
        if (linked.error() == ConsoleActions::LinkRefusal::NameTaken)
            notifier.notify(services.strings.format("akira/hosts/link_name_taken",
                ConsoleEntry(remote->facts(), false).displayName()));
        return;
    }
    notifier.notify(services.strings.format("akira/hosts/linked_to", *linked));
    refresh();
}

auto ConsoleOperations::saveConsolePin(Host& target, const std::string& pin) -> std::optional<std::string>
{
    if (actions().setConsolePin(target, pin))
        return std::nullopt;
    return services.strings.text("akira/host_settings/console_pin_invalid");
}

auto ConsoleOperations::saveAddress(Host& target, const std::string& address) -> std::optional<std::string>
{
    const auto moved = actions().moveTo(target, address);
    if (moved)
        return std::nullopt;
    return services.strings.text(
        moved.error() == Host::AddressRefusal::Invalid ? "akira/hosts/address_invalid" : "akira/hosts/address_not_saved");
}

void ConsoleOperations::saveTakion(Host& target, int index)
{
    const TakionPinChoice choice = actions().takionPins(target);
    if (choice.contains(index))
        actions().pinTakion(target, choice.versionAt(index));
}

void ConsoleOperations::remove(const std::string& catalogName)
{
    shelf.dismiss();
    Host* target = host(catalogName);
    if (!target)
        return;
    const std::string name = target->getHostName();
    log::info("[CONSOLES] deleting {}", name);
    actions().remove(name);
    notifier.notify(services.strings.text("akira/hosts/host_deleted"));
    refresh();
}

void ConsoleOperations::openAdd()
{
    shelf.offerAccount(findsRemote());
    shelf.openAdd(true);
    publishScan();
}

void ConsoleOperations::chooseAddMode(ConsoleShelf::AddMode mode)
{
    shelf.chooseMode(mode);
    const std::optional<ConsoleShelf::Adding> adding = shelf.view().get().adding;
    if (!adding || adding->mode != mode)
        return;
    if (mode == ConsoleShelf::AddMode::Automatic)
        publishScan();
    if (mode == ConsoleShelf::AddMode::PlayStation && adding->lookup == ConsoleShelf::Lookup::Idle)
        findRemote();
}

void ConsoleOperations::publishScan()
{
    const DiscoveryManager::SweepStatus sweep = services.discovery.getSweepStatus();
    shelf.scanned(ConsoleShelf::Scan{ .running = sweep.serviceRunning,
        .searched = sweep.serviceRunning && !sweep.sweepActive,
        .broadcast = services.discovery.getLocalSubnetCidr(),
        .unicast = sweep.subnets,
        .address = services.discovery.getLocalAddress().value_or(std::string()) });
}

void ConsoleOperations::addByAddress(const std::string& address, bool ps5)
{
    const auto added = actions().addManual(address, address,
        ps5 ? libchiaking::consoles::Generation::ps5 : libchiaking::consoles::Generation::ps4);
    if (!added)
    {
        shelf.addFailed(manualRefusalText(added.error()));
        return;
    }
    shelf.openAdd(false);
    refresh();
    if (Host* target = host(address))
        beginRegistration(*target);
}

void ConsoleOperations::findRemote()
{
    accounts::Accounts* psn = services.core.accounts();
    const RemoteLookup lookup(psn ? std::optional(psn->state()) : std::nullopt);
    if (!lookup.allowed())
    {
        shelf.lookup(ConsoleShelf::Lookup::Failed);
        notifier.notify(services.strings.text("akira/hosts/no_psn_token"));
        return;
    }
    shelf.lookup(ConsoleShelf::Lookup::Loading);
    services.discovery.refreshRemoteDevices(
        [this, guard = alive](const DiscoveryManager::RemoteRefreshResult& result) {
            services.dispatcher.post([this, guard, result] {
                if (!*guard)
                    return;
                shelf.lookup(result.success ? ConsoleShelf::Lookup::Ready : ConsoleShelf::Lookup::Failed);
                if (!result.success)
                    log::warning("[CONSOLES] PlayStation console list failed: {}", result.message);
                refresh();
            });
        },
        true);
}

auto ConsoleOperations::findsRemote() const -> bool
{
    return !services.accounts.isLegacyProfileActive();
}

auto ConsoleOperations::refusalText(RegisterFlow::Refusal refusal) const -> std::string
{
    switch (refusal)
    {
        case RegisterFlow::Refusal::AccountId: return services.strings.text("akira/hosts/error_psn_account_id");
        case RegisterFlow::Refusal::OnlineId: return services.strings.text("akira/hosts/error_psn_online_id");
        case RegisterFlow::Refusal::ConsoleType: return services.strings.text("akira/hosts/error_console_type");
        case RegisterFlow::Refusal::Failed: break;
    }
    return services.strings.text("akira/hosts/error_registration_failed");
}

auto ConsoleOperations::manualRefusalText(ConsoleActions::ManualRefusal refusal) const -> std::string
{
    switch (refusal)
    {
        case ConsoleActions::ManualRefusal::NoName: return services.strings.text("akira/add_host/error_no_name");
        case ConsoleActions::ManualRefusal::NoAddress: return services.strings.text("akira/add_host/error_no_ip");
        case ConsoleActions::ManualRefusal::InvalidAddress: return services.strings.text("akira/add_host/error_invalid_ip");
        case ConsoleActions::ManualRefusal::DuplicateName:
            return services.strings.text("akira/add_host/error_duplicate_name");
    }
    return services.strings.text("akira/add_host/error_invalid_ip");
}

}
