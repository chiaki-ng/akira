#include "views/connection_stage.hpp"
#include "ui/theme.hpp"

#include <borealis.hpp>

#include <cmath>

ConnectionStage connectionStageOf(libchiaking::play::ConnectStep step)
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

const char* connectionStageLabelKey(ConnectionStage stage)
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

const char* connectionFailureKeyForStage(ConnectionStage stage)
{
    switch (stage) {
        case ConnectionStage::Waking: return "akira/connection/fail_wake";
        case ConnectionStage::Finding: return "akira/connection/fail_find";
        case ConnectionStage::Linking: return "akira/connection/fail_link";
        case ConnectionStage::Authenticating: return "akira/connection/fail_auth";
        case ConnectionStage::Tuning:
        case ConnectionStage::Starting: return "akira/connection/fail_stream";
        default: return "akira/connection/fail_generic";
    }
}

const char* connectionFailureKeyFor(const libchiaking::play::Outcome& outcome)
{
    using libchiaking::play::ConsoleQuit;
    using libchiaking::play::EndReason;

    if (const auto& quit = outcome.quit()) {
        switch (quit->reason()) {
            case ConsoleQuit::Reason::console_in_use: return "akira/connection/fail_in_use";
            case ConsoleQuit::Reason::session_request_connection_refused:
            case ConsoleQuit::Reason::control_connect_failed:
            case ConsoleQuit::Reason::control_connection_refused: return "akira/connection/fail_unreachable";
            case ConsoleQuit::Reason::remote_play_version_mismatch: return "akira/connection/fail_version";
            case ConsoleQuit::Reason::remote_play_crashed: return "akira/connection/fail_crash";
            case ConsoleQuit::Reason::internet_pairing_failed: return "akira/connection/fail_regist";
            case ConsoleQuit::Reason::stream_unknown:
            case ConsoleQuit::Reason::console_disconnected:
            case ConsoleQuit::Reason::console_shut_down: return "akira/connection/fail_stream";
            default: break;
        }
    }

    const auto cause = outcome.cause();
    if (!cause)
        return "akira/connection/fail_generic";
    switch (*cause) {
        case EndReason::Cause::session_request_in_use: return "akira/connection/fail_in_use";
        case EndReason::Cause::session_request_connection_refused:
        case EndReason::Cause::control_connection_refused:
        case EndReason::Cause::session_request_network_failure:
        case EndReason::Cause::session_request_timeout:
        case EndReason::Cause::control_network_failure:
        case EndReason::Cause::control_timeout:
        case EndReason::Cause::route_failed: return "akira/connection/fail_unreachable";
        case EndReason::Cause::session_request_version_mismatch: return "akira/connection/fail_version";
        case EndReason::Cause::pairing_rejected: return "akira/connection/fail_regist";
        case EndReason::Cause::pipeline_failed: return "akira/connection/fail_stream";
        default: return "akira/connection/fail_generic";
    }
}

const char* quitReasonKey(libchiaking::play::ConsoleQuit::Reason reason)
{
    using Reason = libchiaking::play::ConsoleQuit::Reason;
    switch (reason) {
        case Reason::stopped: return "akira/stream/quit_stopped";
        case Reason::session_request_unknown: return "akira/stream/quit_request_unknown";
        case Reason::session_request_connection_refused: return "akira/stream/quit_connection_refused";
        case Reason::console_in_use: return "akira/stream/quit_rp_in_use";
        case Reason::remote_play_crashed: return "akira/stream/quit_rp_crash";
        case Reason::remote_play_version_mismatch: return "akira/stream/quit_version_mismatch";
        case Reason::control_unknown: return "akira/stream/quit_ctrl_unknown";
        case Reason::control_connect_failed: return "akira/stream/quit_ctrl_connect_failed";
        case Reason::control_connection_refused: return "akira/stream/quit_ctrl_connection_refused";
        case Reason::stream_unknown: return "akira/stream/quit_stream_unknown";
        case Reason::console_disconnected: return "akira/stream/quit_remote_disconnected";
        case Reason::console_shut_down: return "akira/stream/quit_remote_shutdown";
        case Reason::internet_pairing_failed: return "akira/stream/quit_psn_regist_failed";
        default: return "akira/stream/quit_none";
    }
}

void drawConnectionFailure(NVGcontext* vg, float cx, float cy, const std::string& message)
{
    NVGcolor danger = akira::ui::active().danger;

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 14.0f);
    nvgFillColor(vg, danger);
    nvgFill(vg);

    nvgFontSize(vg, 22.0f);
    nvgFillColor(vg, akira::ui::active().text);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, cx, cy + 54.0f, message.c_str(), nullptr);
}

void drawConnectionRing(NVGcontext* vg, float cx, float cy, const std::string& label,
                        int stageIndex, int stageTotal)
{
    nvgSave(vg);

    NVGcolor accent = akira::ui::active().accent;

    const float radius = 54.0f;
    const float strokeW = 8.0f;
    const float ringCy = cy - 24.0f;

    NVGcolor track = akira::ui::active().textMuted;
    track.a = 0.22f;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, ringCy, radius);
    nvgStrokeWidth(vg, strokeW);
    nvgStrokeColor(vg, track);
    nvgStroke(vg);

    float t = static_cast<float>(brls::getCPUTimeUsec() % 1000000) / 1000000.0f;
    float a0 = t * 2.0f * NVG_PI - NVG_PI * 0.5f;
    float a1 = a0 + NVG_PI * 0.55f;
    nvgLineCap(vg, NVG_ROUND);
    nvgBeginPath(vg);
    nvgArc(vg, cx, ringCy, radius, a0, a1, NVG_CW);
    nvgStrokeWidth(vg, strokeW);
    nvgStrokeColor(vg, accent);
    nvgStroke(vg);

    if (stageTotal > 0) {
        std::string count = std::to_string(stageIndex) + " / " + std::to_string(stageTotal);
        nvgFontSize(vg, 26.0f);
        nvgFillColor(vg, akira::ui::active().text);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, cx, ringCy, count.c_str(), nullptr);
    }

    nvgFontSize(vg, 20.0f);
    nvgFillColor(vg, akira::ui::active().textMuted);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, cx, ringCy + radius + 28.0f, label.c_str(), nullptr);

    nvgRestore(vg);
}
