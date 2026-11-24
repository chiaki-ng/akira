#include "play/ui/connecting_screen.hpp"

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/unit_point.hpp>
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
#include "app/arisa/artwork.hpp"
#include "app/arisa/brand_lockup.hpp"

namespace akira::play::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;

ConnectingScreen::ConnectingScreen(Progress progress, StreamCommands& commands, const app::Strings& strings, std::vector<Art> arts)
    : progress(std::move(progress))
    , commands(&commands)
    , strings(&strings)
    , arts(std::move(arts))
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

auto ConnectingScreen::operation() const -> composition::Element
{
    if (progress.activity.empty())
        return components::Group();
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(progress.activity)
                        .font(theme::Font::subheadline)
                        .foreground_style(theme::ColourRole::label)
                        .id(composition::ElementKey{ "connecting.activity" }));
    if (!progress.details.empty())
        lines.push_back(components::Text(progress.details)
                            .font(theme::Font::footnote)
                            .foreground_style(theme::ColourRole::secondary_label)
                            .multiline_text_alignment(composition::TextAlignment::center)
                            .frame({ .max_width = detailWidth })
                            .id(composition::ElementKey{ "connecting.details" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 4.0F } }, std::move(lines))
        .padding(theme::Edge::top, theme::Space{ 12.0F });
}

auto ConnectingScreen::stagesShown() const -> composition::Element
{
    return components::VStack(components::VStackOptions{ .spacing = theme::space::none },
        bar().padding(theme::Edge::top, theme::Space{ 24.0F }),
        components::Text(progress.status)
            .font(theme::Font::callout)
            .foreground_style(theme::ColourValue(theme::ColourRole::label).opacity(0.72F))
            .padding(theme::Edge::top, theme::Space{ 15.0F })
            .id(composition::ElementKey{ "connecting.status" }),
        operation());
}

auto ConnectingScreen::logShown() const -> composition::Element
{
    std::vector<composition::Element> lines;
    const std::size_t first = progress.log.size() > logRows ? progress.log.size() - logRows : 0;
    for (std::size_t index = first; index < progress.log.size(); ++index)
        lines.push_back(components::Text(progress.log[index])
                            .font(theme::Font::caption.monospaced())
                            .foreground_style(theme::ColourRole::secondary_label)
                            .line_limit(1)
                            .truncation_mode(composition::TruncationMode::tail)
                            .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 3.0F } },
        std::move(lines))
        .frame({ .width = logWidth, .height = logHeight, .alignment = composition::Alignment::bottom_leading })
        .clipped()
        .padding(theme::Edge::top, theme::Space{ 22.0F })
        .id(composition::ElementKey{ "connecting.log" });
}

auto ConnectingScreen::cancelButton(float scale) const -> composition::Element
{
    auto cancel = [commands = commands] { commands->cancel(); };
    return components::Button(components::ButtonRole::cancel, cancel,
        components::Text(strings->text("akira/common/cancel")).font(theme::Font::system(cancelSize * scale)))
        .id(composition::ElementKey{ "connecting.cancel" });
}

auto ConnectingScreen::strip(float scale) const -> composition::Element
{
    const auto white = [](float opacity) { return theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(opacity); };
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(progress.status)
                        .font(theme::Font::system(13.0F * scale, theme::Font::Weight::semibold))
                        .foreground_style(white(1.0F))
                        .line_limit(1)
                        .id(composition::ElementKey{ "connecting.status" }));
    if (!progress.activity.empty())
        lines.push_back(components::Text(progress.activity)
                            .font(theme::Font::system(12.0F * scale))
                            .foreground_style(white(0.56F))
                            .line_limit(1)
                            .id(composition::ElementKey{ "connecting.activity" }));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                  .spacing = theme::Space{ 18.0F * scale } },
        components::Text(strings->text("akira/connection/starting_stream"))
            .font(theme::Font::system(15.0F * scale, theme::Font::Weight::semibold))
            .foreground_style(white(1.0F))
            .fixed_size(),
        composition::Element(components::Group()).frame({ .width = 1.0F, .height = 32.0F * scale }).background(white(0.22F)),
        composition::Element(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                    .spacing = theme::Space{ 2.0F * scale } },
            std::move(lines)))
            .frame({ .width = stripDetailWidth * scale, .alignment = composition::Alignment::leading }),
        cancelButton(scale))
        .padding(theme::Insets{ .leading = theme::Space{ 22.0F * scale }, .trailing = theme::Space{ 12.0F * scale } })
        .frame({ .height = stripHeight * scale })
        .glass_panel(components::PanelRadius::large)
        .shadow(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(0.30F), 28.0F, 0.0F, 12.0F);
}

auto ConnectingScreen::backdrop(const Art& shown) -> composition::Element
{
    std::vector<composition::Element> layers;
    layers.push_back(composition::Element(app::arisa::Artwork(shown.background, ""))
                         .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
                         .clipped());
    if (!shown.hero.empty())
        layers.push_back(composition::Element(app::arisa::Artwork(shown.hero, "", app::arisa::Artwork::Scaling::Fit))
                             .frame({ .max_width = composition::infinity, .max_height = composition::infinity }));
    return composition::Element(components::ZStack(components::ZStackOptions{}, std::move(layers)))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity });
}

auto ConnectingScreen::heading(const Art& shown, float scale) -> composition::Element
{
    composition::Element title = shown.logo.empty()
        ? composition::Element(components::Text(shown.title)
                                   .font(theme::Font::system(artTitleSize * scale, theme::Font::Weight::semibold))
                                   .foreground_style(theme::ColourValue(theme::Colour::hex(0xFFFFFF)))
                                   .frame({ .max_width = artLogoWidth * scale, .alignment = composition::Alignment::leading }))
        : composition::Element(app::arisa::Artwork(shown.logo, "", app::arisa::Artwork::Scaling::Trimmed))
              .frame({ .width = artLogoWidth * scale, .height = artLogoHeight * scale,
                  .alignment = composition::Alignment::bottom_leading });
    return composition::Element(components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::bottom_leading },
                                    std::move(title)))
        .padding(theme::Insets{ .leading = theme::Space{ artEdge * scale }, .bottom = theme::Space{ artLogoBottom * scale } })
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::bottom_leading })
        .allows_hit_testing(false);
}

auto ConnectingScreen::chrome(int current, float scale) const -> composition::Element
{
    std::vector<composition::Element> headings;
    for (std::size_t at = 0; at < arts.size(); ++at)
        headings.push_back(heading(arts[at], scale)
                               .opacity(static_cast<int>(at) == current ? 1.0F : 0.0F)
                               .animation(motion::Animation::ease_in_out(artFade), current)
                               .id(composition::ElementKey{ std::format("connecting.heading.{}", at) }));
    std::vector<composition::Element> layers;
    layers.push_back(composition::Element(components::ZStack(
                         components::ZStackOptions{ .alignment = composition::Alignment::bottom_leading }, std::move(headings)))
                         .frame({ .max_width = composition::infinity, .max_height = composition::infinity }));
    layers.push_back(strip(scale)
                         .padding(theme::Insets{ .leading = theme::Space{ artEdge * scale }, .bottom = theme::Space{ stripBottom * scale },
                             .trailing = theme::Space{ artEdge * scale } })
                         .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                             .alignment = composition::Alignment::bottom_leading }));
    return composition::Element(components::ZStack(
        components::ZStackOptions{ .alignment = composition::Alignment::bottom_leading }, std::move(layers)));
}

auto ConnectingScreen::rotate(composition::StateHandle<int> shown) -> async::Task<void>
{
    while (true)
    {
        co_await async::sleep(artDwell);
        shown.update([](int& value) { ++value; });
    }
}

auto ConnectingScreen::layered(composition::Context& context) const -> composition::Element
{
    const auto page = [](float opacity) { return theme::ColourValue(theme::Colour::hex(0x0B0D12)).opacity(opacity); };
    const graphics::Gradient shade(std::vector<graphics::Gradient::Stop>{ { page(0.9F), 0.0F }, { page(0.5F), 0.2F }, { page(0.0F), 0.44F } });
    const auto shown = context.state(artShown);
    const int count = static_cast<int>(arts.size());
    const int current = shown.get() % count;
    std::vector<composition::Element> backdrops;
    for (int index = 0; index < count; ++index)
    {
        const float opacity = index == current ? 1.0F : 0.0F;
        const std::size_t at = static_cast<std::size_t>(index);
        backdrops.push_back(backdrop(arts[at])
                                .opacity(opacity)
                                .animation(motion::Animation::ease_in_out(artFade), current)
                                .id(composition::ElementKey{ std::format("connecting.art.{}", index) }));
    }
    std::vector<composition::Element> layers;
    layers.push_back(composition::Element(components::ZStack(components::ZStackOptions{}, std::move(backdrops)))
                         .frame({ .max_width = composition::infinity, .max_height = composition::infinity }));
    layers.push_back(composition::Element(components::Group())
                         .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
                         .background(graphics::ShapeStyle(graphics::LinearGradient{ .gradient = shade,
                             .start_point = motion::UnitPoint::bottom, .end_point = motion::UnitPoint::top }))
                         .allows_hit_testing(false));
    layers.push_back(components::GeometryReader([screen = *this, current](const composition::GeometryProxy& proxy) {
        return screen.chrome(current, std::max(1.0F, proxy.size().width / artReferenceWidth));
    }));
    composition::Element screen
        = composition::Element(components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::bottom_leading },
              std::move(layers)))
              .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
              .clipped()
              .environment(components::Environment::colour_scheme, std::optional<theme::ColorScheme>(theme::ColorScheme::dark))
              .background(page(1.0F))
              .on_exit_command(strings->text("akira/common/cancel"), [commands = commands] { commands->cancel(); });
    if (count > 1)
        screen = std::move(screen).task([shown] { return rotate(shown); });
    return std::move(screen).id(composition::ElementKey{ "connecting.layered" });
}

auto ConnectingScreen::body(composition::Context& context) const -> composition::Element
{
    if (!arts.empty())
        return layered(context);
    auto cancel = [commands = commands] { commands->cancel(); };
    composition::Element card =
        components::VStack(components::VStackOptions{ .spacing = theme::space::none },
            composition::Element(app::arisa::BrandLockup(logoHeight, logoHeight * mascotScale, app::arisa::BrandLockup::Motion::Connecting))
                .padding(theme::Edge::bottom, theme::Space{ 30.0F }),
            components::Text(strings->text("akira/connection/starting_stream"))
                .font(theme::Font::system(titleSize, theme::Font::Weight::semibold))
                .tracking(titleTracking),
            progress.stages ? stagesShown() : logShown(),
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
