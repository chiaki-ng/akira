#pragma once

#include <optional>
#include <string>

#include <libchiaking/detail/play/session.hpp>

namespace akira::play {

class StreamRecovery {
public:
    enum class LoginPinStep { Ignore, SubmitSaved, Ask };

    static LoginPinStep loginPin(bool cloud, bool previousWrong, const std::string& savedPin, bool connected);
    static bool watchesFocus(bool intentionalDisconnect, bool autoReconnect, bool sessionStarted, bool cloud);
    static bool sessionAlive(std::optional<libchiaking::play::SessionState> state);
    static bool lost(bool streamActive, bool alive);
};

}
