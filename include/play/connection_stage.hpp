#pragma once

namespace akira::play {

enum class ConnectionStage : int {
    Idle = 0,
    Waking,
    Finding,
    Linking,
    Authenticating,
    Tuning,
    Starting,
    Connected
};

}
