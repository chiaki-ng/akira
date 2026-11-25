#include "test_util.hpp"

#include "play/stream_controls.hpp"

#include <vector>

using akira::play::StatsOverlayMode;
using akira::play::StreamControls;
using Control = StreamControls::Control;
using Kind = StreamControls::Action::Kind;

TEST(stream_controls_list_players_only_when_offered)
{
    StreamControls controls;
    CHECK(controls.controls() == std::vector<Control>({Control::Power, Control::Stats, Control::Pad, Control::Motion}));
    controls.offerPlayers();
    CHECK(controls.controls()
          == std::vector<Control>({Control::Power, Control::Stats, Control::Pad, Control::Players, Control::Motion}));
    CHECK(controls.available(3));
    controls.enablePlayers(false);
    CHECK(!controls.available(3));
}

TEST(stream_controls_power_offers_sleep_only_when_allowed)
{
    StreamControls controls;
    CHECK_EQ(controls.optionCount(0), 2);
    controls.offerSleep(false);
    CHECK_EQ(controls.optionCount(0), 1);
    CHECK(!controls.offersSleep());
    CHECK_EQ(controls.optionCount(2), 0);
}

TEST(stream_controls_selection_wraps_and_skips_unavailable)
{
    StreamControls controls;
    controls.offerPlayers();
    controls.enablePlayers(false);
    controls.moveSelection(1);
    controls.moveSelection(1);
    CHECK_EQ(controls.selection(), 2u);
    controls.moveSelection(1);
    CHECK_EQ(controls.selection(), 4u);
    controls.moveSelection(1);
    CHECK_EQ(controls.selection(), 0u);
    controls.moveSelection(-1);
    CHECK_EQ(controls.selection(), 4u);
}

TEST(stream_controls_fire_opens_panel_then_chooses)
{
    StreamControls controls;
    controls.showStats(StatsOverlayMode::Compact);
    controls.moveSelection(1);
    StreamControls::Action first = controls.fire();
    CHECK(first.kind == Kind::None);
    CHECK(controls.inPanel());
    CHECK_EQ(controls.optionCursor(), 1);
    controls.moveOption(1);
    StreamControls::Action chosen = controls.fire();
    CHECK(chosen.kind == Kind::Choose);
    CHECK_EQ(chosen.control, 1u);
    CHECK_EQ(chosen.option, 2);
    CHECK(chosen.dismiss);
    CHECK(controls.stats() == StatsOverlayMode::Full);
}

TEST(stream_controls_power_choice_does_not_close)
{
    StreamControls controls;
    controls.enterPanel();
    CHECK_EQ(controls.optionCursor(), 0);
    controls.moveOption(-1);
    StreamControls::Action chosen = controls.fire();
    CHECK(chosen.kind == Kind::Choose);
    CHECK_EQ(chosen.option, 1);
    CHECK(!chosen.dismiss);
}

TEST(stream_controls_instant_controls_activate)
{
    StreamControls controls;
    controls.moveSelection(1);
    controls.moveSelection(1);
    StreamControls::Action pad = controls.fire();
    CHECK(pad.kind == Kind::Activate);
    CHECK_EQ(pad.control, 2u);
    CHECK(!pad.dismiss);
    controls.enterPanel();
    CHECK(!controls.inPanel());

    StreamControls::Action motion = controls.tapControl(3);
    CHECK(motion.kind == Kind::Activate);
    CHECK(motion.dismiss);
    CHECK_EQ(controls.selection(), 3u);
}

TEST(stream_controls_tapping_a_panel_control_opens_it)
{
    StreamControls controls;
    controls.showStats(StatsOverlayMode::Full);
    StreamControls::Action tapped = controls.tapControl(1);
    CHECK(tapped.kind == Kind::None);
    CHECK(controls.inPanel());
    CHECK_EQ(controls.selection(), 1u);
    CHECK_EQ(controls.optionCursor(), 2);

    StreamControls::Action option = controls.tapOption(0);
    CHECK(option.kind == Kind::Choose);
    CHECK_EQ(option.option, 0);
    CHECK(controls.stats() == StatsOverlayMode::Off);
    CHECK(!controls.tapControl(9).dismiss);
}

TEST(stream_controls_back_leaves_panel_before_dismissing)
{
    StreamControls controls;
    controls.enterPanel();
    CHECK(!controls.back());
    CHECK(!controls.inPanel());
    CHECK(controls.back());
}

TEST(stream_controls_unavailable_control_does_nothing)
{
    StreamControls controls;
    controls.offerPlayers();
    controls.enablePlayers(false);
    StreamControls::Action tapped = controls.tapControl(3);
    CHECK(tapped.kind == Kind::Activate);
    controls.moveSelection(0);
    CHECK(controls.fire().kind == Kind::None);
}
