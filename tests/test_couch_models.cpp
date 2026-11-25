#include "test_util.hpp"

#include <chrono>
#include <optional>
#include <vector>

#include "arisa/fake_couch_table.hpp"
#include "fake_strings.hpp"
#include "play/pad_choice.hpp"
#include "play/seat_board.hpp"

using akira::play::CouchTable;
using akira::play::PadChoice;
using akira::play::PadId;
using akira::play::SeatBoard;
using tests::FakeCouchTable;

namespace {

auto pads() -> std::vector<CouchTable::Pad>
{
    return {
        FakeCouchTable::pad(0, "Joy-Con", CouchTable::PadKind::JoyCon, false, false),
        FakeCouchTable::pad(1, "DualSense", CouchTable::PadKind::PlayStation, true, true),
    };
}

}

TEST(pad_choice_asks_only_when_the_pads_differ)
{
    CHECK(PadChoice::worthAsking(pads()));
    std::vector<CouchTable::Pad> same{ pads()[0], pads()[0] };
    same[1].id = static_cast<PadId>(1);
    CHECK(!PadChoice::worthAsking(same));
    CHECK(!PadChoice::worthAsking({ pads()[1] }));
    CHECK(PadChoice::fallback(pads()) == std::optional<PadId>(static_cast<PadId>(1)));
    CHECK(!PadChoice::fallback(same).has_value());
}

TEST(pad_choice_lists_capabilities)
{
    const std::vector<PadChoice::Capability> rich = PadChoice::capabilities(pads()[1]);
    CHECK(rich == std::vector<PadChoice::Capability>({ PadChoice::Capability::Touchpad, PadChoice::Capability::Triggers,
                      PadChoice::Capability::Motion }));
    CouchTable::Pad plain = pads()[0];
    plain.motion = false;
    CHECK(PadChoice::capabilities(plain) == std::vector<PadChoice::Capability>({ PadChoice::Capability::Digital }));
}

TEST(pad_choice_needs_a_release_before_a_press)
{
    PadChoice choice(pads(), PadChoice::Clock::now());
    CHECK(!choice.sample([](PadId pad) { return pad == static_cast<PadId>(1); }));
    CHECK(!choice.selected().has_value());
    CHECK(!choice.sample([](PadId) { return false; }));
    CHECK(choice.sample([](PadId pad) { return pad == static_cast<PadId>(1); }));
    CHECK(choice.choice() == std::optional<PadId>(static_cast<PadId>(1)));
}

TEST(pad_choice_carries_the_selection_across_a_refresh)
{
    PadChoice choice(pads(), PadChoice::Clock::now());
    static_cast<void>(choice.sample([](PadId) { return false; }));
    static_cast<void>(choice.sample([](PadId pad) { return pad == static_cast<PadId>(1); }));
    std::vector<CouchTable::Pad> moved = pads();
    moved[1].id = static_cast<PadId>(3);
    moved[1].vendor = 0x054c;
    moved[1].product = 0x0ce6;
    std::vector<CouchTable::Pad> before = pads();
    before[1].vendor = 0x054c;
    before[1].product = 0x0ce6;
    PadChoice tracked(before, PadChoice::Clock::now());
    static_cast<void>(tracked.sample([](PadId) { return false; }));
    static_cast<void>(tracked.sample([](PadId pad) { return pad == static_cast<PadId>(1); }));
    CHECK(tracked.refresh(moved));
    CHECK(tracked.choice() == std::optional<PadId>(static_cast<PadId>(3)));
    CHECK(!tracked.refresh(moved));
    CHECK(!tracked.idle(PadChoice::Clock::now()));
    CHECK(tracked.idle(PadChoice::Clock::now() + PadChoice::idleLimit));
}

TEST(seat_board_counts_seats_and_names_failures)
{
    FakeCouchTable table;
    table.console5 = false;
    table.listed = pads();
    table.choices = { akira::play::CouchProfileChoice{ .profileId = 7, .label = "mika" } };
    const tests::FakeStrings text({ { "akira/couch/failed_status", "refused ({})" } });
    table.seatPad(1, 2, akira::play::CouchSeat{ .phase = akira::play::SeatPhase::Failed,
        .refusal = libchiaking::play::SeatRefusal::by_console(libchiaking::controllers::SeatNumber::masked(2), 7) });
    const SeatBoard board(table, text);
    CHECK(board.seated() == 2);
    CHECK(!board.full());
    CHECK(board.lowestOpen() == std::optional<std::uint8_t>(1));
    const std::vector<SeatBoard::Row> rows = board.rows();
    REQUIRE(rows.size() == 4);
    CHECK(rows[1].slot == 2);
    CHECK(rows[1].action == SeatBoard::Action::Retry);
    CHECK(rows[1].status.starts_with("refused (7)"));
    CHECK(rows[2].slot == 1);
    CHECK(rows[2].action == SeatBoard::Action::Invite);
    CHECK(board.candidates().empty());
}

TEST(seat_board_without_profiles_explains_the_open_slot)
{
    FakeCouchTable table;
    table.listed = pads();
    const tests::FakeStrings text({ { "akira/couch/no_profiles_for_slot", "No other profile" } });
    const SeatBoard board(table, text);
    const std::vector<SeatBoard::Row> rows = board.rows();
    REQUIRE(rows.size() == 4);
    CHECK(rows[1].status == "No other profile");
    CHECK(rows[1].action == SeatBoard::Action::None);
    REQUIRE(board.candidates().size() == 1);
    CHECK(board.candidates().front().label == "DualSense");
}
