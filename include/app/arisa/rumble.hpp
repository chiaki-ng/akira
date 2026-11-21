#pragma once

#include <optional>

#include <arisa/provider/haptics/provider.hpp>
#include <arisa/provider/haptics/rumble_player.hpp>

#include "platform/pads/rumble.hpp"

namespace akira::app::arisa {

class Rumble final : public platform::PadRumble {
public:
    explicit Rumble(::arisa::provider::HapticsProvider& haptics);

    void playerOne(Wave wave) override;
    void npad(unsigned npad, Wave wave) override;
    void refresh(unsigned npad) override;

private:
    static constexpr unsigned handheldNpad = 0x20;

    static auto player(unsigned npad) -> std::optional<::arisa::provider::RumblePlayer>;
    void send(::arisa::provider::RumblePlayer target, Wave wave);

    ::arisa::provider::HapticsProvider& haptics;
};

}
