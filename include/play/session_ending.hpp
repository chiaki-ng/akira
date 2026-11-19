#pragma once

#include <libchiaking/detail/play/outcome.hpp>

namespace akira::play {

class SessionEnding {
public:
    enum class Verdict { Reconnect, Unpaired, Stopped, CloudFailure, LaunchRefused, Failed };

    struct Situation {
        bool loginPinEntered = false;
        bool duringConnect = false;
        bool consoleAwake = false;
        bool cloudScreen = false;
        bool cloudLaunch = false;
    };

    SessionEnding(libchiaking::play::Outcome outcome, Situation situation);

    Verdict verdict() const;
    bool duringConnect() const { return situation.duringConnect; }
    bool endsPlayTime() const;
    const char* reasonKey() const;
    const char* failureKey() const;
    const char* connectFailureKey() const;

private:
    bool stopped() const;
    bool unpaired() const;
    bool rejectedWhileAwake() const;

    libchiaking::play::Outcome outcome;
    Situation situation;
};

}
