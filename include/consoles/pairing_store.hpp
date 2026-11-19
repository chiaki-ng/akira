#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <libchiaking/consoles.hpp>
#include <toml++/toml.hpp>

class SettingsManager;

namespace akira {

class PairingStore final : public libchiaking::consoles::ConsoleStore {
public:
    using SavedConsole = libchiaking::consoles::SavedConsole;
    using SavedPairing = libchiaking::consoles::SavedPairing;
    using HardwareId = libchiaking::consoles::HardwareId;
    using Firmware = libchiaking::consoles::Firmware;

    explicit PairingStore(SettingsManager& settings);

    [[nodiscard]] auto load_saved_consoles() -> libchiaking::Expected<Saved, Error> override;
    [[nodiscard]] auto save(const SavedConsole& console) -> libchiaking::Expected<void, Error> override;
    [[nodiscard]] auto save(const SavedPairing& pairing) -> libchiaking::Expected<void, Error> override;
    [[nodiscard]] auto remove_pairing(libchiaking::AccountId account, HardwareId console)
        -> libchiaking::Expected<void, Error> override;
    [[nodiscard]] auto remove_console(HardwareId console) -> libchiaking::Expected<void, Error> override;

    [[nodiscard]] auto adopt(SavedConsole console, std::vector<SavedPairing> pairings)
        -> libchiaking::Expected<void, Error>;
    auto knows(HardwareId console) const -> bool;

    void read(const toml::table& config);
    void write(toml::table& config) const;

    static auto firmwareOf(std::string_view text) -> std::optional<Firmware>;
    static auto textOf(Firmware firmware) -> std::string_view;

private:
    using NameSource = libchiaking::consoles::ConsoleName::Source;

    static constexpr std::array<std::pair<Firmware::Family, std::string_view>, 4> firmwareNames{{
        {Firmware::Family::ps4_8, "ps4_8"},
        {Firmware::Family::ps4_9, "ps4_9"},
        {Firmware::Family::ps4_10, "ps4_10"},
        {Firmware::Family::ps5_1, "ps5_1"},
    }};
    static constexpr std::array<std::pair<NameSource, std::string_view>, 5> nameSources{{
        {NameSource::generation, "generation"},
        {NameSource::account, "account"},
        {NameSource::pairing, "pairing"},
        {NameSource::discovered, "discovered"},
        {NameSource::renamed, "renamed"},
    }};

    static auto consoleOf(const toml::table& row) -> std::optional<SavedConsole>;
    static auto pairingOf(const toml::table& row, libchiaking::AccountId account)
        -> std::optional<SavedPairing>;
    static auto rowOf(const SavedConsole& console) -> toml::table;
    static auto rowOf(const SavedPairing& pairing) -> toml::table;
    static auto sourceOf(std::string_view text) -> NameSource;
    static auto textOf(NameSource source) -> std::string_view;

    void upsert(const SavedConsole& console);
    void upsert(const SavedPairing& pairing);
    auto persist() -> libchiaking::Expected<void, Error>;

    SettingsManager& settings;
    mutable std::mutex mutex;
    std::vector<SavedConsole> consoles;
    std::vector<SavedPairing> pairings;
};

}
