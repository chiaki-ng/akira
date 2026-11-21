#include "play/ui/button_board_view.hpp"

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/callout_board.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <iterator>
#include <utility>

#include "play/combo_capture.hpp"
#include "play/remap_target.hpp"
#include "play/swipe_direction.hpp"

namespace akira::play::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace motion = ::arisa::motion;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

ButtonBoardView::ButtonBoardView(Mapping& mapping, const app::Strings& strings, bool art)
    : mapping(&mapping)
    , strings(&strings)
    , art(art)
{
}

auto ButtonBoardView::body(composition::Context& context) const -> composition::Element
{
    const Mapping::Status& status = context.read(mapping->view());
    const settings::ui::ControlOptions::Values& values = context.read(mapping->options().view());
    Mapping* held = mapping;
    composition::Binding<bool> capturing([held] { return held->view().get().capturing.has_value(); },
        [held](bool open) {
            if (!open)
                held->cancel();
        });
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 16.0F } },
        callouts().focus_section().id(composition::ElementKey{ "board.chips" }),
        touchpad(values),
        components::Button(strings->text("akira/remap/reset_all"), components::Image::SystemName{ "restart_alt" },
            [held] { held->resetAll(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .id(composition::ElementKey{ "board.reset" }))
        .sheet(std::move(capturing), [held, words = strings, status] {
            return captureSheet(held, words, status).presentation_sizing(composition::PresentationSizing::fitted);
        });
}

auto ButtonBoardView::callouts() const -> composition::Element
{
    if (!art)
        return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                      .spacing = theme::Space{ 16.0F } },
            column(ButtonBoard::Side::Leading), column(ButtonBoard::Side::Trailing));
    std::vector<structure::CalloutBoard::Callout> placed;
    for (const ButtonBoard::Target& target : ButtonBoard::targets())
        if (target.side != ButtonBoard::Side::Touchpad)
            placed.push_back(structure::CalloutBoard::Callout{ .chip = chip(target),
                .anchor = anchor(target.key),
                .side = target.side == ButtonBoard::Side::Leading ? structure::CalloutBoard::Side::leading
                                                                  : structure::CalloutBoard::Side::trailing });
    return structure::CalloutBoard(components::Image("img/ps5/dualsense.png").resizable().aspect_ratio(artAspect),
        std::move(placed),
        structure::CalloutBoard::Options{ .art = { .width = artWidth, .height = artWidth / artAspect },
            .column_width = chipWidth,
            .gap = leaderGap });
}

auto ButtonBoardView::anchor(std::uint32_t key) -> motion::UnitPoint
{
    switch (static_cast<RemapTarget>(key))
    {
        case RemapTarget::Cross: return { 0.809F, 0.407F };
        case RemapTarget::Moon: return { 0.884F, 0.296F };
        case RemapTarget::Box: return { 0.735F, 0.296F };
        case RemapTarget::Pyramid: return { 0.809F, 0.186F };
        case RemapTarget::Share: return { 0.264F, 0.128F };
        case RemapTarget::Ps: return { 0.495F, 0.495F };
        case RemapTarget::Options: return { 0.733F, 0.120F };
        case RemapTarget::L3: return { 0.337F, 0.500F };
        case RemapTarget::R3: return { 0.661F, 0.500F };
        case RemapTarget::L1: return { 0.238F, 0.067F };
        case RemapTarget::R1: return { 0.761F, 0.067F };
        case RemapTarget::L2: return { 0.171F, 0.033F };
        case RemapTarget::R2: return { 0.828F, 0.033F };
        case RemapTarget::Touchpad: return { 0.499F, 0.189F };
        case RemapTarget::DpadLeft: return { 0.130F, 0.295F };
        case RemapTarget::DpadRight: return { 0.243F, 0.295F };
        case RemapTarget::DpadUp: return { 0.186F, 0.212F };
        case RemapTarget::DpadDown: return { 0.186F, 0.376F };
    }
    return { 0.5F, 0.5F };
}

auto ButtonBoardView::column(ButtonBoard::Side side) const -> composition::Element
{
    std::vector<composition::Element> chips;
    for (const ButtonBoard::Target& target : ButtonBoard::targets())
        if (target.side == side)
            chips.push_back(chip(target));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 8.0F } }, std::move(chips))
        .frame({ .width = compactChipWidth });
}

auto ButtonBoardView::glyphs(const app::Strings& strings, const std::vector<std::uint64_t>& combo, float size)
    -> composition::Element
{
    if (combo.empty())
        return components::Text(strings.text("akira/remap/not_mapped"))
            .font(theme::Font::footnote)
            .line_limit(1)
            .foreground_style(theme::ColourRole::secondary_label);
    std::vector<composition::Element> parts;
    for (std::size_t index = 0; index < combo.size(); ++index)
    {
        if (index > 0)
            parts.push_back(components::Text("+").font(theme::Font::footnote));
        const std::string_view path = ButtonBoard::image(combo[index]);
        if (path.empty())
            parts.push_back(components::Text(std::string(ButtonBoard::label(combo[index]))).font(theme::Font::footnote));
        else
            parts.push_back(components::Image(std::string(path)).resizable().frame({ .width = size, .height = size }));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 3.0F } }, std::move(parts));
}

auto ButtonBoardView::chip(const ButtonBoard::Target& target) const -> composition::Element
{
    Mapping* held = mapping;
    const std::uint32_t key = target.key;
    const bool clash = mapping->board().conflicts(key);
    composition::Element shown = components::Button([held, key] { held->capture(key); },
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            components::Image(std::string(target.image)).resizable().frame({ .width = targetSize, .height = targetSize }),
            components::Text(strings->text(target.name)).line_limit(1),
            components::Spacer(),
            glyphs(*strings, mapping->board().mapping(key), glyphSize)))
                                     .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                                     .button_sizing(components::ButtonSizing::flexible)
                                     .on_command(provider::Button::x, strings->text("akira/remap/reset_one"),
                                         [held, key] { held->reset(key); }, composition::CommandOptions{ .hold = resetHold })
                                     .id(composition::ElementKey{ "board.chip." + std::string(target.name) });
    if (clash)
        shown = std::move(shown).background(theme::ColourValue(theme::ColourRole::red).opacity(0.3F), theme::Shape::capsule());
    return shown;
}

auto ButtonBoardView::enabled(std::uint32_t key, const settings::ui::ControlOptions::Values& values) const -> bool
{
    if (key == SWIPE_TOUCHPAD_UP)
        return values.swipeUp;
    if (key == SWIPE_TOUCHPAD_DOWN)
        return values.swipeDown;
    if (key == SWIPE_TOUCHPAD_LEFT)
        return values.swipeLeft;
    if (key == SWIPE_TOUCHPAD_RIGHT)
        return values.swipeRight;
    return values.touchpad;
}

auto ButtonBoardView::toggle(std::uint32_t key, const std::string& label, bool on) const -> composition::Element
{
    using Swipe = settings::ui::ControlOptions::Swipe;
    settings::ui::ControlOptions* options = &mapping->options();
    return controls::Toggle(label + strings->text("akira/remap/enabled_suffix"),
        composition::Binding<bool>([on] { return on; },
            [options, key](bool value) {
                if (key == SWIPE_TOUCHPAD_UP)
                    options->setSwipe(Swipe::Up, value);
                else if (key == SWIPE_TOUCHPAD_DOWN)
                    options->setSwipe(Swipe::Down, value);
                else if (key == SWIPE_TOUCHPAD_LEFT)
                    options->setSwipe(Swipe::Left, value);
                else if (key == SWIPE_TOUCHPAD_RIGHT)
                    options->setSwipe(Swipe::Right, value);
                else
                    options->setTouchpad(value);
            }))
        .id(composition::ElementKey{ "board.enabled." + std::to_string(key) });
}

auto ButtonBoardView::touchpad(const settings::ui::ControlOptions::Values& values) const -> composition::Element
{
    std::vector<composition::Element> chips;
    std::vector<composition::Element> toggles;
    for (const ButtonBoard::Target& target : ButtonBoard::targets())
    {
        if (target.side != ButtonBoard::Side::Touchpad)
            continue;
        chips.push_back(chip(target).frame({ .width = compactChipWidth }));
        toggles.push_back(toggle(target.key, strings->text(target.name), enabled(target.key, values)));
    }
    std::vector<composition::Element> leading(std::make_move_iterator(chips.begin()), std::make_move_iterator(chips.begin() + 3));
    std::vector<composition::Element> trailing(std::make_move_iterator(chips.begin() + 3), std::make_move_iterator(chips.end()));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 10.0F } },
        components::Text(strings->text("akira/settings/touchpad_heading"))
            .font(theme::Font::subheadline)
            .foreground_style(theme::ColourRole::secondary_label),
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                               .spacing = theme::Space{ 16.0F } },
            components::VStack(components::VStackOptions{ .spacing = theme::Space{ 8.0F } }, std::move(leading)),
            components::VStack(components::VStackOptions{ .spacing = theme::Space{ 8.0F } }, std::move(trailing)))
            .focus_section(),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 4.0F } },
            std::move(toggles)));
}

auto ButtonBoardView::captureSheet(Mapping* held, const app::Strings* strings, const Mapping::Status& status)
    -> composition::Element
{
    const ButtonBoard::Target* wanted = nullptr;
    for (const ButtonBoard::Target& target : ButtonBoard::targets())
        if (status.capturing && target.key == *status.capturing)
            wanted = &target;
    std::vector<composition::Element> row;
    for (const SwitchButton button : ComboCapture::buttons())
    {
        const std::string_view path = ButtonBoard::image(bits(button));
        if (path.empty() || button == SwitchButton::RightSL || button == SwitchButton::RightSR
            || (bits(button) >= bits(SwitchButton::StickLLeft) && bits(button) <= bits(SwitchButton::StickRDown)))
            continue;
        const bool lit = (status.peak & bits(button)) != 0;
        row.push_back(components::Image(std::string(path)).resizable().frame({ .width = 30.0F, .height = 30.0F }).opacity(lit ? 1.0F : 0.2F));
    }
    std::vector<std::uint64_t> combo;
    for (int bit = 0; bit < 64; ++bit)
        if ((status.peak & (std::uint64_t{ 1 } << bit)) != 0)
            combo.push_back(std::uint64_t{ 1 } << bit);
    composition::Element caught = combo.empty()
        ? composition::Element(components::Text(strings->text("akira/remap/waiting")).font(theme::Font::title3).foreground_style(theme::ColourRole::green))
        : glyphs(*strings, combo, 40.0F);
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 18.0F } },
        wanted ? composition::Element(components::Image(std::string(wanted->image)).resizable().frame({ .width = 72.0F, .height = 72.0F }))
               : composition::Element(components::Group()),
        components::Text(wanted ? strings->text("akira/remap/capture_prompt") + " " + strings->text(wanted->name) : std::string())
            .font(theme::Font::headline),
        std::move(caught),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 4.0F } }, std::move(row)),
        components::Text(strings->text("akira/remap/capture_hint"))
            .font(theme::Font::footnote)
            .foreground_style(theme::ColourRole::secondary_label))
        .padding(theme::Space{ 28.0F })
        .frame({ .width = 640.0F })
        .task([held] { return poll(held); })
        .id(composition::ElementKey{ "board.capture" });
}

auto ButtonBoardView::poll(Mapping* mapping) -> async::Task<void>
{
    while (mapping->view().get().capturing)
    {
        mapping->poll();
        co_await async::sleep(std::chrono::milliseconds{ 16 });
    }
}

}
