#include "play/ui/connecting_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

#include "app/arisa/ambient_background.hpp"
#include "app/arisa/brand_lockup.hpp"

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

ConnectingScreen::ConnectingScreen(Progress progress, StreamCommands& commands, const app::Strings& strings)
    : progress(std::move(progress))
    , commands(&commands)
    , strings(&strings)
{
}

auto ConnectingScreen::progressOf(const ConnectFlow::Ring& ring, std::string console, const app::Strings& strings)
    -> Progress
{
    const int steps = std::max(ring.total, 1);
    return Progress{
        .console = std::move(console),
        .status = ring.label.empty() && ring.labelKey ? strings.text(ring.labelKey) : ring.label,
        .step = std::clamp(ring.index, 1, steps),
        .steps = steps,
    };
}

auto ConnectingScreen::capsule(int index, int step) -> Capsule
{
    const int active = std::max(step, 1) - 1;
    if (index < active)
        return Capsule::Done;
    return index == active ? Capsule::Active : Capsule::Waiting;
}

auto ConnectingScreen::pill(Capsule state) -> composition::Element
{
    theme::ColourValue fill = theme::ColourValue(theme::ColourRole::secondary_label).opacity(0.28F);
    if (state == Capsule::Done)
        fill = theme::ColourValue(theme::Colour::hex(0xCEDD2A));
    else if (state == Capsule::Active)
        fill = theme::ColourValue(theme::ColourRole::accent);
    composition::Element shown = components::ShapeView(theme::Shape::capsule())
                                     .fill(fill)
                                     .frame({ .width = state == Capsule::Active ? activeCapsuleWidth : capsuleWidth,
                                         .height = capsuleHeight });
    if (state == Capsule::Active)
        return std::move(shown).shadow(theme::ColourValue(theme::ColourRole::accent).opacity(0.75F), capsuleGlow);
    return shown;
}

auto ConnectingScreen::bar() const -> composition::Element
{
    const int steps = std::max(progress.steps, 1);
    std::vector<composition::Element> pills;
    for (int index = 0; index < steps; ++index)
        pills.push_back(pill(capsule(index, progress.step))
                            .id(composition::ElementKey{ std::format("connecting.step.{}", index) }));
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ capsuleSpacing } }, std::move(pills))
        .accessibility_hidden(true)
        .id(composition::ElementKey{ "connecting.progress" });
}

auto ConnectingScreen::body(composition::Context&) const -> composition::Element
{
    auto cancel = [commands = commands] { commands->cancel(); };
    composition::Element card =
        components::VStack(components::VStackOptions{ .spacing = theme::space::none },
            composition::Element(app::arisa::BrandLockup(logoHeight, logoHeight * mascotScale, app::arisa::BrandLockup::Motion::Connecting))
                .padding(theme::Edge::bottom, theme::Space{ 30.0F }),
            components::Text(strings->text("akira/connection/starting_stream"))
                .font(theme::Font::system(titleSize, theme::Font::Weight::semibold))
                .tracking(titleTracking),
            bar().padding(theme::Edge::top, theme::Space{ 24.0F }),
            components::Text(progress.status)
                .font(theme::Font::callout)
                .foreground_style(theme::ColourValue(theme::ColourRole::label).opacity(0.72F))
                .padding(theme::Edge::top, theme::Space{ 15.0F })
                .id(composition::ElementKey{ "connecting.status" }),
            composition::Element(components::Group()).frame({ .height = 32.0F }),
            components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel, cancel)
                .button_style(components::ButtonStyle::plain())
                .foreground_style(theme::ColourValue(theme::ColourRole::label).opacity(0.78F))
                .id(composition::ElementKey{ "connecting.cancel" }))
            .padding(theme::Insets::symmetric(theme::Space{ 56.0F }, theme::Space{ 42.0F }))
            .frame({ .min_width = cardWidth })
            .glass_panel(components::PanelRadius::modal)
            .shadow(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(0.30F), 28.0F, 0.0F, 12.0F);
    return components::ZStack(
        composition::Element(app::arisa::AmbientBackground(app::arisa::AmbientBackground::Presence::Quiet))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity }),
        composition::Element(components::Group())
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(backdropDim))
            .allows_hit_testing(false),
        std::move(card))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourRole::window_background)
        .on_exit_command(strings->text("akira/common/cancel"), cancel)
        .id(composition::ElementKey{ "connecting" });
}

}
