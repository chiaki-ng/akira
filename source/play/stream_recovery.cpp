#include "play/stream_recovery.hpp"

namespace akira::play {

StreamRecovery::LoginPinStep StreamRecovery::loginPin(bool cloud, bool previousWrong, const std::string& savedPin,
                                                      bool connected)
{
    if (cloud)
        return LoginPinStep::Ignore;
    if (!previousWrong && !savedPin.empty() && connected)
        return LoginPinStep::SubmitSaved;
    return LoginPinStep::Ask;
}

bool StreamRecovery::watchesFocus(bool intentionalDisconnect, bool autoReconnect, bool sessionStarted, bool cloud)
{
    return !intentionalDisconnect && autoReconnect && sessionStarted && !cloud;
}

bool StreamRecovery::sessionAlive(std::optional<libchiaking::play::SessionState> state)
{
    return state && (*state == libchiaking::play::SessionState::playing ||
                     *state == libchiaking::play::SessionState::connecting);
}

bool StreamRecovery::lost(bool streamActive, bool alive)
{
    return !streamActive || !alive;
}

}
