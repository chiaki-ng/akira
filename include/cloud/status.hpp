#pragma once

#include <string>

#include "cloud/models.hpp"

namespace akira::cloud {

enum class Availability {
    NoProfile,
    NeedsPairing,
    Checking,
    CatalogAvailable,
    SubscriptionRequired,
    Ready,
    Warning,
    Empty,
    Error,
    LaunchBlocked
};

struct Status {
    Availability availability = Availability::NoProfile;
    std::string title;
    std::string detail;
    bool canBrowse = false;
    bool canPair = false;
    bool degraded = false;
    int gameCount = 0;
    PlusMembership plusMembership = PlusMembership::Unknown;
};

}
