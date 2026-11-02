#ifndef AKIRA_PROFILE_HPP
#define AKIRA_PROFILE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "cloud/models.hpp"

struct Profile {
    int64_t id = 0;

    std::string onlineId;
    std::string accountId;

    std::string avatarUrl;
    bool isPlus = false;
    std::string region;
    int trophyLevel = 0;
    bool trophiesEnabled = true;

    bool legacy = false;

    std::vector<cloud::Game> cloudShortcuts;

    bool isRemote() const { return !legacy && !accountId.empty(); }
    bool isLegacy() const { return legacy; }
    bool hasIdentity() const { return !accountId.empty() || !onlineId.empty(); }

    std::string label() const {
        if (!onlineId.empty()) return onlineId;
        if (!accountId.empty()) return accountId;
        return "";
    }
};

#endif // AKIRA_PROFILE_HPP
