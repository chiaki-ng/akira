#include "play/connect_failure.hpp"

namespace akira::play {

const char* ConnectFailure::forStage(ConnectionStage stage)
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

const char* ConnectFailure::forOutcome(const libchiaking::play::Outcome& outcome)
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

const char* ConnectFailure::forRefusal(libchiaking::play::StartRefusal::Kind kind)
{
    using Kind = libchiaking::play::StartRefusal::Kind;
    switch (kind) {
        case Kind::needs_psn_sign_in:
        case Kind::sign_in_expired: return "akira/stream/psn_token_expired";
        case Kind::remote_play_disabled: return "akira/connection/fail_rp_disabled";
        case Kind::not_paired: return "akira/connection/fail_regist";
        case Kind::console_unreachable: return "akira/connection/fail_unreachable";
        case Kind::busy: return "akira/connection/fail_in_use";
        default: return "akira/connection/fail_generic";
    }
}

const char* ConnectFailure::quitReason(libchiaking::play::ConsoleQuit::Reason reason)
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

std::uint8_t ConnectFailure::seatStatus(const libchiaking::play::SeatRefusal& refusal)
{
    using Kind = libchiaking::play::SeatRefusal::Kind;
    switch (refusal.kind()) {
        case Kind::account_not_on_console: return 1;
        case Kind::refused_by_console: return refusal.console_status().value_or(0);
        case Kind::passcode_attempts_used: return libchiaking::play::SeatRefusal::passcode_attempts_status;
        case Kind::timed_out: return libchiaking::play::SeatRefusal::timed_out_status;
        default: return 0;
    }
}

}
