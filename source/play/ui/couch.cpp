#include "play/ui/couch.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

#include "log/log.hpp"

namespace akira::play::ui {

Couch::Couch(CouchTable& table, const app::Strings& strings)
    : seats(&table)
    , strings(&strings)
{
}

void Couch::refresh()
{
    const SeatBoard board(*seats, *strings);
    View next = state.get();
    next.rows = board.rows();
    next.seated = board.seated();
    next.full = board.full();
    next.candidates.clear();
    next.profiles.clear();
    next.claimPercent = 0;
    if (next.mode == Mode::Claiming)
    {
        next.candidates = board.candidates();
        next.claimPercent = static_cast<int>(seats->claimProgress() * 100.0F);
    }
    if (next.mode == Mode::ChoosingProfile)
        for (const CouchProfileChoice& offered : seats->profileChoices(next.profileSlot))
            next.profiles.push_back(Profile{ .id = offered.profileId, .label = seats->maskedName(offered.label) });
    next.picker.reset();
    if (choice)
        next.picker = Picker{ .pads = choice->pads(), .selected = choice->selected() };
    state.set(std::move(next));
}

void Couch::tick(PadChoice::Clock::time_point now)
{
    if (state.get().mode == Mode::Claiming)
        seats->tickClaim();
    if (choice)
    {
        if (now - lastRefresh >= PadChoice::refreshInterval)
        {
            lastRefresh = now;
            if (choice->refresh(seats->pads()))
                log::info("[COUCH] the pads changed under the picker, {} listed", choice->pads().size());
        }
        if (choice->sample([this](PadId pad) { return seats->picking(pad); }))
            log::info("[COUCH] picker selected pad {}", static_cast<std::uint32_t>(*choice->choice()));
        if (choice->idle(now))
        {
            log::warning("[COUCH] no pad choice in {}s, continuing", PadChoice::idleLimit.count());
            acceptPad();
            return;
        }
    }
    refresh();
}

void Couch::stopClaim()
{
    seats->cancelClaim();
    if (state.get().mode == Mode::Claiming)
        state.write(&View::mode, Mode::Roster);
}

void Couch::resetModes()
{
    profileRetry = false;
    pendingProfile = 0;
    state.update([](View& view) {
        view.mode = Mode::Roster;
        view.rejected = false;
        view.profileSlot = 0;
    });
}

void Couch::close()
{
    stopClaim();
    resetModes();
    refresh();
}

void Couch::reset()
{
    choice.reset();
    picked = {};
    answered = {};
    passcodeClosed = {};
    state.update([](View& view) {
        view.arrival = false;
        view.passcode.reset();
    });
    close();
}

auto Couch::back() -> bool
{
    switch (state.get().mode)
    {
        case Mode::Claiming:
            stopClaim();
            refresh();
            return true;
        case Mode::ChoosingProfile:
            resetModes();
            refresh();
            return true;
        case Mode::Roster: break;
    }
    return false;
}

void Couch::invite()
{
    const SeatBoard board(*seats, *strings);
    if (board.full())
        return;
    if (!seats->ps5())
    {
        claim();
        return;
    }
    if (const std::optional<std::uint8_t> slot = board.lowestOpen())
        chooseProfile(*slot, false);
}

void Couch::claim()
{
    if (state.get().mode == Mode::Claiming || SeatBoard(*seats, *strings).full())
        return;
    state.write(&View::mode, Mode::Claiming);
    seats->beginClaim();
    refresh();
}

void Couch::chooseProfile(std::uint8_t slot, bool retry)
{
    if (!seats->ps5() || slot == 0 || slot >= CouchTable::seatCount)
        return;
    stopClaim();
    profileRetry = retry;
    pendingProfile = 0;
    const bool rejected = retry && SeatBoard::needsAnotherAccount(seats->seat(slot));
    state.update([slot, rejected](View& view) {
        view.mode = Mode::ChoosingProfile;
        view.profileSlot = slot;
        view.rejected = rejected;
    });
    refresh();
}

void Couch::selectProfile(std::int64_t profile)
{
    if (state.get().mode != Mode::ChoosingProfile)
        return;
    const std::uint8_t slot = state.get().profileSlot;
    if (profileRetry)
    {
        seats->removePlayer(slot);
        if (!seats->addPlayer(slot, profile))
            return;
        resetModes();
        refresh();
        return;
    }
    resetModes();
    pendingProfile = profile;
    claim();
}

void Couch::joined(PadId pad, std::uint8_t slot)
{
    state.write(&View::mode, Mode::Roster);
    const bool ps5 = seats->ps5();
    const std::optional<std::int64_t> profile = ps5 ? std::optional<std::int64_t>(pendingProfile) : std::nullopt;
    if (!seats->addPlayer(slot, profile))
    {
        log::warning("[COUCH] player {} could not join on pad {}", slot + 1, static_cast<std::uint32_t>(pad));
        seats->release(pad);
        if (ps5)
            chooseProfile(slot, false);
        else
            refresh();
        return;
    }
    pendingProfile = 0;
    refresh();
}

void Couch::leave(PadId pad)
{
    const std::vector<SeatBoard::Row>& rows = state.get().rows;
    const auto seated = std::ranges::find(rows, std::optional<PadId>(pad), &SeatBoard::Row::pad);
    seats->release(pad);
    if (seated != rows.end() && seated->slot != 0)
        seats->removePlayer(seated->slot);
    refresh();
}

void Couch::retry(std::uint8_t slot)
{
    const bool ps5 = seats->ps5();
    const std::int64_t profile = seats->seat(slot).profileId;
    seats->removePlayer(slot);
    if (!seats->addPlayer(slot, ps5 ? std::optional<std::int64_t>(profile) : std::nullopt) && ps5)
    {
        chooseProfile(slot, true);
        return;
    }
    refresh();
}

void Couch::askPad(std::vector<CouchTable::Pad> pads, PadChoice::Clock::time_point now, Picked done)
{
    choice.emplace(std::move(pads), now);
    picked = std::move(done);
    lastRefresh = now;
    refresh();
}

void Couch::acceptPad()
{
    if (!choice)
        return;
    if (const std::optional<PadId> pad = choice->choice())
    {
        finishPick(Pick::Chosen, pad);
        return;
    }
    finishPick(Pick::Default, std::nullopt);
}

void Couch::cancelPad()
{
    if (choice)
        finishPick(Pick::Cancelled, std::nullopt);
}

void Couch::finishPick(Pick pick, std::optional<PadId> pad)
{
    Picked done = std::move(picked);
    picked = {};
    choice.reset();
    refresh();
    if (done)
        done(pick, pad);
}

void Couch::arrived(Answered done)
{
    answered = std::move(done);
    state.write(&View::arrival, true);
}

void Couch::answerArrival(Arrival answer)
{
    if (!state.get().arrival)
        return;
    Answered done = std::move(answered);
    answered = {};
    state.write(&View::arrival, false);
    if (done)
        done(answer);
}

void Couch::askPasscode(std::uint8_t slot, bool retry, Closed closed)
{
    if (state.get().passcode)
        return;
    passcodeClosed = std::move(closed);
    state.write(&View::passcode, std::optional<Passcode>(Passcode{ .slot = slot, .retry = retry }));
}

void Couch::submitPasscode(const std::string& passcode)
{
    const std::optional<Passcode> asked = state.get().passcode;
    if (!asked)
        return;
    Closed done = std::move(passcodeClosed);
    passcodeClosed = {};
    state.write(&View::passcode, std::optional<Passcode>());
    seats->sendPasscode(asked->slot, passcode);
    if (done)
        done();
}

void Couch::cancelPasscode()
{
    const std::optional<Passcode> asked = state.get().passcode;
    if (!asked)
        return;
    Closed done = std::move(passcodeClosed);
    passcodeClosed = {};
    state.write(&View::passcode, std::optional<Passcode>());
    seats->removePlayer(asked->slot);
    if (done)
        done();
}

}
