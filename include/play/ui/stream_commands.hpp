#pragma once

#include <string>

#include "play/stream_stats.hpp"

namespace akira::play::ui {

class StreamCommands {
public:
    virtual ~StreamCommands() = default;

    virtual void cancel() = 0;
    virtual void reconnect() = 0;
    virtual void resume() = 0;
    virtual void leave(bool rest) = 0;
    virtual void resetMotion() = 0;
    virtual void showStats(StatsOverlayMode mode) = 0;
    virtual auto stats() -> StreamStats = 0;
    virtual auto decoder() -> std::string = 0;
};

}
