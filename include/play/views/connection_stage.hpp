#ifndef AKIRA_CONNECTION_STAGE_HPP
#define AKIRA_CONNECTION_STAGE_HPP

#include <libchiaking/play_types.hpp>

#include <string>

struct NVGcontext;

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

ConnectionStage connectionStageOf(libchiaking::play::ConnectStep step);
const char* connectionStageLabelKey(ConnectionStage stage);
const char* connectionFailureKeyForStage(ConnectionStage stage);
const char* connectionFailureKeyFor(const libchiaking::play::Outcome& outcome);
const char* quitReasonKey(libchiaking::play::ConsoleQuit::Reason reason);
void drawConnectionRing(NVGcontext* vg, float cx, float cy, const std::string& label, int stageIndex, int stageTotal);
void drawConnectionFailure(NVGcontext* vg, float cx, float cy, const std::string& message);

#endif // AKIRA_CONNECTION_STAGE_HPP
