#include "trophies/ui/trophy_sheet.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/arisa/artwork.hpp"
#include "trophies/trophy_listing.hpp"

namespace akira::trophies::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

TrophySheet::TrophySheet(TrophyModel::Trophy trophy, TrophyModel& model, const app::Strings& strings)
    : trophy(std::move(trophy))
    , model(&model)
    , strings(&strings)
{
}

auto TrophySheet::body(composition::Context&) const -> composition::Element
{
    const bool secret = TrophyListing::concealed(trophy);
    std::vector<composition::Element> shown;
    shown.push_back(composition::Element(app::arisa::Artwork(trophy.icon_url, "trophy"))
                        .frame({ .width = iconSize, .height = iconSize })
                        .clip_shape(theme::Shape::rounded(iconRadius))
                        .opacity(trophy.earned ? 1.0F : 0.45F));
    shown.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ 6.0F } },
        components::Image(std::string(TrophyListing::gradeAsset(trophy.grade)))
            .resizable()
            .aspect_ratio(std::nullopt, composition::ContentMode::fit)
            .frame({ .width = 20.0F, .height = 20.0F }),
        components::Text(strings->text(TrophyListing::gradeKey(trophy.grade))).font(theme::Font::subheadline)));
    shown.push_back(components::Text(secret ? strings->text("akira/trophies/hidden_name") : trophy.name)
                        .font(theme::Font::title2)
                        .font_weight(theme::Font::Weight::semibold)
                        .multiline_text_alignment(composition::TextAlignment::center));
    shown.push_back(components::Text(secret ? strings->text("akira/trophies/hidden_detail") : trophy.detail)
                        .font(theme::Font::body)
                        .foreground_style(theme::ColourRole::secondary_label)
                        .multiline_text_alignment(composition::TextAlignment::center));
    const std::uint64_t target = trophy.progress_target.value_or(0);
    if (trophy.progress && target > 0 && !trophy.earned)
        shown.push_back(composition::Element(controls::ProgressView(
                            static_cast<double>(std::min(trophy.progress->value, target)), static_cast<double>(target),
                            components::Text(std::format("{}/{}", trophy.progress->value, target))))
                            .frame({ .width = barWidth }));
    shown.push_back(components::Text(model->rarityText(trophy))
                        .font(theme::Font::footnote)
                        .foreground_style(theme::ColourRole::secondary_label));
    if (trophy.earned)
        shown.push_back(components::Text(model->earnedText(trophy))
                            .font(theme::Font::footnote)
                            .foreground_style(theme::ColourRole::accent));
    shown.push_back(components::Button(strings->text("akira/common/close"), [model = model] { model->closeTrophy(); })
                        .button_style(components::ButtonStyle::glass_prominent())
                        .id(composition::ElementKey{ "trophies.sheet.close" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 12.0F } }, std::move(shown))
        .padding(theme::Space{ 28.0F })
        .frame({ .width = width })
        .on_exit_command(strings->text("akira/common/close"), [model = model] { model->closeTrophy(); })
        .id(composition::ElementKey{ "trophies.sheet" });
}

}
