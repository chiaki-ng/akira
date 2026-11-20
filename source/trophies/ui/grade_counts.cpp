#include "trophies/ui/grade_counts.hpp"

#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "trophies/trophy_listing.hpp"

namespace akira::trophies::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

GradeCounts::GradeCounts(libchiaking::trophies::TrophyCounts counts, Size size)
    : counts(counts)
    , size(size)
{
}

auto GradeCounts::body(composition::Context&) const -> composition::Element
{
    const bool large = size == Size::Large;
    const float icon = large ? largeIcon : compactIcon;
    std::vector<composition::Element> shown;
    for (const TrophyListing::Grade grade : TrophyListing::grades)
    {
        const std::uint32_t count = counts.of(grade);
        shown.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ 4.0F } },
            components::Image(std::string(TrophyListing::gradeAsset(grade)))
                .resizable()
                .aspect_ratio(std::nullopt, composition::ContentMode::fit)
                .frame({ .width = icon, .height = icon })
                .opacity(count > 0 ? 1.0F : 0.4F),
            components::Text(std::to_string(count))
                .font(theme::Font::system(large ? largeText : compactText,
                    large ? theme::Font::Weight::semibold : theme::Font::Weight::regular))
                .line_limit(1)));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ large ? 18.0F : 10.0F } },
        std::move(shown));
}

}
