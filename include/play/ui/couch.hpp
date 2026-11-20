#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "app/strings.hpp"
#include "play/couch_table.hpp"
#include "play/pad_choice.hpp"
#include "play/seat_board.hpp"

namespace akira::play::ui {

class Couch {
public:
    using Clock = PadChoice::Clock;

    enum class Mode { Roster, Claiming, ChoosingProfile };
    enum class Pick { Chosen, Default, Cancelled };
    enum class Arrival { AddPlayer, NotNow, Dismissed };

    struct Profile {
        std::int64_t id = 0;
        std::string label;

        friend bool operator==(const Profile&, const Profile&) = default;
    };

    struct Picker {
        std::vector<CouchTable::Pad> pads;
        std::optional<std::size_t> selected;

        friend bool operator==(const Picker&, const Picker&) = default;
    };

    struct Passcode {
        std::uint8_t slot = 0;
        bool retry = false;

        friend bool operator==(const Passcode&, const Passcode&) = default;
    };

    struct View {
        Mode mode = Mode::Roster;
        std::vector<SeatBoard::Row> rows;
        std::vector<SeatBoard::Candidate> candidates;
        std::vector<Profile> profiles;
        int seated = 1;
        bool full = false;
        int claimPercent = 0;
        std::uint8_t profileSlot = 0;
        bool rejected = false;
        std::optional<Picker> picker;
        bool arrival = false;
        std::optional<Passcode> passcode;

        friend bool operator==(const View&, const View&) = default;
    };

    using Picked = std::function<void(Pick, std::optional<PadId>)>;
    using Answered = std::function<void(Arrival)>;
    using Closed = std::function<void()>;

    Couch(CouchTable& table, const app::Strings& strings);

    auto view() const -> const ::arisa::composition::Observable<View>& { return state; }
    auto table() const -> CouchTable& { return *seats; }

    void refresh();
    void tick(PadChoice::Clock::time_point now);
    void close();
    void reset();
    [[nodiscard]] auto back() -> bool;

    void invite();
    void claim();
    void chooseProfile(std::uint8_t slot, bool retry);
    void selectProfile(std::int64_t profile);
    void joined(PadId pad, std::uint8_t slot);
    void leave(PadId pad);
    void retry(std::uint8_t slot);

    void askPad(std::vector<CouchTable::Pad> pads, PadChoice::Clock::time_point now, Picked picked);
    void acceptPad();
    void cancelPad();

    void arrived(Answered answered);
    void answerArrival(Arrival answer);

    void askPasscode(std::uint8_t slot, bool retry, Closed closed);
    void submitPasscode(const std::string& passcode);
    void cancelPasscode();

private:
    void stopClaim();
    void resetModes();
    void finishPick(Pick pick, std::optional<PadId> pad);

    CouchTable* seats;
    const app::Strings* strings;
    ::arisa::composition::Observable<View> state;
    std::optional<PadChoice> choice;
    Picked picked;
    Answered answered;
    Closed passcodeClosed;
    PadChoice::Clock::time_point lastRefresh{};
    bool profileRetry = false;
    std::int64_t pendingProfile = 0;
};

}
