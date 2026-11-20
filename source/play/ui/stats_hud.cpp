#include "play/ui/stats_hud.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <type_traits>
#include <utility>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

StatsHud::StatsHud(StatsBoard& board, StatsHudPlacement& placement, platform::video::OverlayPalette::Colours palette)
    : board(&board)
    , placement(&placement)
    , palette(palette)
{
}

auto StatsHud::colour(platform::video::OverlayPalette::Colour source) -> theme::Colour
{
    return theme::Colour{ .red = source.r, .green = source.g, .blue = source.b, .alpha = source.a };
}

auto StatsHud::ink(float alpha) const -> theme::ColourValue
{
    return colour(palette.text).with_alpha(alpha);
}

auto StatsHud::toned(StatsReadout::Tone tone) const -> theme::ColourValue
{
    switch (tone)
    {
        case StatsReadout::Tone::Warn: return colour(palette.warning);
        case StatsReadout::Tone::Bad: return colour(palette.danger);
        case StatsReadout::Tone::Good: break;
    }
    return ink(0.95F);
}

auto StatsHud::text(std::string shown, float size, theme::ColourValue shade) const -> composition::Element
{
    return components::Text(std::move(shown))
        .font(theme::Font::system(size).monospaced())
        .foreground_style(shade)
        .line_limit(1)
        .fixed_size();
}

auto StatsHud::dot(theme::ColourValue shade, float size) -> composition::Element
{
    return composition::Element(components::ShapeView(theme::Shape::capsule()).fill(shade)).frame({ .width = size, .height = size });
}

auto StatsHud::rule(theme::ColourValue shade, float width, float height) -> composition::Element
{
    composition::Element line = components::ShapeView(theme::Shape::rounded(0.0F)).fill(shade);
    if (width > 0.0F)
        return std::move(line).frame({ .width = width, .height = height });
    return std::move(line).frame({ .height = height, .max_width = composition::infinity });
}

auto StatsHud::panel(composition::Element content) const -> composition::Element
{
    return std::move(content)
        .background(colour(palette.backgroundDeep).with_alpha(0.58F), theme::Shape::rounded(radius))
        .overlay(components::ShapeView(theme::Shape::rounded(radius)).stroke_border(ink(0.15F), 1.0F))
        .fixed_size()
        .on_geometry_change(
            std::type_identity<composition::Size>{}, [](const composition::GeometryProxy& proxy) { return proxy.size(); },
            [placement = placement](const composition::Size& measured) {
                placement->measurePanel(StatsHudPlacement::Size{ .width = measured.width, .height = measured.height });
            });
}

auto StatsHud::compactCell(const StatsReadout::Cell& cell, bool first) const -> composition::Element
{
    std::vector<composition::Element> parts;
    parts.push_back(text(cell.label, 10.0F, ink(0.46F)).padding(theme::Edge::trailing, theme::Space{ 7.0F }));
    parts.push_back(text(cell.value, 14.0F, toned(cell.tone)));
    if (!cell.unit.empty())
        parts.push_back(text(cell.unit, 10.5F, ink(0.46F)).padding(theme::Edge::leading, theme::Space{ 3.0F }));
    composition::Element reading =
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::space::none },
            std::move(parts))
            .padding(theme::Insets{ .leading = theme::Space{ first ? 0.0F : compactInset },
                .trailing = theme::Space{ compactInset } });
    if (first)
        return reading;
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::space::none },
        rule(ink(0.10F), 1.0F, 20.0F), std::move(reading));
}

auto StatsHud::compactUpscale(const StatsReadout& readout) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::space::none },
            dot(colour(palette.success), 5.0F).padding(theme::Edge::trailing, theme::Space{ 4.0F }),
            text("FSR", 10.0F, colour(palette.accent)).padding(theme::Edge::trailing, theme::Space{ 7.0F }),
            text(readout.upscaleSummary(), 10.5F, ink(0.63F)))
            .frame({ .height = compactUpscaleHeight });
}

auto StatsHud::compact(const StatsReadout& readout) const -> composition::Element
{
    const StatsReadout::Tone overall = readout.overall();
    std::vector<composition::Element> line;
    line.push_back(dot(overall == StatsReadout::Tone::Good ? theme::ColourValue(colour(palette.success)) : toned(overall), 7.0F)
                       .padding(theme::Edge::trailing, theme::Space{ 13.0F }));
    bool first = true;
    for (const StatsReadout::Cell& cell : readout.compact())
    {
        line.push_back(compactCell(cell, first));
        first = false;
    }
    std::vector<composition::Element> rows;
    composition::Element cells = components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                                        .spacing = theme::space::none },
        std::move(line))
                                     .frame({ .height = compactHeight });
    if (readout.upscaled())
        cells = std::move(cells).overlay(composition::Alignment::bottom, rule(ink(0.10F), 0.0F, 1.0F));
    rows.push_back(std::move(cells));
    if (readout.upscaled())
        rows.push_back(compactUpscale(readout));
    return panel(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                        .spacing = theme::space::none },
        std::move(rows))
                     .padding(theme::Insets{ .leading = theme::Space{ compactInset }, .trailing = theme::Space{ 13.0F } }))
        .id(composition::ElementKey{ "stats-hud.compact" });
}

auto StatsHud::header(const StatsReadout& readout) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::space::none },
        text("AKIRA", 12.5F, colour(palette.accent)).padding(theme::Edge::trailing, theme::Space{ 9.0F }),
        text("/", 12.5F, ink(0.22F)).padding(theme::Edge::trailing, theme::Space{ 5.0F }),
        text(readout.context(), 12.5F, ink(0.72F)), components::Spacer(), text(readout.uptime(), 12.0F, ink(0.46F)))
        .padding(theme::Edge::horizontal, theme::Space{ fullInset })
        .frame({ .height = headerHeight });
}

auto StatsHud::row(const StatsReadout::Cell& cell) const -> composition::Element
{
    std::vector<composition::Element> parts;
    parts.push_back(text(cell.label, 11.5F, ink(0.46F)));
    parts.push_back(components::Spacer());
    parts.push_back(text(cell.value, 12.5F, toned(cell.tone)));
    if (!cell.unit.empty())
        parts.push_back(text(cell.unit, 10.0F, ink(0.46F)).padding(theme::Edge::leading, theme::Space{ 3.0F }));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::space::none },
        std::move(parts))
        .frame({ .height = rowHeight, .max_width = composition::infinity });
}

auto StatsHud::upscaleRow(const StatsReadout& readout) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::space::none },
        text("FSR", 11.5F, ink(0.46F)).padding(theme::Edge::trailing, theme::Space{ 5.0F }),
        dot(colour(palette.success), 5.0F), components::Spacer(),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::trailing,
                               .spacing = theme::space::none },
            text(readout.upscalePath(), 10.5F, colour(palette.accent)), text(readout.upscaleDetail(), 8.0F, ink(0.39F))))
        .frame({ .height = rowHeight, .max_width = composition::infinity });
}

auto StatsHud::column(std::string title, const std::vector<StatsReadout::Cell>& cells, const StatsReadout* upscaled) const
    -> composition::Element
{
    std::vector<composition::Element> rows;
    rows.push_back(text(std::move(title), 9.5F, ink(0.34F)).frame({ .height = 22.0F }));
    for (std::size_t index = 0; index < cells.size(); ++index)
    {
        rows.push_back(row(cells[index]));
        if (upscaled && index == 0)
            rows.push_back(upscaleRow(*upscaled));
    }
    const float width = (fullWidth - fullInset * 2.0F - columnGap * 2.0F) / 3.0F;
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::space::none },
        std::move(rows))
        .frame({ .width = width, .alignment = composition::Alignment::top_leading });
}

auto StatsHud::latency(const StatsReadout& readout) const -> composition::Element
{
    const float width = fullWidth - fullInset * 2.0F;
    const StatsReadout::Cell total = readout.latency();
    const StatsReadout::Bar shown = readout.bar();
    std::vector<composition::Element> bar;
    bar.push_back(composition::Element(components::Group())
                      .frame({ .width = width, .height = barHeight })
                      .background(ink(0.08F), theme::Shape::capsule()));
    if (shown.filled * width > 1.0F)
        bar.push_back(composition::Element(components::Group())
                          .frame({ .width = shown.filled * width, .height = barHeight })
                          .background(colour(palette.media).with_alpha(0.88F), theme::Shape::capsule()));
    if (shown.network * width > 1.0F)
        bar.push_back(composition::Element(components::Group())
                          .frame({ .width = shown.network * width, .height = barHeight })
                          .background(colour(palette.accent).with_alpha(0.92F), theme::Shape::capsule()));
    std::vector<composition::Element> legend;
    const std::vector<StatsReadout::Cell> keys = readout.legend();
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        if (index == 0)
            legend.push_back(dot(colour(palette.accent), 6.0F).padding(theme::Edge::trailing, theme::Space{ 4.0F }));
        if (index == 1)
            legend.push_back(dot(colour(palette.media), 6.0F).padding(theme::Edge::trailing, theme::Space{ 4.0F }));
        legend.push_back(text(keys[index].label, 9.5F, ink(0.39F)).padding(theme::Edge::trailing, theme::Space{ 5.0F }));
        legend.push_back(text(keys[index].value, 10.5F, ink(0.78F)).padding(theme::Edge::trailing, theme::Space{ 16.0F }));
    }
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::space::none },
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::space::none },
            text(total.label, 9.5F, ink(0.34F)), components::Spacer(), text(total.value, 13.0F, toned(total.tone)),
            text(total.unit, 10.0F, ink(0.46F)).padding(theme::Edge::leading, theme::Space{ 3.0F }))
            .frame({ .height = 18.0F }),
        components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::leading }, std::move(bar))
            .padding(theme::Edge::top, theme::Space{ 1.0F }),
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::space::none },
            std::move(legend))
            .frame({ .height = 20.0F }))
        .padding(theme::Insets{ .top = theme::Space{ 6.0F }, .leading = theme::Space{ fullInset },
            .bottom = theme::Space{ 4.0F }, .trailing = theme::Space{ fullInset } });
}

auto StatsHud::footer(const StatsReadout& readout) const -> composition::Element
{
    std::vector<composition::Element> parts;
    for (const StatsReadout::Cell& cell : readout.footer())
    {
        parts.push_back(text(cell.label, 10.5F, ink(0.46F)).padding(theme::Edge::trailing, theme::Space{ 6.0F }));
        parts.push_back(text(cell.value, 10.5F, ink(0.74F)).padding(theme::Edge::trailing, theme::Space{ 18.0F }));
    }
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::space::none },
        std::move(parts))
        .padding(theme::Edge::horizontal, theme::Space{ fullInset })
        .frame({ .height = footerHeight, .max_width = composition::infinity, .alignment = composition::Alignment::leading });
}

auto StatsHud::full(const StatsReadout& readout) const -> composition::Element
{
    const std::size_t rows = readout.upscaled() ? 5 : 4;
    const float columnHeight = 22.0F + rowHeight * static_cast<float>(rows);
    const auto divider = [this, columnHeight] {
        return rule(ink(0.10F), 1.0F, columnHeight - 4.0F)
            .padding(theme::Edge::horizontal, theme::Space{ columnGap / 2.0F - 0.5F });
    };
    composition::Element columns =
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                               .spacing = theme::space::none },
            column("REQUESTED", readout.requested(), nullptr), divider(),
            column("OUTPUT", readout.output(), readout.upscaled() ? &readout : nullptr), divider(),
            column("LINK", readout.link(), nullptr))
            .padding(theme::Insets{ .top = theme::Space{ 8.0F }, .leading = theme::Space{ fullInset },
                .bottom = theme::Space{ 10.0F }, .trailing = theme::Space{ fullInset } });
    return panel(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                        .spacing = theme::space::none },
        header(readout), rule(ink(0.10F), 0.0F, 1.0F), std::move(columns), rule(ink(0.10F), 0.0F, 1.0F), latency(readout),
        rule(ink(0.10F), 0.0F, 1.0F), footer(readout))
                     .frame({ .width = fullWidth }))
        .id(composition::ElementKey{ "stats-hud.full" });
}

auto StatsHud::body(composition::Context& context) const -> composition::Element
{
    const StatsBoard::View& view = context.read(board->view());
    if (view.mode == StatsOverlayMode::Off)
        return components::Group();
    composition::Element shown = view.mode == StatsOverlayMode::Compact ? compact(view.readout) : full(view.readout);
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::top_leading },
        std::move(shown).offset(view.origin.x, view.origin.y))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .on_geometry_change(
            std::type_identity<composition::Size>{}, [](const composition::GeometryProxy& proxy) { return proxy.size(); },
            [placement = placement](const composition::Size& measured) {
                placement->measureScreen(StatsHudPlacement::Size{ .width = measured.width, .height = measured.height });
            })
        .allows_hit_testing(false)
        .id(composition::ElementKey{ "stats-hud" });
}

}
