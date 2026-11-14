#include "core/host.hpp"
#include "core/chiaki_core.hpp"
#include "core/settings_manager.hpp"
#include "core/wireguard_manager.hpp"

#include <borealis.hpp>
#include <algorithm>
#include <ranges>

Host::Host(const std::string& name)
    : hostName(name)
{
    settings = SettingsManager::getInstance();
}

Host::~Host()
{
    pairingGeneration->fetch_add(1);
}

std::string Host::getHostAddr() const
{
    if (console) {
        if (const auto address = console->last_address())
            return address->text();
    }
    return hostAddr;
}

std::optional<Host::Firmware> Host::getFirmware() const
{
    if (console)
        return console->firmware();
    return firmware;
}

void Host::linkConsole(std::shared_ptr<libchiaking::consoles::Console> linked)
{
    console = std::move(linked);
    if (console)
        hardwareId = console->hardware_id();
}

bool Host::isPS5() const
{
    const auto known = getFirmware();
    return known && known->generation() == libchiaking::consoles::Generation::ps5;
}

bool Host::needsAccountId() const
{
    const auto known = getFirmware();
    return known && known->family() != Firmware::Family::ps4_8;
}

bool Host::hasRpKey() const
{
    const Profile* profile = settings ? settings->getActiveProfile() : nullptr;
    if (!profile)
        return false;
    if (hasLegacyRegistration(profile->id))
        return true;
    if (!console)
        return false;
    const auto account = libchiaking::AccountId::from_base64(profile->accountId);
    if (!account)
        return false;
    return std::ranges::contains(console->paired_accounts(), *account);
}

std::string Host::getStateString() const
{
    switch (presence)
    {
        case Presence::ready:
            return "Ready";
        case Presence::rest_mode:
            return "Standby";
        default:
            return "Unknown";
    }
}

std::string Host::getTargetString() const
{
    return isPS5() ? "PS5" : "PS4";
}

bool Host::hasLegacyRegistration(int64_t profileId) const
{
    return std::ranges::any_of(legacyRegistrations,
        [profileId](const Registration& reg) { return reg.profileId == profileId; });
}

void Host::removeRegistrationsForProfile(int64_t profileId)
{
    std::erase_if(legacyRegistrations,
        [profileId](const Registration& reg) { return reg.profileId == profileId; });
}

void Host::copyRegistrationFrom(const Host* other)
{
    if (!other) return;

    linkConsole(other->console);
    for (const Registration& reg : other->legacyRegistrations) {
        removeRegistrationsForProfile(reg.profileId);
        legacyRegistrations.push_back(reg.clone());
    }

    consolePIN = other->consolePIN;

    brls::Logger::info("Copied registration data from '{}' to '{}'", other->hostName, hostName);
}

libchiaking::Expected<void, Host::WakeRefusal> Host::wake()
{
    auto* accounts = akira::ChiakiCore::current().accounts();
    const auto user = accounts ? accounts->activeUser() : nullptr;
    if (!console || !user)
    {
        brls::Logger::error("Host::wake: {} has no paired console or no user to wake it as", hostName);
        return libchiaking::unexpected(WakeRefusal::not_paired);
    }

    const auto network = homeNetwork();
    auto woken = console->wake(*user, network);
    if (!woken)
        brls::Logger::error("Host::wake: {} refused (reason {})", hostName, static_cast<int>(woken.error()));
    else
        brls::Logger::info("Host::wake: wake packet sent to {}{}", hostName,
            network.network() ? " through the WireGuard tunnel" : "");
    return woken;
}

libchiaking::consoles::HomeNetwork Host::homeNetwork() const
{
    if (isRemote() || isCloud())
        return {};
    return WireGuardManager::instance().homeNetwork(getHostAddr());
}

HostRegisterError Host::registerHost(int pin, PairingCallback done)
{
    namespace consoles = libchiaking::consoles;
    namespace platform = libchiaking::platform;

    auto& core = akira::ChiakiCore::current();
    auto* accounts = core.accounts();
    auto user = accounts ? accounts->activeUser() : nullptr;
    if (!user)
    {
        brls::Logger::error("Undefined PSN account for registration (please configure the profile)");
        return needsAccountId() ? HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID : HOST_REGISTER_ERROR_SETTING_PSNONLINEID;
    }

    const auto known = getFirmware();
    if (!known)
    {
        brls::Logger::error("Undefined PS4 system version (please run discover first)");
        return HOST_REGISTER_ERROR_UNDEFINED_TARGET;
    }

    auto parsedPin = consoles::PairingPin::create(static_cast<uint32_t>(pin));
    if (!parsedPin)
    {
        brls::Logger::error("Registration PIN for {} is out of range", hostName);
        return HOST_REGISTER_ERROR_UNDEFINED_TARGET;
    }

    std::optional<consoles::Host> address;
    if (!console)
    {
        auto created = consoles::Host::create(hostAddr, known->generation());
        if (!created)
        {
            brls::Logger::error("Registration address {} is not usable", hostAddr);
            return HOST_REGISTER_ERROR_UNDEFINED_TARGET;
        }
        address = std::move(created.value());
    }

    const uint64_t generation = pairingGeneration->fetch_add(1) + 1;
    pairingThread = platform::Thread{};

    brls::Logger::info("Registering to host `{}` `{}` as `{}`", hostName, getHostAddr(),
        settings->maskAccountName(user->online_id()));

    auto work = [this, &core, user, found = console, address = std::move(address), firmware = *known,
                 pinHolder = std::make_shared<consoles::PairingPin>(std::move(*parsedPin)), generation,
                 name = hostName, done](platform::StopToken stop) {
        auto& known = core.chiaki().consoles();
        auto paired = found
            ? known.pair_at_home(*user, *found, std::move(*pinHolder), stop)
            : known.pair_at_home(*user, *address, firmware.generation(), std::move(*pinHolder), stop);

        PairingOutcome outcome = PairingOutcome::Paired;
        std::shared_ptr<consoles::Console> console;
        if (paired)
        {
            console = std::move(paired).value();
        }
        else
        {
            const auto reason = paired.error().reason;
            brls::Logger::error("Registration with {} failed (reason {})", name, static_cast<int>(reason));
            outcome = reason == consoles::PairingFailure::Reason::cancelled ? PairingOutcome::Canceled
                : reason == consoles::PairingFailure::Reason::refused       ? PairingOutcome::Refused
                                                                            : PairingOutcome::Failed;
        }

        brls::sync([this, alive = std::weak_ptr<std::atomic<uint64_t>>(pairingGeneration), generation, outcome,
                    console = std::move(console), done]() {
            const auto token = alive.lock();
            if (!token || token->load() != generation)
                return;
            finishPairing(outcome, console, done);
        });
    };

    auto started = platform::Thread::start(core.threading(), "registration", platform::ThreadRole::registration,
        std::move(work));
    if (!started)
    {
        brls::Logger::error("Registration thread could not start");
        return HOST_REGISTER_ERROR_UNDEFINED_TARGET;
    }
    pairingThread = std::move(started).value();
    return HOST_REGISTER_OK;
}

bool Host::canPairOverInternet() const
{
    if (settings->isLegacyProfileActive())
        return false;
    auto* accounts = akira::ChiakiCore::current().accounts();
    if (!accounts || accounts->state() != psn::SessionState::Valid)
        return false;
    return !settings->getPsnAccountId(const_cast<Host*>(this)).empty();
}

HostRegisterError Host::registerOverInternet(PairingStageCallback stage, PairingCallback done)
{
    namespace consoles = libchiaking::consoles;
    namespace platform = libchiaking::platform;

    auto& core = akira::ChiakiCore::current();
    auto* accounts = core.accounts();
    auto user = accounts ? accounts->activeUser() : nullptr;
    if (!user || settings->isLegacyProfileActive())
        return HOST_REGISTER_ERROR_SETTING_PSNACCOUNTID;

    const uint64_t generation = pairingGeneration->fetch_add(1) + 1;
    pairingThread = platform::Thread{};

    const auto consoleGeneration = isPS5() ? consoles::Generation::ps5 : consoles::Generation::ps4;
    brls::Logger::info("Pairing {} over the internet", hostName);

    auto work = [this, &core, accounts, user, generation, consoleGeneration, name = hostName,
                 duid = consoles::DeviceUid::parse(remoteDuid), stage, done](platform::StopToken stop) {
        const std::weak_ptr<std::atomic<uint64_t>> alive = pairingGeneration;
        auto& known = core.chiaki().consoles();
        auto finish = [this, alive, generation, done](PairingOutcome outcome,
                                                       std::shared_ptr<consoles::Console> paired) {
            brls::sync([this, alive, generation, outcome, paired = std::move(paired), done]() {
                const auto token = alive.lock();
                if (!token || token->load() != generation)
                    return;
                finishPairing(outcome, paired, done);
            });
        };

        const bool signedIn = accounts->staySignedIn(stop);
        brls::Logger::info("Internet pairing: sign-in renewed (valid={})", signedIn);
        postStage(alive, generation, stage, 2);

        auto listed = known.consoles_on_account(*user, consoleGeneration, stop);
        if (!listed)
        {
            brls::Logger::error("Internet pairing: PSN did not list the consoles (reason {})",
                static_cast<int>(listed.error().reason));
            finish(stop.stop_requested() ? PairingOutcome::Canceled : PairingOutcome::Failed, nullptr);
            return;
        }

        const auto match = std::ranges::find_if(listed.value(), [&](const consoles::AccountConsole& entry) {
            return (duid && entry.device_uid() == *duid) || entry.name() == name;
        });
        if (match == listed.value().end())
        {
            brls::Logger::error("Internet pairing: {} is not on the PSN account", name);
            finish(PairingOutcome::Failed, nullptr);
            return;
        }

        postStage(alive, generation, stage, 3);
        InternetPairingWatcher watcher(alive, generation, stage);
        auto paired = known.pair_over_the_internet(*user, *match, &watcher, stop);
        if (!paired)
        {
            const auto reason = paired.error().reason;
            brls::Logger::error("Internet pairing with {} failed (reason {})", name, static_cast<int>(reason));
            finish(reason == consoles::PairingFailure::Reason::cancelled ? PairingOutcome::Canceled
                : reason == consoles::PairingFailure::Reason::refused    ? PairingOutcome::Refused
                                                                         : PairingOutcome::Failed,
                nullptr);
            return;
        }
        finish(PairingOutcome::Paired, std::move(paired).value());
    };

    auto started = platform::Thread::start(core.threading(), "internet-pairing", platform::ThreadRole::registration,
        std::move(work));
    if (!started)
    {
        brls::Logger::error("Internet pairing thread could not start");
        return HOST_REGISTER_ERROR_HOLEPUNCH;
    }
    pairingThread = std::move(started).value();
    return HOST_REGISTER_OK;
}

void Host::postStage(const std::weak_ptr<std::atomic<uint64_t>>& alive, uint64_t generation,
                     const PairingStageCallback& stage, int index)
{
    if (!stage)
        return;
    brls::sync([alive, generation, stage, index]() {
        const auto token = alive.lock();
        if (!token || token->load() != generation)
            return;
        stage(index, internetPairingStages);
    });
}

Host::InternetPairingWatcher::InternetPairingWatcher(std::weak_ptr<std::atomic<uint64_t>> alive, uint64_t generation,
                                                     PairingStageCallback stage)
    : alive(std::move(alive))
    , generation(generation)
    , stage(std::move(stage))
{
}

void Host::InternetPairingWatcher::on_stage(libchiaking::consoles::InternetPairingStage reached)
{
    using libchiaking::consoles::InternetPairingStage;
    brls::Logger::info("Internet pairing stage {}", static_cast<int>(reached));
    if (reached == InternetPairingStage::pairing)
        postStage(alive, generation, stage, internetPairingStages);
}

void Host::cancelRegistration()
{
    pairingGeneration->fetch_add(1);
    static_cast<void>(pairingThread.request_stop());
}

void Host::abandonRegistration()
{
    cancelRegistration();
    pairingThread = libchiaking::platform::Thread{};
}

void Host::releaseConsole()
{
    console.reset();
}

void Host::finishPairing(PairingOutcome outcome, std::shared_ptr<libchiaking::consoles::Console> paired,
                         const PairingCallback& done)
{
    if (outcome == PairingOutcome::Paired && paired)
    {
        linkConsole(paired);
        firmware = paired->firmware();
        serverNickname = paired->name();
        if (const Profile* profile = settings->getActiveProfile())
            removeRegistrationsForProfile(profile->id);
        brls::Logger::info("Register Success {}", hostName);
    }
    if (done)
        done(outcome);
}
