#include "play/ui/diagnostics_overlay.hpp"

#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>
#include <utility>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace graphics = ::arisa::graphics;
namespace theme = ::arisa::theme;

DiagnosticsOverlay::Sparkline::Sparkline(std::vector<double> values)
    : values(std::move(values))
{
}

auto DiagnosticsOverlay::Sparkline::path(composition::Rect rect) const -> graphics::Path
{
    graphics::Path drawn;
    if (values.size() < 2)
        return drawn;
    const auto [lowest, highest] = std::ranges::minmax(values);
    const double span = std::max(highest - lowest, std::max(std::abs(highest) * 0.1, 0.001));
    const double step = rect.width / static_cast<double>(DiagnosticsHistory::capacity - 1);
    const double start = static_cast<double>(DiagnosticsHistory::capacity - values.size()) * step;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const composition::Point point{ .x = rect.x + static_cast<float>(start + static_cast<double>(index) * step),
            .y = rect.y + rect.height - 1.0F - static_cast<float>((values[index] - lowest) / span) * (rect.height - 2.0F) };
        if (index == 0)
            drawn.move(point);
        else
            drawn.add_line(point);
    }
    return drawn;
}

DiagnosticsOverlay::Marks::Marks(std::vector<std::size_t> slots)
    : slots(std::move(slots))
{
}

auto DiagnosticsOverlay::Marks::path(composition::Rect rect) const -> graphics::Path
{
    graphics::Path drawn;
    const float slot = rect.width / static_cast<float>(DiagnosticsHistory::capacity);
    for (const std::size_t index : slots)
        drawn.add_rect(composition::Rect{ .x = rect.x + static_cast<float>(index) * slot,
            .y = rect.y,
            .width = std::max(slot, 2.0F),
            .height = rect.height });
    return drawn;
}

DiagnosticsOverlay::DiagnosticsOverlay(StatsBoard& board, StatsHudPlacement& placement, const app::Strings& strings)
    : board(&board)
    , placement(&placement)
    , strings(&strings)
{
}

auto DiagnosticsOverlay::toneColour(Tone tone) -> theme::ColourValue
{
    switch (tone)
    {
        case Tone::Good: return theme::ColourRole::green;
        case Tone::Warning: return theme::ColourRole::yellow;
        case Tone::Bad: return theme::ColourRole::red;
        case Tone::Neutral: break;
    }
    return theme::Colour::hex(0xFFFFFF);
}

auto DiagnosticsOverlay::headlineColour(Tone tone) -> theme::ColourValue
{
    switch (tone)
    {
        case Tone::Good: return theme::Colour{ .red = 0.49F, .green = 0.88F, .blue = 0.75F, .alpha = 1.0F };
        case Tone::Warning: return theme::Colour{ .red = 0.97F, .green = 0.73F, .blue = 0.33F, .alpha = 1.0F };
        case Tone::Bad: return theme::Colour{ .red = 1.0F, .green = 0.66F, .blue = 0.66F, .alpha = 1.0F };
        case Tone::Neutral: break;
    }
    return theme::ColourRole::label;
}

auto DiagnosticsOverlay::eventColour(DiagnosticsHistory::Event event) -> theme::ColourValue
{
    switch (event)
    {
        case DiagnosticsHistory::Event::NetworkLoss: return theme::Colour{ .red = 0.61F, .green = 0.78F, .blue = 0.94F, .alpha = 1.0F };
        case DiagnosticsHistory::Event::RenderOverBudget: return theme::Colour{ .red = 0.89F, .green = 0.29F, .blue = 0.29F, .alpha = 1.0F };
        case DiagnosticsHistory::Event::DecodeError: return theme::Colour{ .red = 0.69F, .green = 0.55F, .blue = 0.95F, .alpha = 1.0F };
    }
    return theme::ColourRole::secondary_label;
}

auto DiagnosticsOverlay::eventKey(DiagnosticsHistory::Event event) -> const char*
{
    switch (event)
    {
        case DiagnosticsHistory::Event::NetworkLoss: return "akira/diagnostics/event_lost";
        case DiagnosticsHistory::Event::RenderOverBudget: return "akira/diagnostics/event_budget";
        case DiagnosticsHistory::Event::DecodeError: return "akira/diagnostics/event_decode";
    }
    return "akira/diagnostics/event_lost";
}

auto DiagnosticsOverlay::stageKey(DiagnosticsReport::StageKind kind) -> const char*
{
    switch (kind)
    {
        case DiagnosticsReport::StageKind::Network: return "akira/diagnostics/stage_network";
        case DiagnosticsReport::StageKind::Assembly: return "akira/diagnostics/stage_assembly";
        case DiagnosticsReport::StageKind::Decode: return "akira/diagnostics/stage_decode";
        case DiagnosticsReport::StageKind::Render: return "akira/diagnostics/stage_render";
        case DiagnosticsReport::StageKind::Display: return "akira/diagnostics/stage_display";
    }
    return "akira/diagnostics/stage_network";
}

auto DiagnosticsOverlay::healthTint(Tone tone) -> theme::ColourValue
{
    switch (tone)
    {
        case Tone::Warning: return theme::ColourValue(theme::ColourRole::orange).opacity(0.26F);
        case Tone::Bad: return theme::ColourValue(theme::ColourRole::red).opacity(0.3F);
        case Tone::Neutral:
        case Tone::Good: break;
    }
    return theme::ColourValue(theme::Colour::hex(0x000000)).opacity(0.2F);
}

auto DiagnosticsOverlay::surface(composition::Element content, Glass glass, Tone tone, theme::Shape shape)
    -> composition::Element
{
    switch (glass)
    {
        case Glass::Clear: return std::move(content).glass_effect(theme::Glass::clear(), shape);
        case Glass::ClearDimmed:
            return std::move(content)
                .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(0.32F), shape)
                .glass_effect(theme::Glass::clear(), shape);
        case Glass::HealthTint: return std::move(content).glass_effect(theme::Glass::regular().tint(healthTint(tone)), shape);
        case Glass::Solid:
            return std::move(content)
                .background(theme::Colour{ .red = 0.1F, .green = 0.1F, .blue = 0.1F, .alpha = 1.0F }, shape)
                .overlay(components::ShapeView(shape).stroke_border(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(0.12F), 0.5F));
        case Glass::Regular: break;
    }
    return std::move(content).glass_effect(theme::Glass::regular(), shape);
}

auto DiagnosticsOverlay::floats(const DiagnosticsAppearance& appearance) -> bool
{
    return appearance.layout == DiagnosticsAppearance::Layout::Floating
        && appearance.level != DiagnosticsAppearance::Level::Compact;
}

auto DiagnosticsOverlay::block(composition::Element content, const DiagnosticsAppearance& appearance, Tone tone, float padding,
    float fill) -> composition::Element
{
    const theme::Shape shape = theme::Shape::rounded(blockRadius);
    if (floats(appearance))
        return surface(components::ZStack(std::move(content))
                           .padding(theme::Space{ padding })
                           .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::top_leading }),
            appearance.glass, tone, shape);
    return components::ZStack(std::move(content))
        .padding(theme::Space{ padding })
        .background(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(fill), shape);
}

auto DiagnosticsOverlay::body(composition::Context& context) const -> composition::Element
{
    const StatsBoard::View& view = context.read(board->view());
    if (view.appearance.level == DiagnosticsAppearance::Level::Off)
        return components::Group();
    composition::Element panel = components::Group();
    if (view.appearance.level == DiagnosticsAppearance::Level::Compact)
        panel = surface(compact(view.report), view.appearance.glass, view.report.overallTone(), theme::Shape::capsule());
    else if (floats(view.appearance))
        panel = detailed(view);
    else
        panel = surface(detailed(view), view.appearance.glass, view.report.overallTone(), theme::Shape::rounded(sheetRadius));
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
        measured(std::move(panel)).offset(view.origin.x, view.origin.y))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .environment(components::Environment::colour_scheme, std::optional<theme::ColorScheme>(theme::ColorScheme::dark))
        .on_geometry_change(
            std::type_identity<composition::Size>{}, [](const composition::GeometryProxy& proxy) { return proxy.size(); },
            [placement = placement](const composition::Size& size) {
                placement->measureScreen(StatsHudPlacement::Size{ .width = size.width, .height = size.height });
            })
        .allows_hit_testing(false)
        .id(composition::ElementKey{ "stats-hud" });
}

auto DiagnosticsOverlay::measured(composition::Element panel) const -> composition::Element
{
    return components::ZStack(std::move(panel))
        .fixed_size()
        .on_geometry_change(
            std::type_identity<composition::Size>{}, [](const composition::GeometryProxy& proxy) { return proxy.size(); },
            [placement = placement](const composition::Size& size) {
                placement->measurePanel(StatsHudPlacement::Size{ .width = size.width, .height = size.height });
            });
}

auto DiagnosticsOverlay::compactValue(std::string value, std::string unit, Tone tone) const -> composition::Element
{
    std::vector<composition::Element> parts;
    parts.push_back(components::Text(std::move(value))
                        .font(theme::Font::system(14.0F, theme::Font::Weight::semibold).monospaced_digit())
                        .foreground_style(headlineColour(tone))
                        .line_limit(1));
    if (!unit.empty())
        parts.push_back(components::Text(std::move(unit)).font(theme::Font::caption2).foreground_style(theme::ColourRole::secondary_label));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::first_text_baseline,
                                  .spacing = theme::Space{ 3.0F } },
        std::move(parts));
}

auto DiagnosticsOverlay::compact(const DiagnosticsReport& report) const -> composition::Element
{
    std::vector<composition::Element> parts;
    parts.push_back(components::ZStack(composition::Element(components::Group())
                                           .frame({ .width = 8.0F, .height = 8.0F })
                                           .background(toneColour(report.compactTone()), theme::Shape::capsule())));
    if (report.rttPresent())
        parts.push_back(compactValue(report.rttValue(), std::string(), Tone::Neutral));
    parts.push_back(compactValue(report.lossValue(), strings->text("akira/diagnostics/unit_loss"), report.lossTone()));
    parts.push_back(compactValue(report.bitrateValue(), strings->text("akira/diagnostics/unit_mbps"), Tone::Neutral));
    if (const std::optional<DiagnosticsReport::Alert> alert = report.compactAlert())
        parts.push_back(components::Text(alert->headline
                                ? strings->text(stageKey(alert->kind)) + " " + alert->value + " " + strings->text(alert->label)
                                : strings->text(stageKey(alert->kind)) + " " + strings->text(alert->label) + " " + alert->value)
                            .font(theme::Font::callout)
                            .font_weight(theme::Font::Weight::semibold)
                            .foreground_style(headlineColour(alert->tone))
                            .line_limit(1));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::Space{ 14.0F } },
        std::move(parts))
        .padding(theme::Insets::symmetric(theme::Space{ 16.0F }, theme::Space{ 9.0F }))
        .fixed_size();
}

auto DiagnosticsOverlay::detailed(const StatsBoard::View& view) const -> composition::Element
{
    const DiagnosticsReport& report = view.report;
    const DiagnosticsAppearance& appearance = view.appearance;
    const bool floating = floats(appearance);
    const float inner = floating ? floatingPadding : 0.0F;
    std::vector<composition::Element> stages;
    for (const DiagnosticsReport::Stage& shown : report.stages())
        stages.push_back(stage(shown, appearance));
    std::vector<composition::Element> rows;
    rows.push_back(block(header(report), appearance, report.overallTone(), inner, 0.0F));
    rows.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                          .spacing = theme::Space{ stageSpacing } },
        std::move(stages)));
    rows.push_back(block(strip(report), appearance, Tone::Neutral, inner, 0.0F));
    if (appearance.level == DiagnosticsAppearance::Level::Full)
    {
        const std::vector<DiagnosticsReport::Section> sections = report.fullSections();
        const auto count = static_cast<float>(sections.size());
        const float width = (contentWidth - stageSpacing * (count - 1.0F)) / count;
        std::vector<composition::Element> cards;
        for (const DiagnosticsReport::Section& shown : sections)
            cards.push_back(section(shown, width, appearance));
        rows.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                              .spacing = theme::Space{ stageSpacing } },
            std::move(cards)));
    }
    const float outer = floating ? 0.0F : panelPadding;
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ floating ? stageSpacing : 8.0F } },
        std::move(rows))
        .padding(theme::Space{ outer })
        .frame({ .width = contentWidth + outer * 2.0F });
}

auto DiagnosticsOverlay::header(const DiagnosticsReport& report) const -> composition::Element
{
    std::vector<composition::Element> chips;
    for (const DiagnosticsReport::Chip& shown : report.headerChips())
        chips.push_back(chip(shown));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 6.0F } },
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::Space{ 8.0F } },
            components::Text(strings->text("akira/diagnostics/title")).font(theme::Font::headline),
            components::Text(report.uptime())
                .font(theme::Font::callout.monospaced_digit())
                .foreground_style(theme::ColourRole::secondary_label),
            components::Spacer()),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 6.0F } }, std::move(chips)));
}

auto DiagnosticsOverlay::chip(const DiagnosticsReport::Chip& shown) const -> composition::Element
{
    std::vector<composition::Element> parts;
    for (std::size_t index = 0; index < shown.segments.size(); ++index)
    {
        if (index > 0)
            parts.push_back(composition::Element(components::Group())
                                .frame({ .width = 1.0F, .height = 10.0F })
                                .background(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(0.2F)));
        parts.push_back(components::Text(shown.segments[index]).line_limit(1));
    }
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::Space{ 6.0F } },
        std::move(parts))
        .font(theme::Font::caption.monospaced_digit())
        .font_weight(theme::Font::Weight::medium)
        .foreground_style(headlineColour(shown.tone))
        .padding(theme::Insets::symmetric(theme::Space{ 8.0F }, theme::Space{ 3.0F }))
        .overlay(components::ShapeView(theme::Shape::capsule())
                     .stroke_border(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(0.2F), 0.5F));
}

auto DiagnosticsOverlay::stage(const DiagnosticsReport::Stage& shown, const DiagnosticsAppearance& appearance) const
    -> composition::Element
{
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(strings->text(stageKey(shown.kind)))
                        .font(theme::Font::caption)
                        .font_weight(theme::Font::Weight::medium)
                        .foreground_style(shown.tone == Tone::Bad ? toneColour(shown.tone)
                                                                  : theme::ColourValue(theme::ColourRole::secondary_label)));
    lines.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::first_text_baseline,
                                           .spacing = theme::Space{ 3.0F } },
        components::Text(shown.headline)
            .font(theme::Font::system(17.0F, theme::Font::Weight::semibold).monospaced_digit())
            .foreground_style(headlineColour(shown.tone))
            .line_limit(1),
        components::Text(strings->text(shown.unit))
            .font(theme::Font::caption2)
            .foreground_style(theme::ColourRole::secondary_label)
            .line_limit(1)));
    const theme::ColourValue line = shown.tone == Tone::Neutral
        ? theme::ColourValue(theme::Colour{ .red = 0.61F, .green = 0.78F, .blue = 0.94F, .alpha = 1.0F })
        : headlineColour(shown.tone);
    lines.push_back(composition::Element(components::ShapeView(Sparkline(shown.series)).stroke(line, 1.3F))
                        .frame({ .height = sparklineHeight, .max_width = composition::infinity }));
    for (const DiagnosticsReport::Row& listed : shown.rows)
        lines.push_back(row(listed));
    composition::Element card = block(components::ZStack(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                                                .spacing = theme::Space{ 3.0F } },
                                                            std::move(lines))
                                                            .padding(theme::Insets::symmetric(theme::Space{ 9.0F }, theme::Space{ 7.0F })))
                                          .frame({ .width = stageWidth, .alignment = composition::Alignment::leading }),
        appearance, shown.tone, 0.0F, 0.07F)
                                      .frame({ .width = stageWidth });
    if (shown.tone == Tone::Bad)
        card = std::move(card).overlay(components::ShapeView(theme::Shape::rounded(blockRadius)).stroke_border(toneColour(Tone::Bad), 1.0F));
    return card;
}

auto DiagnosticsOverlay::strip(const DiagnosticsReport& report) const -> composition::Element
{
    std::vector<composition::Element> layers;
    layers.push_back(composition::Element(components::Group())
                         .frame({ .height = 2.0F, .max_width = composition::infinity })
                         .background(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(0.18F)));
    for (const DiagnosticsHistory::Event event : DiagnosticsHistory::events)
        layers.push_back(composition::Element(components::ShapeView(Marks(report.marks(event))).fill(eventColour(event)))
                             .frame({ .height = stripHeight, .max_width = composition::infinity }));
    std::vector<composition::Element> parts;
    parts.push_back(components::Text(strings->text("akira/diagnostics/window"))
                        .font(theme::Font::caption2)
                        .foreground_style(theme::ColourRole::secondary_label));
    parts.push_back(components::ZStack(components::ZStackOptions{}, std::move(layers)).frame({ .height = stripHeight, .max_width = composition::infinity }));
    for (const DiagnosticsHistory::Event event : report.events())
        parts.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                               .spacing = theme::Space{ 4.0F } },
            components::ZStack(composition::Element(components::Group())
                                   .frame({ .width = 8.0F, .height = 8.0F })
                                   .background(eventColour(event), theme::Shape::rounded(2.0F))),
            components::Text(strings->text(eventKey(event))).line_limit(1))
                            .font(theme::Font::caption2)
                            .foreground_style(theme::ColourRole::secondary_label));
    parts.push_back(components::Text(strings->format("akira/diagnostics/latency_summary", report.latencySummary()))
                        .font(theme::Font::caption)
                        .foreground_style(theme::ColourRole::secondary_label)
                        .line_limit(1));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::Space{ 10.0F } },
        std::move(parts));
}

auto DiagnosticsOverlay::section(const DiagnosticsReport::Section& shown, float width,
    const DiagnosticsAppearance& appearance) const -> composition::Element
{
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(strings->text(shown.title))
                        .font(theme::Font::caption)
                        .font_weight(theme::Font::Weight::medium)
                        .foreground_style(theme::ColourRole::secondary_label)
                        .padding(theme::Edge::bottom, theme::Space{ 2.0F }));
    for (const DiagnosticsReport::Row& listed : shown.rows)
        lines.push_back(row(listed));
    return block(components::ZStack(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                           .spacing = theme::Space{ 3.0F } },
                                       std::move(lines))
                                       .padding(theme::Insets::symmetric(theme::Space{ 9.0F }, theme::Space{ 7.0F })))
                     .frame({ .width = width, .alignment = composition::Alignment::top_leading }),
        appearance, Tone::Neutral, 0.0F, 0.05F);
}

auto DiagnosticsOverlay::row(const DiagnosticsReport::Row& shown) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 6.0F } },
        components::Text(strings->text(shown.label)).foreground_style(theme::ColourRole::secondary_label).line_limit(1),
        components::Spacer(),
        components::Text(shown.keyed ? strings->text(shown.value) : shown.value)
            .font(theme::Font::caption.monospaced_digit())
            .foreground_style(headlineColour(shown.tone))
            .line_limit(1))
        .font(theme::Font::caption);
}

}
