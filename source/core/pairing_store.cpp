#include "core/pairing_store.hpp"
#include "core/profile_tables.hpp"
#include "core/settings_manager.hpp"
#include "util/base64.hpp"

#include <algorithm>
#include <string>

namespace akira {

using libchiaking::unexpected;
using libchiaking::consoles::ConsoleName;
using libchiaking::consoles::DeviceUid;
using libchiaking::consoles::PairingKeys;

PairingStore::PairingStore(SettingsManager& settings)
    : settings(settings)
{
}

auto PairingStore::load_saved_consoles() -> libchiaking::Expected<Saved, Error>
{
    std::lock_guard lock(mutex);
    Saved saved;
    saved.consoles = consoles;
    saved.pairings.reserve(pairings.size());
    for (const SavedPairing& pairing : pairings)
        saved.pairings.push_back(pairing.clone());
    return saved;
}

auto PairingStore::save(const SavedConsole& console) -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        upsert(console);
    }
    return persist();
}

auto PairingStore::save(const SavedPairing& pairing) -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        upsert(pairing);
    }
    return persist();
}

auto PairingStore::remove_pairing(libchiaking::AccountId account, HardwareId console)
    -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        std::erase_if(pairings, [&](const SavedPairing& pairing) {
            return pairing.account == account && pairing.console == console;
        });
    }
    return persist();
}

auto PairingStore::remove_console(HardwareId console) -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        std::erase_if(consoles, [&](const SavedConsole& saved) { return saved.hardware_id == console; });
        std::erase_if(pairings, [&](const SavedPairing& pairing) { return pairing.console == console; });
    }
    return persist();
}

auto PairingStore::adopt(SavedConsole console, std::vector<SavedPairing> adopted)
    -> libchiaking::Expected<void, Error>
{
    {
        std::lock_guard lock(mutex);
        upsert(console);
        for (const SavedPairing& pairing : adopted)
            upsert(pairing);
    }
    return persist();
}

auto PairingStore::knows(HardwareId console) const -> bool
{
    std::lock_guard lock(mutex);
    return std::ranges::any_of(consoles, [&](const SavedConsole& saved) { return saved.hardware_id == console; });
}

void PairingStore::read(const toml::table& config)
{
    std::vector<SavedConsole> loadedConsoles;
    std::vector<SavedPairing> loadedPairings;
    if (const auto* array = config["paired_consoles"].as_array()) {
        for (const auto& node : *array) {
            if (const auto* row = node.as_table()) {
                if (auto console = consoleOf(*row))
                    loadedConsoles.push_back(std::move(*console));
            }
        }
    }
    ProfileTables::each(config, [&loadedPairings](libchiaking::AccountId account, const toml::table& profile) {
        const auto* array = profile["pairings"].as_array();
        if (!array)
            return;
        for (const auto& node : *array) {
            if (const auto* row = node.as_table()) {
                if (auto pairing = pairingOf(*row, account))
                    loadedPairings.push_back(std::move(*pairing));
            }
        }
    });
    std::lock_guard lock(mutex);
    consoles = std::move(loadedConsoles);
    pairings = std::move(loadedPairings);
}

void PairingStore::write(toml::table& config) const
{
    std::lock_guard lock(mutex);
    toml::array consoleRows;
    for (const SavedConsole& console : consoles)
        consoleRows.push_back(rowOf(console));
    if (!consoleRows.empty())
        config.insert("paired_consoles", std::move(consoleRows));
    ProfileTables::each(config, [this](libchiaking::AccountId account, toml::table& profile) {
        toml::array owned;
        for (const SavedPairing& pairing : pairings) {
            if (pairing.account == account)
                owned.push_back(rowOf(pairing));
        }
        if (!owned.empty())
            profile.insert_or_assign("pairings", std::move(owned));
    });
}

auto PairingStore::firmwareOf(std::string_view text) -> std::optional<Firmware>
{
    const auto found = std::ranges::find(firmwareNames, text, &std::pair<Firmware::Family, std::string_view>::second);
    if (found == firmwareNames.end())
        return std::nullopt;
    return Firmware{found->first};
}

auto PairingStore::textOf(Firmware firmware) -> std::string_view
{
    const auto found = std::ranges::find(firmwareNames, firmware.family(),
                                         &std::pair<Firmware::Family, std::string_view>::first);
    return found->second;
}

auto PairingStore::sourceOf(std::string_view text) -> NameSource
{
    const auto found = std::ranges::find(nameSources, text, &std::pair<NameSource, std::string_view>::second);
    return found == nameSources.end() ? NameSource::renamed : found->first;
}

auto PairingStore::textOf(NameSource source) -> std::string_view
{
    const auto found = std::ranges::find(nameSources, source, &std::pair<NameSource, std::string_view>::first);
    return found->second;
}

auto PairingStore::consoleOf(const toml::table& row) -> std::optional<SavedConsole>
{
    const auto hardwareId = HardwareId::parse(row["hardware_id"].value_or(std::string_view{}));
    const auto firmware = firmwareOf(row["firmware"].value_or(std::string_view{}));
    if (!hardwareId || !firmware)
        return std::nullopt;

    auto name = ConsoleName::create(row["name"].value_or(std::string{}),
                                    sourceOf(row["name_source"].value_or(std::string_view{})));
    SavedConsole console{*hardwareId, *firmware,
                         name ? std::move(*name) : ConsoleName::of(firmware->generation())};
    if (const auto* address = row["address"].as_string()) {
        if (auto host = libchiaking::consoles::Host::create(address->get(), firmware->generation()))
            console.last_address = std::move(host.value());
    }
    return console;
}

auto PairingStore::pairingOf(const toml::table& row, libchiaking::AccountId account)
    -> std::optional<SavedPairing>
{
    const auto console = HardwareId::parse(row["hardware_id"].value_or(std::string_view{}));
    if (!console)
        return std::nullopt;

    std::array<std::byte, PairingKeys::registration_key_size> registrationKey{};
    std::array<std::byte, PairingKeys::remote_play_key_size> remotePlayKey{};
    const bool decoded =
        akira::base64::decodeExactly(row["rp_regist_key"].value_or(std::string_view{}), registrationKey) &&
        akira::base64::decodeExactly(row["rp_key"].value_or(std::string_view{}), remotePlayKey);
    auto keys = decoded
        ? PairingKeys::create(registrationKey, remotePlayKey,
                              static_cast<std::uint32_t>(row["rp_key_type"].value_or(int64_t{0})))
        : std::nullopt;
    libchiaking::memory::secure_zero(std::span<std::byte>{registrationKey});
    libchiaking::memory::secure_zero(std::span<std::byte>{remotePlayKey});
    if (!keys)
        return std::nullopt;

    SavedPairing pairing{account, *console, std::move(*keys)};
    if (const auto* uid = row["device_uid"].as_string())
        pairing.device_uid = DeviceUid::parse(uid->get());
    return pairing;
}

auto PairingStore::rowOf(const SavedConsole& console) -> toml::table
{
    toml::table row;
    row.insert("hardware_id", console.hardware_id.text());
    row.insert("firmware", std::string{textOf(console.firmware)});
    row.insert("name", console.name.text());
    row.insert("name_source", std::string{textOf(console.name.source())});
    if (console.last_address)
        row.insert("address", console.last_address->text());
    return row;
}

auto PairingStore::rowOf(const SavedPairing& pairing) -> toml::table
{
    toml::table row;
    row.insert("hardware_id", pairing.console.text());
    row.insert("rp_regist_key", akira::base64::encode(pairing.keys.registration_key()));
    row.insert("rp_key", akira::base64::encode(pairing.keys.remote_play_key()));
    row.insert("rp_key_type", static_cast<int64_t>(pairing.keys.key_type()));
    if (pairing.device_uid)
        row.insert("device_uid", pairing.device_uid->text());
    return row;
}

void PairingStore::upsert(const SavedConsole& console)
{
    const auto existing = std::ranges::find(consoles, console.hardware_id, &SavedConsole::hardware_id);
    if (existing != consoles.end())
        *existing = console;
    else
        consoles.push_back(console);
}

void PairingStore::upsert(const SavedPairing& pairing)
{
    const auto existing = std::ranges::find_if(pairings, [&](const SavedPairing& saved) {
        return saved.account == pairing.account && saved.console == pairing.console;
    });
    if (existing != pairings.end())
        *existing = pairing.clone();
    else
        pairings.push_back(pairing.clone());
}

auto PairingStore::persist() -> libchiaking::Expected<void, Error>
{
    if (settings.writeStores() != 0)
        return unexpected(Error::unavailable);
    return {};
}

}
