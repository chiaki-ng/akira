#pragma once

namespace akira::play {

class Volume {
public:
    static constexpr double step = 0.05;
    static constexpr double lowest = 0.0;
    static constexpr double highest = 1.0;

    Volume() = default;
    explicit Volume(double level);

    auto level() const -> double { return chosen; }

    friend bool operator==(const Volume&, const Volume&) = default;

private:
    double chosen = highest;
};

}
