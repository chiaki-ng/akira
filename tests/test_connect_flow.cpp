#include "test_util.hpp"

#include "play/connect_failure.hpp"
#include "play/connect_flow.hpp"

#include <string>

using akira::play::ConnectFailure;
using akira::play::ConnectFlow;
using akira::play::ConnectionStage;
using libchiaking::play::ConnectStep;

TEST(connect_flow_stages_only_move_forward)
{
    ConnectFlow flow;
    CHECK(flow.stage() == ConnectionStage::Idle);
    flow.advance(ConnectionStage::Linking);
    flow.advance(ConnectionStage::Finding);
    CHECK(flow.stage() == ConnectionStage::Linking);
    flow.advance(ConnectionStage::Tuning);
    CHECK(flow.stage() == ConnectionStage::Tuning);
}

TEST(connect_flow_follows_core_steps)
{
    ConnectFlow flow;
    flow.follow(ConnectStep::opening_control, false);
    CHECK(flow.stage() == ConnectionStage::Authenticating);
    flow.follow(ConnectStep::finding_console, false);
    CHECK(flow.stage() == ConnectionStage::Authenticating);
    flow.follow(ConnectStep::finding_console, true);
    CHECK(flow.stage() == ConnectionStage::Finding);
    flow.connected();
    CHECK(flow.stage() == ConnectionStage::Connected);
    flow.restart();
    CHECK(flow.stage() == ConnectionStage::Idle);
}

TEST(connect_flow_maps_every_step_to_a_stage)
{
    CHECK(ConnectFlow::stageOf(ConnectStep::waking_console) == ConnectionStage::Waking);
    CHECK(ConnectFlow::stageOf(ConnectStep::opening_account_session) == ConnectionStage::Finding);
    CHECK(ConnectFlow::stageOf(ConnectStep::reaching_console) == ConnectionStage::Linking);
    CHECK(ConnectFlow::stageOf(ConnectStep::opening_stream_route) == ConnectionStage::Linking);
    CHECK(ConnectFlow::stageOf(ConnectStep::pairing) == ConnectionStage::Authenticating);
    CHECK(ConnectFlow::stageOf(ConnectStep::requesting_session) == ConnectionStage::Authenticating);
    CHECK(ConnectFlow::stageOf(ConnectStep::measuring_connection) == ConnectionStage::Tuning);
    CHECK(ConnectFlow::stageOf(ConnectStep::starting_stream) == ConnectionStage::Starting);
}

TEST(connect_flow_ring_clamps_stage_into_six_steps)
{
    ConnectFlow flow;
    ConnectFlow::Ring idle = flow.ring();
    CHECK_EQ(idle.index, 1);
    CHECK_EQ(idle.total, 6);
    CHECK_EQ(std::string(idle.labelKey), std::string("akira/connection/stage_connecting"));
    CHECK(idle.label.empty());

    flow.connected();
    ConnectFlow::Ring done = flow.ring();
    CHECK_EQ(done.index, 6);
    CHECK_EQ(std::string(done.labelKey), std::string("akira/connection/stage_starting"));
}

TEST(connect_flow_ring_prefers_route_steps)
{
    ConnectFlow flow;
    flow.advance(ConnectionStage::Linking);
    flow.step(2, 5, "Allocating");
    ConnectFlow::Ring ring = flow.ring();
    CHECK_EQ(ring.index, 2);
    CHECK_EQ(ring.total, 5);
    CHECK_EQ(ring.label, std::string("Allocating"));

    flow.step(0, 0, "");
    ring = flow.ring();
    CHECK_EQ(ring.index, 2);
    CHECK_EQ(ring.total, 5);
    CHECK(ring.label.empty());
    CHECK_EQ(std::string(ring.labelKey), std::string("akira/connection/stage_connecting"));
}

TEST(connect_flow_settles_once)
{
    ConnectFlow flow;
    CHECK(!flow.settled());
    CHECK(flow.settle());
    CHECK(!flow.settle());
    CHECK(!flow.cancel());
    CHECK(!flow.cancelled());

    ConnectFlow cancelled;
    CHECK(cancelled.cancel());
    CHECK(cancelled.cancelled());
    CHECK(cancelled.settled());
    CHECK(!cancelled.settle());
}

TEST(connect_flow_names_a_failure_by_its_stage)
{
    ConnectFlow flow;
    flow.advance(ConnectionStage::Linking);
    CHECK_EQ(std::string(flow.failOnRing()), std::string("akira/connection/fail_link"));
    CHECK(flow.settled());
    CHECK(flow.showingFailure());
    CHECK(flow.dismiss());
    CHECK(!flow.dismiss());

    ConnectFlow dialog;
    dialog.failOnDialog();
    CHECK(dialog.settled());
    CHECK(!dialog.showingFailure());
}

TEST(connect_failure_keys_by_stage_and_refusal)
{
    using Kind = libchiaking::play::StartRefusal::Kind;
    CHECK_EQ(std::string(ConnectFailure::forStage(ConnectionStage::Waking)), std::string("akira/connection/fail_wake"));
    CHECK_EQ(std::string(ConnectFailure::forStage(ConnectionStage::Starting)), std::string("akira/connection/fail_stream"));
    CHECK_EQ(std::string(ConnectFailure::forStage(ConnectionStage::Idle)), std::string("akira/connection/fail_generic"));
    CHECK_EQ(std::string(ConnectFailure::forRefusal(Kind::sign_in_expired)), std::string("akira/stream/psn_token_expired"));
    CHECK_EQ(std::string(ConnectFailure::forRefusal(Kind::busy)), std::string("akira/connection/fail_in_use"));
    CHECK_EQ(std::string(ConnectFailure::forRefusal(Kind::cancelled)), std::string("akira/connection/fail_generic"));
}

TEST(connect_failure_keys_by_outcome)
{
    using libchiaking::play::ConsoleQuit;
    using libchiaking::play::EndReason;
    using libchiaking::play::Outcome;

    CHECK_EQ(std::string(ConnectFailure::forOutcome(Outcome::not_started(EndReason::Cause::route_failed))),
             std::string("akira/connection/fail_unreachable"));
    CHECK_EQ(std::string(ConnectFailure::forOutcome(Outcome::not_started(EndReason::Cause::invalid_state))),
             std::string("akira/connection/fail_generic"));

    Outcome::Evidence quit;
    quit.quit = ConsoleQuit(ConsoleQuit::Reason::console_in_use);
    CHECK_EQ(std::string(ConnectFailure::forOutcome(Outcome(quit))), std::string("akira/connection/fail_in_use"));
    CHECK_EQ(std::string(ConnectFailure::quitReason(ConsoleQuit::Reason::console_shut_down)),
             std::string("akira/stream/quit_remote_shutdown"));
}

TEST(connect_failure_reads_seat_refusals)
{
    using libchiaking::play::SeatRefusal;
    const auto seat = libchiaking::controllers::SeatNumber::masked(1);
    CHECK_EQ(static_cast<int>(ConnectFailure::seatStatus(SeatRefusal::by_console(seat, 0x23))), 0x23);
    CHECK_EQ(static_cast<int>(ConnectFailure::seatStatus(SeatRefusal::by_console(seat, SeatRefusal::timed_out_status))),
             static_cast<int>(SeatRefusal::timed_out_status));
    CHECK_EQ(static_cast<int>(ConnectFailure::seatStatus(SeatRefusal::account_not_on_console(seat))), 1);
    CHECK_EQ(static_cast<int>(ConnectFailure::seatStatus(SeatRefusal::no_free_seat())), 0);
}
