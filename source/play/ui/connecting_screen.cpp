#include "play/ui/connecting_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <utility>
#include <vector>

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

auto ConnectingScreen::activeSegment(int step, int steps) -> int
{
    if (steps <= 0)
        return 0;
    return std::clamp(((step - 1) * segments) / steps, 0, segments - 1);
}

auto ConnectingScreen::bar() const -> composition::Element
{
    const int active = activeSegment(progress.step, progress.steps);
    std::vector<composition::Element> pieces;
    for (int index = 0; index < segments; ++index)
    {
        theme::ColourValue fill = theme::ColourRole::quaternary_label;
        if (index < active)
            fill = theme::ColourRole::green;
        else if (index == active)
            fill = theme::ColourRole::accent;
        pieces.push_back(composition::Element(components::Group())
                             .frame({ .width = index == active ? 34.0F : 26.0F, .height = 6.0F })
                             .background(fill, theme::Shape::capsule())
                             .id(composition::ElementKey{ index == active ? "connecting.segment.active"
                                                                           : "connecting.segment" }));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 9.0F } }, std::move(pieces));
}

auto ConnectingScreen::body(composition::Context&) const -> composition::Element
{
    auto cancel = [commands = commands] { commands->cancel(); };
    composition::Element panel =
        components::VStack(components::VStackOptions{ .spacing = theme::space::none },
            components::Text(progress.console)
                .font(theme::Font::subheadline)
                .foreground_style(theme::ColourRole::secondary_label)
                .id(composition::ElementKey{ "connecting.console" }),
            components::Text(strings->text("akira/connection/starting_stream"))
                .font(theme::Font::title)
                .padding(theme::Edge::top, theme::Space{ 6.0F }),
            bar().padding(theme::Edge::top, theme::Space{ 24.0F }),
            components::Text(progress.status)
                .font(theme::Font::callout)
                .foreground_style(theme::ColourRole::secondary_label)
                .padding(theme::Edge::top, theme::Space{ 15.0F })
                .id(composition::ElementKey{ "connecting.status" }),
            components::Text(strings->format("akira/connection/step_of", progress.step, progress.steps))
                .font(theme::Font::footnote)
                .foreground_style(theme::ColourRole::tertiary_label)
                .padding(theme::Edge::top, theme::Space{ 4.0F })
                .id(composition::ElementKey{ "connecting.step" }),
            components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel, cancel)
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .padding(theme::Edge::top, theme::Space{ 32.0F })
                .id(composition::ElementKey{ "connecting.cancel" }))
            .padding(theme::Insets::symmetric(theme::Space{ 56.0F }, theme::Space{ 42.0F }))
            .glass_effect(theme::Glass::regular(), theme::Shape::rounded(28.0F));
    return components::VStack(std::move(panel))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourRole::window_background)
        .on_exit_command(strings->text("akira/common/cancel"), cancel)
        .id(composition::ElementKey{ "connecting" });
}

}
