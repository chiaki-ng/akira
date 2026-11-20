#include "test_util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "accounts/ui/account_model.hpp"
#include "app/notifier.hpp"
#include "fake_account_desk.hpp"
#include "fake_couch_table.hpp"
#include "fake_strings.hpp"
#include "log/ui/log_shelf.hpp"
#include "play/ui/couch.hpp"

namespace {

using akira::accounts::AccountDesk;
using akira::accounts::ui::AccountModel;
using akira::play::CouchTable;
using akira::play::PadId;
using akira::play::SeatBoard;
using akira::play::SeatPhase;
using akira::play::ui::Couch;
using tests::FakeAccountDesk;
using tests::FakeCouchTable;

class RecordingNotifier final : public akira::app::Notifier {
public:
    void notify(std::string text) override { shown.push_back(std::move(text)); }

    std::vector<std::string> shown;
};

auto couchStrings() -> tests::FakeStrings
{
    return tests::FakeStrings({
        { "akira/couch/player_n", "Player {}" },
        { "akira/couch/player_ready", "Player {} · ready" },
        { "akira/couch/with_account", "{} · PSN: {}" },
        { "akira/couch/no_account", "{} · no PSN account configured" },
        { "akira/couch/joining", "Joining…" },
        { "akira/couch/failed_not_on_console", "Account is not registered on this PS5" },
        { "akira/couch/open_slot", "Open slot" },
        { "akira/couch/waiting_previous", "Waiting for the previous slot" },
    });
}

auto livingRoom() -> FakeCouchTable
{
    FakeCouchTable table;
    table.listed = {
        FakeCouchTable::pad(0, "Pro Controller", CouchTable::PadKind::SwitchPro, false, false),
        FakeCouchTable::pad(1, "DualSense", CouchTable::PadKind::PlayStation, true, true),
    };
    table.choices = { akira::play::CouchProfileChoice{ .profileId = 7, .label = "Couch guest" },
        akira::play::CouchProfileChoice{ .profileId = 8, .label = "mika" } };
    return table;
}

}

TEST(arisa_couch_roster_lists_the_primary_seats_and_open_slots)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    table.seatPad(1, 1, akira::play::CouchSeat{ .phase = SeatPhase::Joining, .accountLabel = "mika" });
    Couch couch(table, text);
    couch.refresh();
    const Couch::View& view = couch.view().get();
    REQUIRE(view.rows.size() == 4);
    CHECK(view.rows[0].device == "Pro Controller");
    CHECK(view.rows[0].status == "Player 1 · ready · PSN: kyogre_404");
    CHECK(view.rows[1].device == "DualSense");
    CHECK(view.rows[1].status == "Joining… · PSN: mika");
    CHECK(view.rows[1].pad == std::optional<PadId>(static_cast<PadId>(1)));
    CHECK(view.rows[2].open);
    CHECK(view.rows[2].action == SeatBoard::Action::Invite);
    CHECK(view.rows[3].status == "Waiting for the previous slot");
    CHECK(view.seated == 2);
    CHECK(!view.full);
}

TEST(arisa_couch_invite_on_ps5_chooses_a_profile_then_claims_a_pad)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    Couch couch(table, text);
    couch.refresh();
    couch.invite();
    CHECK(couch.view().get().mode == Couch::Mode::ChoosingProfile);
    CHECK(couch.view().get().profileSlot == 1);
    REQUIRE(couch.view().get().profiles.size() == 2);
    couch.selectProfile(8);
    CHECK(couch.view().get().mode == Couch::Mode::Claiming);
    CHECK(table.claims == 1);
    REQUIRE(couch.view().get().candidates.size() == 1);
    CHECK(couch.view().get().candidates.front().label == "DualSense");
    couch.joined(static_cast<PadId>(1), 1);
    CHECK(couch.view().get().mode == Couch::Mode::Roster);
    REQUIRE(table.added.size() == 1);
    CHECK(table.added.front().first == 1);
    CHECK(table.added.front().second == std::optional<std::int64_t>(8));
}

TEST(arisa_couch_invite_on_ps4_claims_straight_away)
{
    FakeCouchTable table = livingRoom();
    table.console5 = false;
    const tests::FakeStrings text = couchStrings();
    Couch couch(table, text);
    couch.invite();
    CHECK(couch.view().get().mode == Couch::Mode::Claiming);
    table.progress = 0.42F;
    couch.tick(Couch::Clock::now());
    CHECK(table.ticks == 1);
    CHECK(couch.view().get().claimPercent == 42);
    CHECK(couch.back());
    CHECK(couch.view().get().mode == Couch::Mode::Roster);
    CHECK(table.cancels >= 1);
    CHECK(!couch.back());
}

TEST(arisa_couch_refused_join_offers_another_profile)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    table.seatPad(1, 1, akira::play::CouchSeat{ .phase = SeatPhase::Failed,
        .refusal = libchiaking::play::SeatRefusal::account_not_on_console(libchiaking::controllers::SeatNumber::masked(1)),
        .profileId = 7 });
    Couch couch(table, text);
    couch.refresh();
    CHECK(couch.view().get().rows[1].action == SeatBoard::Action::ChooseProfile);
    CHECK(couch.view().get().rows[1].tone == SeatBoard::Tone::Failed);
    couch.chooseProfile(1, true);
    CHECK(couch.view().get().rejected);
    couch.selectProfile(8);
    REQUIRE(!table.removed.empty());
    CHECK(table.removed.back() == 1);
    CHECK(table.added.back().second == std::optional<std::int64_t>(8));
    CHECK(couch.view().get().mode == Couch::Mode::Roster);
}

TEST(arisa_couch_leave_releases_the_pad_and_the_seat)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    table.seatPad(1, 1, akira::play::CouchSeat{ .phase = SeatPhase::Playing });
    Couch couch(table, text);
    couch.refresh();
    couch.leave(static_cast<PadId>(1));
    REQUIRE(table.released.size() == 1);
    CHECK(table.released.front() == static_cast<PadId>(1));
    REQUIRE(table.removed.size() == 1);
    CHECK(table.removed.front() == 1);
}

TEST(arisa_couch_picker_selects_on_a_fresh_shoulder_press)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    Couch couch(table, text);
    const auto start = Couch::Clock::now();
    std::optional<Couch::Pick> picked;
    std::optional<PadId> chosen;
    table.picked = { 1 };
    couch.askPad(table.pads(), start, [&](Couch::Pick pick, std::optional<PadId> pad) {
        picked = pick;
        chosen = pad;
    });
    REQUIRE(couch.view().get().picker.has_value());
    couch.tick(start);
    CHECK(!couch.view().get().picker->selected.has_value());
    table.picked.clear();
    couch.tick(start);
    table.picked = { 1 };
    couch.tick(start);
    CHECK(couch.view().get().picker->selected == std::optional<std::size_t>(1));
    couch.acceptPad();
    CHECK(picked == std::optional<Couch::Pick>(Couch::Pick::Chosen));
    CHECK(chosen == std::optional<PadId>(static_cast<PadId>(1)));
    CHECK(!couch.view().get().picker.has_value());
}

TEST(arisa_couch_picker_cancels_and_times_out)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    Couch couch(table, text);
    std::vector<Couch::Pick> picks;
    const auto start = Couch::Clock::now();
    couch.askPad(table.pads(), start, [&](Couch::Pick pick, std::optional<PadId>) { picks.push_back(pick); });
    couch.cancelPad();
    couch.askPad(table.pads(), start, [&](Couch::Pick pick, std::optional<PadId>) { picks.push_back(pick); });
    couch.tick(start + std::chrono::seconds{ 31 });
    REQUIRE(picks.size() == 2);
    CHECK(picks[0] == Couch::Pick::Cancelled);
    CHECK(picks[1] == Couch::Pick::Default);
}

TEST(arisa_couch_arrival_and_passcode_report_back)
{
    FakeCouchTable table = livingRoom();
    const tests::FakeStrings text = couchStrings();
    Couch couch(table, text);
    std::optional<Couch::Arrival> answer;
    couch.arrived([&](Couch::Arrival given) { answer = given; });
    CHECK(couch.view().get().arrival);
    couch.answerArrival(Couch::Arrival::AddPlayer);
    CHECK(answer == std::optional<Couch::Arrival>(Couch::Arrival::AddPlayer));
    CHECK(!couch.view().get().arrival);

    int closed = 0;
    couch.askPasscode(2, true, [&] { ++closed; });
    REQUIRE(couch.view().get().passcode.has_value());
    CHECK(couch.view().get().passcode->retry);
    couch.askPasscode(3, false, [&] { ++closed; });
    CHECK(couch.view().get().passcode->slot == 2);
    couch.submitPasscode("1234");
    CHECK(closed == 1);
    REQUIRE(table.passcodes.size() == 1);
    CHECK(table.passcodes.front() == std::make_pair(std::uint8_t{ 2 }, std::string("1234")));
    couch.askPasscode(3, false, [&] { ++closed; });
    couch.cancelPasscode();
    CHECK(closed == 2);
    CHECK(table.removed.back() == 3);
}

TEST(arisa_account_model_reloads_profiles_and_secrets)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.reload();
    const AccountModel::View& view = model.view().get();
    CHECK(view.entries.size() == 2);
    CHECK(view.identity.accountId == "****Js");
    CHECK(view.credentials.size() == 8);
    REQUIRE(view.summary.has_value());
    CHECK(view.summary->trophies->level == 412);
    model.reveal(true);
    CHECK(model.view().get().identity.accountId == "aGVsbG93b3Js");
}

TEST(arisa_account_model_walks_the_qr_sign_in)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({ { "akira/qr_sign_in/signed_in_fmt", "Signed in as {}" } });
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.welcome(true);
    CHECK(model.view().get().flow == AccountModel::Flow::Welcome);
    model.startQr();
    CHECK(model.view().get().flow == AccountModel::Flow::Qr);
    CHECK(desk.creating);
    REQUIRE(desk.qr);
    desk.qr(AccountDesk::QrProgress{ .step = AccountDesk::QrStep::Waiting,
        .url = "https://my.account.sony.com/central/signin/qr?code=482113",
        .expires = std::chrono::system_clock::now() + std::chrono::seconds{ 299 } });
    CHECK(model.view().get().qr.code.has_value());
    CHECK(model.view().get().qr.secondsLeft >= 297);
    desk.qr(AccountDesk::QrProgress{ .step = AccountDesk::QrStep::Failed, .failureKey = "akira/qr_sign_in/status_declined" });
    CHECK(model.view().get().qr.failureKey == "akira/qr_sign_in/status_declined");
    CHECK(!model.view().get().qr.code.has_value());
    model.retryQr();
    desk.qr(AccountDesk::QrProgress{ .step = AccountDesk::QrStep::SignedIn, .onlineId = "kyogre_404" });
    CHECK(model.view().get().flow == AccountModel::Flow::None);
    REQUIRE(!notifier.shown.empty());
    CHECK(notifier.shown.back() == "Signed in as kyogre_404");
}

TEST(arisa_account_model_pairs_with_the_companion)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.reload();
    model.startCompanion();
    CHECK(model.view().get().companion.pairing.code == "4821");
    CHECK(model.view().get().companion.secondsLeft == AccountDesk::pairWindowSeconds);
    desk.paired(AccountDesk::PairStep::TimedOut);
    CHECK(model.view().get().companion.pairing.code == "7310");
    desk.paired(AccountDesk::PairStep::BadCode);
    CHECK(model.view().get().companion.step == AccountDesk::PairStep::BadCode);
    desk.paired(AccountDesk::PairStep::LockedOut);
    CHECK(model.view().get().companion.secondsLeft == -1);
    desk.paired(AccountDesk::PairStep::Imported);
    CHECK(model.view().get().flow == AccountModel::Flow::None);
    CHECK(desk.closedPairings >= 1);
}

TEST(arisa_account_model_creates_legacy_profiles)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.welcome(true);
    model.startLegacy();
    model.createLegacy("short", "");
    CHECK(model.view().get().legacyRejected);
    CHECK(model.view().get().flow == AccountModel::Flow::Legacy);
    model.createLegacy("aGVsbG93b3Js", "Couch");
    CHECK(model.view().get().flow == AccountModel::Flow::None);
    CHECK(model.view().get().entries.size() == 3);
}

TEST(arisa_account_model_switches_and_reopens_expired_sign_ins)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.reload();
    model.switchTo(1);
    CHECK(desk.switched.empty());
    desk.expired = true;
    model.switchTo(2);
    REQUIRE(desk.switched.size() == 1);
    CHECK(desk.switched.front() == 2);
    CHECK(model.view().get().flow == AccountModel::Flow::Qr);
}

TEST(arisa_account_model_removing_the_last_profile_returns_to_setup)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    int emptied = 0;
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text, .emptied = [&] { ++emptied; } });
    model.reload();
    model.askRemove(2);
    CHECK(model.view().get().removing == std::optional<std::int64_t>(2));
    model.confirmRemove();
    CHECK(model.view().get().entries.size() == 1);
    CHECK(emptied == 0);
    model.askRemove(1);
    model.confirmRemove();
    CHECK(emptied == 1);
    CHECK(model.view().get().flow == AccountModel::Flow::Welcome);
    CHECK(model.view().get().firstRun);
}

TEST(arisa_account_model_validates_ports_and_refreshes_tokens)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.reload();
    model.setCompanionPort("70000");
    CHECK(!notifier.shown.empty());
    CHECK(model.view().get().identity.companionPort == 9295);
    model.setCompanionPort("9300");
    CHECK(model.view().get().identity.companionPort == 9300);
    model.refreshToken();
    CHECK(desk.refreshes == 1);
    desk.refreshing = AccountDesk::Refresh{ .state = AccountDesk::RefreshState::NotLinked };
    const std::size_t before = notifier.shown.size();
    model.refreshToken();
    CHECK(desk.refreshes == 1);
    CHECK(notifier.shown.size() == before + 1);
    model.askClear();
    model.confirmClear();
    CHECK(desk.cleared == 1);
}

TEST(arisa_account_model_back_returns_to_the_right_place)
{
    FakeAccountDesk desk;
    RecordingNotifier notifier;
    const tests::FakeStrings text({});
    AccountModel model(AccountModel::Parts{ .desk = desk, .notifier = notifier, .strings = text });
    model.reload();
    model.welcome(true);
    model.startQr();
    model.back();
    CHECK(model.view().get().flow == AccountModel::Flow::Welcome);
    CHECK(desk.cancelledSignIns == 1);
    model.closeFlow();
    model.startCompanion();
    model.back();
    CHECK(model.view().get().flow == AccountModel::Flow::None);
}

TEST(arisa_log_shelf_walks_folders_and_files)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "akira-log-shelf";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "connections");
    std::ofstream(root / "akira.log") << "[INFO] started\n[ERROR] boom\nstack line\n";
    std::ofstream(root / "connections" / "stream.log") << "[DEBUG] takion v20\n";
    akira::log::ui::LogShelf shelf(root);
    shelf.open();
    REQUIRE(shelf.view().get().entries.has_value());
    REQUIRE(shelf.view().get().entries->size() == 2);
    CHECK(shelf.view().get().entries->front().directory);
    const akira::log::LogFolder::Entry file = shelf.view().get().entries->back();
    shelf.enter(file);
    REQUIRE(shelf.view().get().excerpt.has_value());
    REQUIRE(shelf.view().get().excerpt->blocks().size() == 2);
    CHECK(shelf.view().get().excerpt->blocks()[1].level == akira::log::LogExcerpt::Level::Error);
    CHECK(shelf.back());
    CHECK(!shelf.view().get().file.has_value());
    shelf.enter(shelf.view().get().entries->front());
    CHECK(!shelf.atRoot());
    CHECK(shelf.view().get().entries->size() == 1);
    CHECK(shelf.back());
    CHECK(shelf.atRoot());
    CHECK(!shelf.back());
    std::filesystem::remove_all(root);
}
