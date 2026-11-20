#include "play/ui/pad_picker.hpp"

#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <utility>
#include <vector>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

PadPicker::PadPicker(Couch& couch, const app::Strings& strings)
    : couch(&couch)
    , strings(&strings)
{
}

auto PadPicker::cardWidth(std::size_t pads) -> float
{
    const float count = static_cast<float>(std::max<std::size_t>(pads, 1));
    const float budget = (panelWidth - 140.0F) / count - cardSpacing;
    return std::clamp(budget, minCard, maxCard);
}

auto PadPicker::slotLine(const CouchTable::Pad& pad) const -> std::string
{
    if (pad.handheld)
        return strings->text("akira/couch/handheld");
    return strings->format("akira/couch/player_n", pad.player);
}

auto PadPicker::capability(PadChoice::Capability shown) const -> std::string
{
    switch (shown)
    {
        case PadChoice::Capability::Touchpad: return strings->text("akira/couch/cap_touchpad");
        case PadChoice::Capability::Triggers: return strings->text("akira/couch/cap_triggers");
        case PadChoice::Capability::Motion: return strings->text("akira/couch/cap_motion");
        case PadChoice::Capability::Digital: break;
    }
    return strings->text("akira/couch/cap_digital");
}

auto PadPicker::chip(std::string text) -> composition::Element
{
    return components::Text(std::move(text))
        .font(theme::Font::caption)
        .foreground_style(theme::ColourRole::secondary_label)
        .padding(theme::Insets::symmetric(theme::Space{ 10.0F }, theme::Space{ 5.0F }))
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.08F), theme::Shape::capsule());
}

auto PadPicker::battery(int level) -> composition::Element
{
    static constexpr std::array<const char*, 5> glyphs{ "battery_0_bar", "battery_2_bar", "battery_3_bar", "battery_5_bar",
        "battery_full" };
    const std::size_t index = static_cast<std::size_t>(std::clamp(level, 0, 4));
    return components::Image(components::Image::SystemName{ glyphs[index] })
        .font(theme::Font::title3)
        .foreground_style(level <= 1 ? theme::ColourValue(theme::ColourRole::orange)
                                     : theme::ColourValue(theme::ColourRole::secondary_label));
}

auto PadPicker::card(const CouchTable::Pad& pad, bool selected, float width) const -> composition::Element
{
    std::vector<composition::Element> chips;
    for (const PadChoice::Capability shown : PadChoice::capabilities(pad))
        chips.push_back(chip(capability(shown)));
    std::vector<composition::Element> rows;
    rows.push_back(components::Image(pad.art)
                       .resizable()
                       .aspect_ratio(std::nullopt, composition::ContentMode::fit)
                       .frame({ .width = artSide, .height = artSide }));
    rows.push_back(components::Text(pad.label).font(theme::Font::title3).line_limit(1));
    rows.push_back(components::Text(slotLine(pad)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label));
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::xs }, std::move(chips)));
    if (!pad.batteries.empty())
    {
        std::vector<composition::Element> cells;
        for (const int level : pad.batteries)
            cells.push_back(battery(level));
        rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(cells)));
    }
    composition::Element shown = components::VStack(components::VStackOptions{ .spacing = theme::Space{ 8.0F } }, std::move(rows))
                                     .frame({ .width = width, .height = cardHeight })
                                     .glass_panel(components::PanelRadius::medium);
    if (selected)
        shown = std::move(shown).overlay(
            components::ShapeView(theme::Shape::rounded(18.0F)).stroke_border(theme::ColourRole::accent, 2.5F));
    return std::move(shown).id(composition::ElementKey{ std::format("pad-picker.card.{}", static_cast<std::uint32_t>(pad.id)) });
}

auto PadPicker::body(composition::Context& context) const -> composition::Element
{
    const Couch::View& view = context.read(couch->view());
    const Couch::Picker picker = view.picker.value_or(Couch::Picker{});
    const float width = cardWidth(picker.pads.size());
    std::vector<composition::Element> cards;
    for (std::size_t index = 0; index < picker.pads.size(); ++index)
        cards.push_back(card(picker.pads[index], picker.selected == index, width));
    Couch* held = couch;
    composition::Element panel =
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 14.0F } },
            components::Text(strings->text("akira/couch/choose_controller")).font(theme::Font::title).font_weight(theme::Font::Weight::semibold),
            components::Text(strings->text("akira/couch/picker_prompt")).font(theme::Font::body).foreground_style(theme::ColourRole::secondary_label),
            components::Divider(),
            components::HStack(components::HStackOptions{ .spacing = theme::Space{ cardSpacing } }, components::Spacer(),
                components::HStack(components::HStackOptions{ .spacing = theme::Space{ cardSpacing } }, std::move(cards)),
                components::Spacer())
                .frame({ .height = 280.0F, .max_width = composition::infinity }))
            .padding(theme::Insets{ .top = theme::Space{ 26.0F }, .leading = theme::Space{ 28.0F }, .bottom = theme::Space{ 22.0F },
                .trailing = theme::Space{ 28.0F } })
            .frame({ .width = panelWidth })
            .glass_panel(components::PanelRadius::large)
            .on_exit_command(strings->text("akira/common/back"), [held] { held->cancelPad(); })
            .id(composition::ElementKey{ "pad-picker" });
    if (picker.selected)
        panel = std::move(panel).on_command(provider::Button::a, strings->text("akira/couch/use_controller"),
            [held] { held->acceptPad(); });
    return std::move(panel)
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(scrim));
}

}
