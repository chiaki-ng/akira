#include "play/connect_flow.hpp"

#include <algorithm>

#include "play/connect_failure.hpp"

namespace akira::play {

ConnectionStage ConnectFlow::stageOf(libchiaking::play::ConnectStep step)
{
    using libchiaking::play::ConnectStep;
    switch (step) {
        case ConnectStep::waking_console: return ConnectionStage::Waking;
        case ConnectStep::finding_console:
        case ConnectStep::opening_account_session: return ConnectionStage::Finding;
        case ConnectStep::reaching_console:
        case ConnectStep::opening_stream_route: return ConnectionStage::Linking;
        case ConnectStep::pairing:
        case ConnectStep::requesting_session:
        case ConnectStep::opening_control: return ConnectionStage::Authenticating;
        case ConnectStep::measuring_connection: return ConnectionStage::Tuning;
        case ConnectStep::starting_stream: return ConnectionStage::Starting;
    }
    return ConnectionStage::Idle;
}

const char* ConnectFlow::labelKey(ConnectionStage stage)
{
    switch (stage) {
        case ConnectionStage::Waking: return "akira/connection/stage_waking";
        case ConnectionStage::Finding: return "akira/connection/stage_finding";
        case ConnectionStage::Linking: return "akira/connection/stage_linking";
        case ConnectionStage::Authenticating: return "akira/connection/stage_authenticating";
        case ConnectionStage::Tuning: return "akira/connection/stage_tuning";
        case ConnectionStage::Starting: return "akira/connection/stage_starting";
        case ConnectionStage::Connected: return "akira/connection/stage_starting";
        default: return "akira/connection/stage_connecting";
    }
}

void ConnectFlow::advance(ConnectionStage stage)
{
    int reported = static_cast<int>(stage);
    int known = current.load();
    while (reported > known && !current.compare_exchange_weak(known, reported))
        ;
}

void ConnectFlow::follow(libchiaking::play::ConnectStep step, bool reconnecting)
{
    const int reported = static_cast<int>(stageOf(step));
    if (reconnecting || reported > current.load())
        current = reported;
}

void ConnectFlow::restart()
{
    current = static_cast<int>(ConnectionStage::Idle);
}

void ConnectFlow::connected()
{
    current = static_cast<int>(ConnectionStage::Connected);
}

ConnectionStage ConnectFlow::stage() const
{
    return static_cast<ConnectionStage>(current.load());
}

void ConnectFlow::step(int index, int total, const std::string& label)
{
    stepped.store(true);

    if (index > 0 && total > 0) {
        stepIndex.store(index);
        stepTotal.store(total);
    }

    std::lock_guard<std::mutex> lock(stepMutex);
    stepLabel = label;
}

ConnectFlow::Ring ConnectFlow::ring() const
{
    if (stepped.load()) {
        Ring shown{.labelKey = "akira/connection/stage_connecting", .index = stepIndex.load(), .total = stepTotal.load()};
        std::lock_guard<std::mutex> lock(stepMutex);
        shown.label = stepLabel;
        return shown;
    }

    const ConnectionStage known = stage();
    return Ring{.labelKey = labelKey(known),
                .index = std::clamp(static_cast<int>(known), 1, stageCount),
                .total = stageCount};
}

bool ConnectFlow::settle()
{
    return !done.exchange(true);
}

bool ConnectFlow::cancel()
{
    if (done.exchange(true))
        return false;
    wasCancelled.store(true);
    return true;
}

std::optional<const char*> ConnectFlow::failOnRing()
{
    if (done.exchange(true))
        return std::nullopt;
    failureShown.store(true);
    return ConnectFailure::forStage(stage());
}

bool ConnectFlow::failOnDialog()
{
    return !done.exchange(true);
}

bool ConnectFlow::dismiss()
{
    return !dismissed.exchange(true);
}

}
