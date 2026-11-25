#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "play/couch_table.hpp"

namespace akira::play {

class PadChoice {
public:
    enum class Capability { Touchpad, Triggers, Motion, Digital };

    using Clock = std::chrono::steady_clock;

    static constexpr int waitAttempts = 20;
    static constexpr std::chrono::milliseconds waitInterval{ 100 };
    static constexpr std::chrono::seconds idleLimit{ 30 };
    static constexpr std::chrono::milliseconds refreshInterval{ 500 };

    static auto worthAsking(const std::vector<CouchTable::Pad>& pads) -> bool;
    static auto fallback(const std::vector<CouchTable::Pad>& pads) -> std::optional<PadId>;
    static auto capabilities(const CouchTable::Pad& pad) -> std::vector<Capability>;
    static auto sameSet(const std::vector<CouchTable::Pad>& left, const std::vector<CouchTable::Pad>& right) -> bool;

    PadChoice(std::vector<CouchTable::Pad> pads, Clock::time_point now);

    auto pads() const -> const std::vector<CouchTable::Pad>& { return listed; }
    auto selected() const -> std::optional<std::size_t> { return chosen; }
    auto choice() const -> std::optional<PadId>;

    auto refresh(std::vector<CouchTable::Pad> fresh) -> bool;
    auto sample(const std::function<bool(PadId)>& picking) -> bool;
    auto idle(Clock::time_point now) const -> bool;

private:
    static auto differ(const CouchTable::Pad& left, const CouchTable::Pad& right) -> bool;
    auto carriedOver(const std::optional<CouchTable::Pad>& before) const -> std::optional<std::size_t>;

    std::vector<CouchTable::Pad> listed;
    std::vector<bool> released;
    std::optional<std::size_t> chosen;
    Clock::time_point opened;
};

}
