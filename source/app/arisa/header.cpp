#include "app/arisa/header.hpp"

#include "app/version.hpp"
#include "network/ui/network_facts_row.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>


namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace status = ::arisa::components::status;
namespace theme = ::arisa::theme;

Header::Header(std::string title, const app::Strings& strings, std::optional<network::NetworkFacts> facts)
    : title(std::move(title))
    , facts(std::move(facts))
    , strings(&strings)
{
}

auto Header::body(composition::Context&) const -> composition::Element
{
    composition::Element statusRow =
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, status::Wireless(theme::Font::callout),
            status::Battery(theme::Font::callout.monospaced_digit(), theme::Font::callout),
            status::Clock(theme::Font::callout.monospaced_digit()))
            .foreground_style(theme::ColourRole::secondary_label);
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ spacing } },
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::last_text_baseline, .spacing = theme::Space{ 12.0F } },
            components::Text(title)
                .font(theme::Font::large_title)
                .font_weight(theme::Font::Weight::bold)
                .line_limit(1)
                .accessibility_add_traits(composition::AccessibilityTrait::is_header),
            components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center, .spacing = theme::space::sm },
                components::Text("v" + std::string(app::version::semver())).font(theme::Font::callout.monospaced_digit()).foreground_style(theme::ColourRole::tertiary_label),
                components::Text(platformTag)
                    .font(theme::Font::caption2.weight(theme::Font::Weight::semibold))
                    .kerning(0.6F)
                    .foreground_style(theme::ColourRole::secondary_label)
                    .padding(theme::Insets{ .top = theme::Space{ 2.0F }, .leading = theme::Space{ 6.0F },
                        .bottom = theme::Space{ 2.0F }, .trailing = theme::Space{ 6.0F } })
                    .overlay(components::ShapeView(theme::Shape::rounded(5.0F))
                            .stroke_border(theme::ColourValue(theme::ColourRole::label).opacity(0.18F), 1.0F)))),
        components::Spacer(),
        facts ? composition::Element(network::ui::NetworkFactsRow(*facts, *strings)).padding(theme::Edge::trailing, theme::space::lg)
                : composition::Element(components::Spacer().frame({ .width = 0.0F })),
        std::move(statusRow))
        .frame({ .max_width = composition::infinity })
        .id(composition::ElementKey{ "header" });
}

}
