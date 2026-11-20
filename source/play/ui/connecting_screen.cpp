#include "play/ui/connecting_screen.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;
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
    const int steps = std::max(progress.steps, 1);
    const double done = static_cast<double>(activeSegment(progress.step, steps)) / static_cast<double>(segments);
    return controls::ProgressView(done, 1.0)
        .progress_view_style(controls::ProgressViewStyle::segmented(segments))
        .id(composition::ElementKey{ "connecting.progress" });
}

auto ConnectingScreen::body(composition::Context&) const -> composition::Element
{
    auto cancel = [commands = commands] { commands->cancel(); };
    composition::Element card =
        components::VStack(components::VStackOptions{ .spacing = theme::space::none },
            components::Text(strings->text("akira/connection/starting_stream"))
                .font(theme::Font::system(titleSize, theme::Font::Weight::semibold))
                .tracking(titleTracking),
            bar().padding(theme::Edge::top, theme::Space{ 24.0F }),
            components::Text(progress.status)
                .font(theme::Font::body)
                .foreground_style(theme::ColourValue(theme::ColourRole::label).opacity(0.72F))
                .padding(theme::Edge::top, theme::Space{ 15.0F })
                .id(composition::ElementKey{ "connecting.status" }),
            components::Text(strings->format("akira/connection/step_of", progress.step, progress.steps))
                .font(theme::Font::caption)
                .foreground_style(theme::ColourValue(theme::ColourRole::label).opacity(0.55F))
                .padding(theme::Edge::top, theme::Space{ 10.0F })
                .id(composition::ElementKey{ "connecting.step" }),
            composition::Element(components::Group()).frame({ .height = 32.0F }),
            components::Button(strings->text("akira/common/cancel"), components::ButtonRole::cancel, cancel)
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "connecting.cancel" }))
            .padding(theme::Insets::symmetric(theme::Space{ 56.0F }, theme::Space{ 42.0F }))
            .frame({ .min_width = cardWidth })
            .glass_panel(components::PanelRadius::modal);
    const graphics::Gradient glow(std::vector<theme::ColourValue>{
        theme::ColourValue(theme::ColourRole::accent).opacity(0.12F),
        theme::ColourValue(theme::ColourRole::accent).opacity(0.0F) });
    return components::VStack(std::move(card))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(graphics::ShapeStyle(graphics::EllipticalGradient{ .gradient = glow,
            .center = motion::UnitPoint{ 0.5F, 0.4F },
            .end_radius_fraction = 0.7F }))
        .background(theme::ColourRole::window_background)
        .on_exit_command(strings->text("akira/common/cancel"), cancel)
        .id(composition::ElementKey{ "connecting" });
}

}
