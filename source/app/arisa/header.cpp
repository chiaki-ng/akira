#include "app/arisa/header.hpp"

#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/accessibility.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace status = ::arisa::components::status;
namespace theme = ::arisa::theme;

Header::Header(std::string title)
    : title(std::move(title))
{
}

auto Header::body(composition::Context&) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ spacing } },
        components::Text(title)
            .font(theme::Font::large_title)
            .font_weight(theme::Font::Weight::bold)
            .line_limit(1)
            .accessibility_add_traits(composition::AccessibilityTrait::is_header),
        components::Spacer(),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, status::Wireless(theme::Font::callout),
            status::Battery(theme::Font::callout.monospaced_digit(), theme::Font::callout),
            status::Clock(theme::Font::callout.monospaced_digit()))
            .foreground_style(theme::ColourRole::secondary_label))
        .frame({ .max_width = composition::infinity })
        .id(composition::ElementKey{ "header" });
}

}
