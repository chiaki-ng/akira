#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "consoles/console_facts.hpp"

namespace akira::consoles {

class ConsoleEntry {
public:
    enum class Badge { Remote, Manual, Auto, Local };
    enum class Status { Link, Ready, Register, Standby, Presence };
    enum class Action { Link, Connect, Register, Wake };
    enum class Option { ConsolePin, AddAddress, SetAddress, TakionVersion, Delete };

    ConsoleEntry(ConsoleFacts facts, bool linkTargetsExist);

    const ConsoleFacts& facts() const { return known; }
    std::string displayName() const;
    Badge badge() const;
    Status status() const;
    bool dimmed() const;
    std::optional<Action> primaryAction() const;
    std::vector<Option> options() const;

private:
    static constexpr std::string_view remoteSuffix = " (Remote)";

    bool remote() const { return known.type == HostType::Remote; }
    bool usesInternetPairing() const { return remote() && !known.ps5; }
    bool offersLink() const;
    bool offersRegistration() const;

    ConsoleFacts known;
    bool linkTargetsExist;
};

}
