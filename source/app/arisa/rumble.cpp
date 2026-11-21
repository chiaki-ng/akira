#include "app/arisa/rumble.hpp"

#include <cstdint>

#include <arisa/provider/haptics/rumble_value.hpp>

#include "log/log.hpp"

namespace akira::app::arisa {

namespace provider = ::arisa::provider;

Rumble::Rumble(provider::HapticsProvider& haptics)
    : haptics(haptics)
{
}

auto Rumble::player(unsigned npad) -> std::optional<provider::RumblePlayer>
{
    if (npad == handheldNpad)
        return provider::RumblePlayer::handheld();
    if (npad >= provider::RumblePlayer::players)
        return std::nullopt;
    return provider::RumblePlayer::number(static_cast<std::uint8_t>(npad + 1));
}

void Rumble::send(provider::RumblePlayer target, Wave wave)
{
    const auto sent = haptics.rumble(target, provider::RumbleFrequency{ wave.freqLow },
        provider::RumbleFrequency{ wave.freqHigh }, provider::RumbleAmplitude{ wave.ampLow },
        provider::RumbleAmplitude{ wave.ampHigh });
    if (!sent)
        log::debug("[RUMBLE] refused: {}", sent.error().explanation());
}

void Rumble::playerOne(Wave wave)
{
    send(provider::RumblePlayer::primary(), wave);
}

void Rumble::npad(unsigned npad, Wave wave)
{
    if (const std::optional<provider::RumblePlayer> target = player(npad))
        send(*target, wave);
}

void Rumble::refresh(unsigned)
{
}

}
