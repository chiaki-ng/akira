#include "cloud/release_notice.hpp"

namespace akira::cloud {

ReleaseNotice::ReleaseNotice(Kind kind, int64_t seconds, int64_t atMs)
    : noticeKind(kind)
    , remainingSeconds(seconds)
    , releaseMs(atMs)
{
}

auto ReleaseNotice::of(const Game& game, int64_t nowMs) -> std::optional<ReleaseNotice>
{
    switch (game.release)
    {
        case ReleaseKind::Unrestricted: return std::nullopt;
        case ReleaseKind::Unannounced: return ReleaseNotice(Kind::Unannounced, 0, game.availableFromMs);
        case ReleaseKind::Scheduled: break;
    }
    const int64_t remainingMs = game.availableFromMs - nowMs;
    if (remainingMs <= 0)
        return std::nullopt;
    const int64_t remaining = (remainingMs + 999) / 1000;
    if (remaining < CountdownLimit)
        return ReleaseNotice(Kind::Countdown, remaining, game.availableFromMs);
    return ReleaseNotice(Kind::Date, remaining, game.availableFromMs);
}

}
