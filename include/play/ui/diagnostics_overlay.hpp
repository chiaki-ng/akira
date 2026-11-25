#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>

#include "app/strings.hpp"
#include "play/diagnostics_appearance.hpp"
#include "play/diagnostics_report.hpp"
#include "play/stats_hud_placement.hpp"
#include "play/ui/stats_board.hpp"

namespace akira::play::ui {

class DiagnosticsOverlay final : public ::arisa::composition::Component {
public:
    class Sparkline {
    public:
        explicit Sparkline(std::vector<double> values);

        auto path(::arisa::composition::Rect rect) const -> ::arisa::graphics::Path;

        friend bool operator==(const Sparkline&, const Sparkline&) = default;

    private:
        std::vector<double> values;
    };

    class Marks {
    public:
        explicit Marks(std::vector<std::size_t> slots);

        auto path(::arisa::composition::Rect rect) const -> ::arisa::graphics::Path;

        friend bool operator==(const Marks&, const Marks&) = default;

    private:
        std::vector<std::size_t> slots;
    };

    using Tone = DiagnosticsReport::Tone;
    using Glass = DiagnosticsAppearance::Glass;

    static constexpr float stageWidth = 170.0F;
    static constexpr float stageSpacing = 6.0F;
    static constexpr float panelPadding = 12.0F;
    static constexpr std::size_t stageCount = 5;
    static constexpr float contentWidth = stageWidth * stageCount + stageSpacing * (stageCount - 1);
    static constexpr float sheetRadius = 20.0F;
    static constexpr float blockRadius = 14.0F;
    static constexpr float floatingPadding = 10.0F;
    static constexpr float sparklineHeight = 14.0F;
    static constexpr float stripHeight = 10.0F;
    static constexpr float rowScaleFloor = 0.7F;
    static constexpr const char* panelSpace = "stats-hud-panel";

    DiagnosticsOverlay(StatsBoard& board, StatsHudPlacement& placement, const app::Strings& strings);

    bool operator==(const DiagnosticsOverlay& other) const = default;

    auto body(::arisa::composition::Context& context) const -> ::arisa::composition::Element override;

    static auto toneColour(Tone tone) -> ::arisa::theme::ColourValue;
    static auto headlineColour(Tone tone) -> ::arisa::theme::ColourValue;
    static auto stageKey(DiagnosticsReport::StageKind kind) -> const char*;

private:
    auto compact(const DiagnosticsReport& report, const DiagnosticsAppearance& appearance) const
        -> ::arisa::composition::Element;
    auto detailed(const StatsBoard::View& view) const -> ::arisa::composition::Element;
    auto compactValue(std::string value, std::string unit, Tone tone) const -> ::arisa::composition::Element;
    auto header(const DiagnosticsReport& report, const DiagnosticsAppearance& appearance) const
        -> ::arisa::composition::Element;
    auto controls(const DiagnosticsAppearance& appearance) const -> ::arisa::composition::Element;
    auto levelChip(DiagnosticsAppearance::Level level, StatsHudPlacement::Tap tap, bool selected) const
        -> ::arisa::composition::Element;
    auto iconChip(const char* symbol, StatsHudPlacement::Tap tap) const -> ::arisa::composition::Element;
    auto targeted(::arisa::composition::Element control, StatsHudPlacement::Tap tap) const
        -> ::arisa::composition::Element;
    auto stage(const DiagnosticsReport::Stage& shown, const DiagnosticsAppearance& appearance) const
        -> ::arisa::composition::Element;
    auto strip(const DiagnosticsReport& report) const -> ::arisa::composition::Element;
    auto section(const DiagnosticsReport::Section& shown, float width, const DiagnosticsAppearance& appearance) const
        -> ::arisa::composition::Element;
    auto row(const DiagnosticsReport::Row& shown) const -> ::arisa::composition::Element;
    auto chip(const DiagnosticsReport::Chip& shown) const -> ::arisa::composition::Element;
    auto measured(::arisa::composition::Element panel) const -> ::arisa::composition::Element;
    static auto eventKey(DiagnosticsHistory::Event event) -> const char*;
    static auto floats(const DiagnosticsAppearance& appearance) -> bool;
    static auto block(::arisa::composition::Element content, const DiagnosticsAppearance& appearance, Tone tone,
        float padding, float fill) -> ::arisa::composition::Element;
    static auto surface(::arisa::composition::Element content, Glass glass, Tone tone, ::arisa::theme::Shape shape)
        -> ::arisa::composition::Element;
    static auto eventColour(DiagnosticsHistory::Event event) -> ::arisa::theme::ColourValue;
    static auto healthTint(Tone tone) -> ::arisa::theme::ColourValue;

    StatsBoard* board;
    StatsHudPlacement* placement;
    const app::Strings* strings;
};

}
