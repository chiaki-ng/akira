#include "consoles/console_settings.hpp"
#include "play/takion_pins.hpp"
#include "settings/settings_store.hpp"
#include "util/base64.hpp"
#include "log/log.hpp"

#include <utility>

namespace akira::consoles {

ConsoleSettings::ConsoleSettings(settings::SettingsStore& store)
    : store(store), pairingStore(store)
{
    read(store.loaded());
    store.attach(*this);
}

ConsoleSettings::~ConsoleSettings()
{
    store.detach(*this);
}

ConsoleSettings::Records ConsoleSettings::follow(LiveRecords source)
{
    live = std::move(source);
    return std::exchange(saved, {});
}

void ConsoleSettings::unfollow(std::vector<HostRecord> last)
{
    live = nullptr;
    saved.clear();
    for (HostRecord& record : last) {
        std::string name = record.nickname;
        saved.insert_or_assign(std::move(name), std::move(record));
    }
}

void ConsoleSettings::read(const toml::table& config)
{
    if (const auto* rows = config["consoles"].as_array()) {
        for (const auto& element : *rows) {
            const auto* row = element.as_table();
            if (!row) continue;

            std::string nickname = (*row)["nickname"].value<std::string>().value_or("");
            if (nickname.empty()) continue;

            auto [entry, created] = saved.try_emplace(nickname);
            HostRecord& record = entry->second;
            if (created)
                record.consoleId = nextConsoleId++;
            record.nickname = nickname;
            record.serverNickname = (*row)["server_nickname"].value<std::string>().value_or("");
            record.consoleId = (*row)["console_id"].value<int64_t>().value_or(record.consoleId);
            if (record.consoleId >= nextConsoleId)
                nextConsoleId = record.consoleId + 1;

            if (auto val = (*row)["host_type"].value<int64_t>())
                record.hostType = static_cast<HostType>(*val);
            if (auto val = (*row)["host_addr"].value<std::string>())
                record.hostAddr = *val;
            if (auto val = (*row)["target"].value<int64_t>())
                record.firmware = firmwareFromTarget(*val);
            if (auto val = (*row)["hardware_id"].value<std::string_view>())
                record.hardwareId = libchiaking::consoles::HardwareId::parse(*val);
            if (auto val = (*row)["console_pin"].value<std::string>())
                record.consolePin = *val;
            if (auto val = (*row)["haptic"].value<int64_t>())
                record.haptic = static_cast<int>(*val);
            const bool ps5 = record.firmware && record.firmware->generation() == libchiaking::consoles::Generation::ps5;
            if (auto val = (*row)["takion_version"].value<int64_t>();
                val && *val > 0 && play::TakionPins::isPinnable(ps5, static_cast<std::uint32_t>(*val)))
                record.pinnedTakionVersion = static_cast<std::uint32_t>(*val);
            if (auto val = (*row)["video_codec"].value<std::string_view>())
                record.codec = codecNamed(*val);
            if (auto val = (*row)["remote_duid"].value<std::string>())
                record.remoteDuid = *val;
        }
    }

    if (const auto* rows = config["registrations"].as_array()) {
        for (const auto& element : *rows) {
            const auto* row = element.as_table();
            if (!row) continue;

            const int64_t consoleId = (*row)["console_id"].value<int64_t>().value_or(0);
            HostRecord* record = nullptr;
            for (auto& [name, candidate] : saved) {
                if (candidate.consoleId == consoleId) {
                    record = &candidate;
                    break;
                }
            }
            if (!record) continue;

            auto registration = registrationOf(*row);
            if (!registration)
                continue;

            std::erase_if(record->legacyRegistrations, [&](const Registration& existing) {
                return existing.profileId == registration->profileId;
            });
            record->legacyRegistrations.push_back(std::move(*registration));
        }
    }

    log::info("Loaded {} console(s) from TOML config", saved.size());
}

void ConsoleSettings::write(toml::table& config) const
{
    std::vector<const HostRecord*> records;
    std::vector<HostRecord> current;
    if (live) {
        current = live();
        for (const HostRecord& record : current)
            records.push_back(&record);
    } else {
        for (const auto& [name, record] : saved)
            records.push_back(&record);
    }
    writeRecords(config, records);
}

void ConsoleSettings::writeRecords(toml::table& config, const std::vector<const HostRecord*>& records)
{
    toml::array consoles;
    toml::array registrations;
    for (const HostRecord* record : records) {
        consoles.push_back(rowOf(*record));
        for (const Registration& registration : record->legacyRegistrations) {
            toml::table row;
            row.insert("console_id", record->consoleId);
            row.insert("profile_id", registration.profileId);
            row.insert("rp_key", akira::base64::encode(registration.keys.remote_play_key()));
            row.insert("rp_regist_key", akira::base64::encode(registration.keys.registration_key()));
            row.insert("rp_key_type", static_cast<int64_t>(registration.keys.key_type()));
            registrations.push_back(std::move(row));
        }
    }
    if (!consoles.empty())
        config.insert("consoles", std::move(consoles));
    if (!registrations.empty())
        config.insert("registrations", std::move(registrations));
}

toml::table ConsoleSettings::rowOf(const HostRecord& record)
{
    toml::table row;
    row.insert("console_id", record.consoleId);
    row.insert("nickname", record.nickname);
    if (!record.serverNickname.empty()) row.insert("server_nickname", record.serverNickname);
    row.insert("host_addr", record.hostAddr);
    row.insert("target", targetOf(record.firmware));
    if (record.hardwareId) row.insert("hardware_id", record.hardwareId->text());
    row.insert("host_type", std::to_underlying(record.hostType));
    if (!record.consolePin.empty()) row.insert("console_pin", record.consolePin);
    if (!record.remoteDuid.empty()) row.insert("remote_duid", record.remoteDuid);
    if (record.haptic >= 0) row.insert("haptic", record.haptic);
    if (record.pinnedTakionVersion) row.insert("takion_version", static_cast<int64_t>(*record.pinnedTakionVersion));
    if (record.codec) row.insert("video_codec", codecName(*record.codec));
    return row;
}

std::optional<ConsoleSettings::Firmware> ConsoleSettings::firmwareFromTarget(int64_t target)
{
    using Family = Firmware::Family;
    if (target >= 1000000)
        return Firmware{Family::ps5_1};
    if (target >= 1000)
        return Firmware{Family::ps4_10};
    if (target >= 900)
        return Firmware{Family::ps4_9};
    if (target >= 800)
        return Firmware{Family::ps4_8};
    return std::nullopt;
}

std::optional<libchiaking::media::VideoCodec> ConsoleSettings::codecNamed(std::string_view name)
{
    if (name == "h264")
        return libchiaking::media::VideoCodec::h264;
    if (name == "h265")
        return libchiaking::media::VideoCodec::h265;
    return std::nullopt;
}

std::string_view ConsoleSettings::codecName(libchiaking::media::VideoCodec codec)
{
    return codec == libchiaking::media::VideoCodec::h264 ? "h264" : "h265";
}

int64_t ConsoleSettings::targetOf(const std::optional<Firmware>& firmware)
{
    using Family = Firmware::Family;
    if (!firmware)
        return 0;
    switch (firmware->family()) {
        case Family::ps4_8: return 800;
        case Family::ps4_9: return 900;
        case Family::ps4_10: return 1000;
        case Family::ps5_1: return 1000100;
    }
    return 0;
}

std::optional<Registration> ConsoleSettings::registrationOf(const toml::table& row)
{
    using PairingKeys = libchiaking::consoles::PairingKeys;
    std::array<std::byte, PairingKeys::registration_key_size> registKey{};
    std::array<std::byte, PairingKeys::remote_play_key_size> key{};
    const bool decoded =
        akira::base64::decodeExactly(row["rp_regist_key"].value_or(std::string_view{}), registKey) &&
        akira::base64::decodeExactly(row["rp_key"].value_or(std::string_view{}), key);
    auto keys = decoded
        ? PairingKeys::create(registKey, key, static_cast<uint32_t>(row["rp_key_type"].value_or(int64_t{0})))
        : std::nullopt;
    libchiaking::memory::secure_zero(std::span<std::byte>{registKey});
    libchiaking::memory::secure_zero(std::span<std::byte>{key});
    if (!keys)
        return std::nullopt;
    return Registration{row["profile_id"].value_or(int64_t{0}), std::move(*keys)};
}

}
