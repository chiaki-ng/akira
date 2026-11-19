#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "consoles/host_record.hpp"
#include "consoles/pairing_store.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::consoles {

class ConsoleSettings final : public settings::Section {
public:
    using Records = std::map<std::string, HostRecord>;
    using LiveRecords = std::function<std::vector<HostRecord>()>;

    explicit ConsoleSettings(settings::SettingsStore& store);
    ~ConsoleSettings() override;

    ConsoleSettings(const ConsoleSettings&) = delete;
    ConsoleSettings& operator=(const ConsoleSettings&) = delete;

    void write(toml::table& config) const override;

    const Records& records() const { return saved; }
    Records follow(LiveRecords live);
    void unfollow(std::vector<HostRecord> last);
    int64_t claimConsoleId() { return nextConsoleId++; }

    PairingStore& pairings() { return pairingStore; }

private:
    using Firmware = libchiaking::consoles::Firmware;

    static std::optional<Firmware> firmwareFromTarget(int64_t target);
    static int64_t targetOf(const std::optional<Firmware>& firmware);
    static std::optional<Registration> registrationOf(const toml::table& row);
    static toml::table rowOf(const HostRecord& record);
    static void writeRecords(toml::table& config, const std::vector<const HostRecord*>& records);

    void read(const toml::table& config);

    settings::SettingsStore& store;
    Records saved;
    LiveRecords live;
    int64_t nextConsoleId = 1;
    PairingStore pairingStore;
};

}
