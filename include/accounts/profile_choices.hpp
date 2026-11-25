#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "accounts/profile.hpp"

namespace akira::app {
class AppSettings;
}

namespace akira::accounts {

class ProfileChoices {
public:
    struct Choice {
        std::optional<std::string> label;
        int64_t id = 0;
    };

    ProfileChoices(const std::vector<Profile>& profiles, int64_t activeId, const app::AppSettings& app);

    const std::vector<Choice>& choices() const { return entries; }
    int selected() const { return active; }
    std::optional<int64_t> idAt(int index) const;

private:
    std::vector<Choice> entries;
    int active = 0;
};

}
