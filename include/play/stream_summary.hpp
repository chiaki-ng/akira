#pragma once

#include <string>

#include "play/stream_stats.hpp"

namespace akira::play {

class StreamSummary {
public:
    enum class Health { Good, Fair, Poor };

    explicit StreamSummary(const StreamStats& stats);

    Health health() const;
    std::string meta() const;

private:
    StreamStats stats;
};

}
