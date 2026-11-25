#pragma once

#include <cstdint>
#include <optional>

#include "cloud/models.hpp"

namespace akira::cloud {

class ReleaseNotice {
public:
    enum class Kind { Unannounced, Countdown, Date };

    static auto of(const Game& game, int64_t nowMs) -> std::optional<ReleaseNotice>;

    auto kind() const -> Kind { return noticeKind; }
    auto seconds() const -> int64_t { return remainingSeconds; }
    auto atMs() const -> int64_t { return releaseMs; }

private:
    static constexpr int64_t CountdownLimit = 86400 * 7;

    ReleaseNotice(Kind kind, int64_t seconds, int64_t atMs);

    Kind noticeKind;
    int64_t remainingSeconds;
    int64_t releaseMs;
};

}
