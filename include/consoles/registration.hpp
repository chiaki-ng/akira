#ifndef AKIRA_REGISTRATION_HPP
#define AKIRA_REGISTRATION_HPP

#include <cstdint>

#include <libchiaking/consoles_types.hpp>

struct Registration {
    int64_t profileId = 0;
    libchiaking::consoles::PairingKeys keys;

    auto clone() const -> Registration { return Registration{profileId, keys.clone()}; }
};

#endif // AKIRA_REGISTRATION_HPP
