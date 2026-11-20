#include "play/seat_board.hpp"

#include <algorithm>
#include <ranges>

namespace akira::play {

SeatBoard::SeatBoard(const CouchTable& table, const app::Strings& strings)
    : table(&table)
    , strings(&strings)
    , pads(table.pads())
    , primaryLabel(strings.format("akira/couch/player_n", 1))
{
    for (const CouchTable::Pad& pad : pads)
    {
        if (pad.primary)
            primaryLabel = pad.label;
        if (pad.seat && *pad.seat != 0)
            roster.push_back(Seated{ .pad = pad.id, .slot = *pad.seat, .label = pad.label });
    }
    std::ranges::sort(roster, {}, &Seated::slot);
}

auto SeatBoard::needsAnotherAccount(const CouchSeat& seat) -> bool
{
    return seat.phase == SeatPhase::Failed && seat.refusal
        && seat.refusal->kind() == libchiaking::play::SeatRefusal::Kind::account_not_on_console;
}

auto SeatBoard::tone(SeatPhase phase) -> Tone
{
    switch (phase)
    {
        case SeatPhase::Playing: return Tone::Ready;
        case SeatPhase::Failed: return Tone::Failed;
        case SeatPhase::Passcode: return Tone::Waiting;
        case SeatPhase::Empty:
        case SeatPhase::Joining: break;
    }
    return Tone::Neutral;
}

auto SeatBoard::failure(const CouchSeat& seat) const -> std::string
{
    using Kind = libchiaking::play::SeatRefusal::Kind;
    if (seat.dropped)
        return strings->text("akira/couch/failed_released");
    if (!seat.refusal)
        return strings->text("akira/couch/failed_refused");
    switch (seat.refusal->kind())
    {
        case Kind::timed_out: return strings->text("akira/couch/failed_timed_out");
        case Kind::account_not_on_console: return strings->text("akira/couch/failed_not_on_console");
        case Kind::passcode_attempts_used: return strings->text("akira/couch/failed_passcodes");
        case Kind::not_sent: return strings->text("akira/couch/failed_not_sent");
        case Kind::no_free_seat: return strings->text("akira/couch/failed_no_seat");
        case Kind::refused_by_console: break;
    }
    return strings->format("akira/couch/failed_status", static_cast<int>(seat.refusal->console_status().value_or(0)));
}

auto SeatBoard::status(int player, const CouchSeat& seat) const -> std::string
{
    switch (seat.phase)
    {
        case SeatPhase::Playing: return strings->format("akira/couch/player_ready", player);
        case SeatPhase::Passcode: return strings->text("akira/couch/passcode_wait");
        case SeatPhase::Failed: return failure(seat);
        case SeatPhase::Empty:
        case SeatPhase::Joining: break;
    }
    return strings->text("akira/couch/joining");
}

auto SeatBoard::withAccount(const std::string& text, std::uint8_t slot) const -> std::string
{
    if (!table->ps5())
        return text;
    const std::string account = table->accountLabel(slot);
    if (account.empty())
        return strings->format("akira/couch/no_account", text);
    return strings->format("akira/couch/with_account", text, account);
}

auto SeatBoard::taken(std::uint8_t slot) const -> bool
{
    return std::ranges::find(roster, slot, &Seated::slot) != roster.end();
}

auto SeatBoard::hasProfiles(std::uint8_t slot) const -> bool
{
    return !table->ps5() || !table->profileChoices(slot).empty();
}

auto SeatBoard::lowestOpen() const -> std::optional<std::uint8_t>
{
    for (std::uint8_t slot = 1; slot < CouchTable::seatCount; ++slot)
        if (!taken(slot))
            return slot;
    return std::nullopt;
}

auto SeatBoard::seated() const -> int
{
    return 1 + static_cast<int>(roster.size());
}

auto SeatBoard::full() const -> bool
{
    return roster.size() >= CouchTable::seatCount - 1U;
}

auto SeatBoard::rows() const -> std::vector<Row>
{
    std::vector<Row> shown;
    shown.push_back(Row{ .slot = 0,
        .player = 1,
        .device = primaryLabel,
        .status = withAccount(strings->format("akira/couch/player_ready", 1), 0),
        .tone = Tone::Ready });
    for (const Seated& seated : roster)
    {
        const CouchSeat seat = table->seat(seated.slot);
        const int player = seated.slot + 1;
        Action action = Action::None;
        if (needsAnotherAccount(seat))
            action = Action::ChooseProfile;
        else if (seat.phase == SeatPhase::Failed)
            action = Action::Retry;
        shown.push_back(Row{ .slot = seated.slot,
            .player = player,
            .device = seated.label,
            .status = withAccount(status(player, seat), seated.slot),
            .tone = tone(seat.phase),
            .action = action,
            .pad = seated.pad });
    }
    const std::optional<std::uint8_t> invite = lowestOpen();
    for (std::uint8_t slot = 1; slot < CouchTable::seatCount; ++slot)
    {
        if (taken(slot))
            continue;
        const bool profiles = hasProfiles(slot);
        Row open{ .slot = slot, .player = slot + 1, .device = strings->text("akira/couch/open_slot"), .open = true };
        if (!profiles)
            open.status = strings->text("akira/couch/no_profiles_for_slot");
        else if (slot == invite)
            open.action = Action::Invite;
        else
            open.status = strings->text("akira/couch/waiting_previous");
        shown.push_back(std::move(open));
    }
    return shown;
}

auto SeatBoard::candidates() const -> std::vector<Candidate>
{
    std::vector<Candidate> shown;
    for (const CouchTable::Pad& pad : pads)
        if (pad.claimable && !pad.seat)
            shown.push_back(Candidate{ .pad = pad.id, .label = pad.label });
    return shown;
}

}
