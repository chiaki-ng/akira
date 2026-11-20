#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

#include "play/touch_claim.hpp"

namespace akira::play {

class StatsHudPlacement final : public TouchClaim {
public:
    struct Point {
        float x = 0.0f;
        float y = 0.0f;

        friend bool operator==(const Point&, const Point&) = default;
    };

    struct Size {
        float width = 0.0f;
        float height = 0.0f;

        friend bool operator==(const Size&, const Size&) = default;
    };

    static constexpr float margin = 24.0f;
    static constexpr float slop = 12.0f;

    explicit StatsHudPlacement(std::optional<Fraction> saved = std::nullopt);

    void show(bool shown);
    void restore(std::optional<Fraction> saved);
    void measureScreen(Size measured);
    void measurePanel(Size measured);
    auto origin() const -> Point;
    auto revision() const -> std::uint64_t;
    auto takeSettled() -> std::optional<Fraction>;

    auto grab(Fraction at) -> bool override;
    void drag(Fraction at) override;
    void release() override;

private:
    auto placed() const -> Point;
    auto touched(Fraction at) const -> Point;
    void changed();

    mutable std::mutex lock;
    std::optional<Fraction> saved;
    std::optional<Fraction> settled;
    Size screen;
    Size panel;
    Point grabOffset;
    bool shown = false;
    bool dragging = false;
    std::uint64_t revisions = 0;
};

}
