#pragma once

#include <optional>
#include <string>
#include <vector>

#include "consoles/console_entry.hpp"
#include "consoles/console_facts.hpp"

namespace akira::consoles {

class ConsoleList {
public:
    enum class Profile { None, Legacy, Signed };

    ConsoleList(std::vector<ConsoleFacts> consoles, Profile profile);

    static Profile profileFor(bool hasProfiles, bool legacy);

    std::vector<ConsoleEntry> shown() const;
    std::vector<std::string> linkTargets() const;
    ConsoleEntry entry(ConsoleFacts facts) const;
    bool hasProfile() const { return profile != Profile::None; }

private:
    bool lists(const ConsoleFacts& facts) const;
    std::optional<ConsoleFacts> internetFor(const ConsoleFacts& home) const;
    bool folded(const ConsoleFacts& facts) const;
    static bool linkable(const ConsoleFacts& facts);

    std::vector<ConsoleFacts> consoles;
    Profile profile;
};

}
