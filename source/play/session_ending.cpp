#include "play/session_ending.hpp"

#include <utility>

#include "play/connect_failure.hpp"

namespace akira::play {

using libchiaking::play::ConsoleQuit;
using libchiaking::play::EndReason;

SessionEnding::SessionEnding(libchiaking::play::Outcome outcome, Situation situation)
    : outcome(std::move(outcome))
    , situation(situation)
{
}

SessionEnding::Verdict SessionEnding::verdict() const
{
    if (situation.loginPinEntered && !stopped() && !unpaired())
        return Verdict::Reconnect;
    if (unpaired())
        return Verdict::Unpaired;
    if (stopped())
        return Verdict::Stopped;
    if (situation.cloudScreen)
        return Verdict::CloudFailure;
    if (outcome.launch_refused() && situation.cloudLaunch)
        return Verdict::LaunchRefused;
    return Verdict::Failed;
}

bool SessionEnding::endsPlayTime() const
{
    const auto& quit = outcome.quit();
    return quit && (quit->reason() == ConsoleQuit::Reason::time_limit_reached ||
                    quit->reason() == ConsoleQuit::Reason::trial_ended);
}

const char* SessionEnding::reasonKey() const
{
    const auto& quit = outcome.quit();
    return quit ? ConnectFailure::quitReason(quit->reason()) : ConnectFailure::forOutcome(outcome);
}

const char* SessionEnding::failureKey() const
{
    return ConnectFailure::forOutcome(outcome);
}

const char* SessionEnding::connectFailureKey() const
{
    return rejectedWhileAwake() ? "akira/connection/fail_rp_disabled" : ConnectFailure::forOutcome(outcome);
}

bool SessionEnding::stopped() const
{
    const auto& quit = outcome.quit();
    return outcome.was_left() || (quit && quit->reason() == ConsoleQuit::Reason::stopped);
}

bool SessionEnding::unpaired() const
{
    return outcome.cause() == EndReason::Cause::unpaired_console;
}

bool SessionEnding::rejectedWhileAwake() const
{
    const auto& quit = outcome.quit();
    return situation.consoleAwake && quit && quit->reason() == ConsoleQuit::Reason::session_request_unknown;
}

}
