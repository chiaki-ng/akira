#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/strings.hpp"
#include "play/couch_table.hpp"

namespace akira::play {

class SeatBoard {
public:
    enum class Tone { Neutral, Ready, Waiting, Failed };
    enum class Action { None, ChooseProfile, Retry, Invite };

    struct Row {
        std::uint8_t slot = 0;
        int player = 1;
        std::string device;
        std::string status;
        Tone tone = Tone::Neutral;
        Action action = Action::None;
        std::optional<PadId> pad;
        bool open = false;

        friend bool operator==(const Row&, const Row&) = default;
    };

    struct Candidate {
        PadId pad{};
        std::string label;

        friend bool operator==(const Candidate&, const Candidate&) = default;
    };

    SeatBoard(const CouchTable& table, const app::Strings& strings);

    auto rows() const -> std::vector<Row>;
    auto candidates() const -> std::vector<Candidate>;
    auto seated() const -> int;
    auto full() const -> bool;
    auto lowestOpen() const -> std::optional<std::uint8_t>;

    static auto needsAnotherAccount(const CouchSeat& seat) -> bool;
    static auto tone(SeatPhase phase) -> Tone;

private:
    struct Seated {
        PadId pad{};
        std::uint8_t slot = 0;
        std::string label;
    };

    auto status(int player, const CouchSeat& seat) const -> std::string;
    auto failure(const CouchSeat& seat) const -> std::string;
    auto withAccount(const std::string& text, std::uint8_t slot) const -> std::string;
    auto taken(std::uint8_t slot) const -> bool;
    auto hasProfiles(std::uint8_t slot) const -> bool;

    const CouchTable* table;
    const app::Strings* strings;
    std::vector<CouchTable::Pad> pads;
    std::vector<Seated> roster;
    std::string primaryLabel;
};

}
