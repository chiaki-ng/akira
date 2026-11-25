#pragma once

#include <array>

namespace akira::play {

struct DiagnosticsAppearance {
    enum class Level { Off, Compact, Standard, Full };
    enum class Glass { Regular, Clear, ClearDimmed, HealthTint, Solid };
    enum class Layout { Sheet, Floating };

    static constexpr std::array levels{ Level::Off, Level::Compact, Level::Standard, Level::Full };
    static constexpr std::array glasses{ Glass::Regular, Glass::Clear, Glass::ClearDimmed, Glass::HealthTint, Glass::Solid };
    static constexpr std::array layouts{ Layout::Sheet, Layout::Floating };

    Level level = Level::Off;
    Glass glass = Glass::Regular;
    Layout layout = Layout::Sheet;

    friend bool operator==(const DiagnosticsAppearance&, const DiagnosticsAppearance&) = default;
};

}
