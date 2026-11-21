#include "trophies/ui/trophy_page.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/theme/colour_value.hpp>
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
#include "trophies/ui/grade_counts.hpp"

namespace akira::trophies::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

TrophyPage::TrophyPage(TrophyModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto TrophyPage::body(composition::Context& context) const -> composition::Element
{
    const TrophyModel::View& view = context.read(model->view());
    if (!view.page)
        return components::Group();
    const theme::Glass glass = context.environment(components::Environment::theme).glass().panel;
    std::vector<composition::Element> shown;
    shown.push_back(heading(*view.page));
    if (!view.page->groups.empty())
        shown.push_back(groupChips(view));
    shown.push_back(choiceChips(view));
    shown.push_back(list(view, glass));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ spacing } },
        std::move(shown))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .on_exit_command(strings->text("akira/common/back"), [model = model] { model->close(); });
}

auto TrophyPage::heading(const TrophyModel::Page& page) const -> composition::Element
{
    std::string refresh = strings->text("akira/trophies/force_refresh_btn");
    if (page.refreshing)
        refresh = strings->text("akira/trophies/force_refresh_busy");
    else if (page.wait)
        refresh = model->waitText(*page.wait);
    const std::string subtitle = page.loading ? strings->text("akira/trophies/loading")
        : !page.error.empty()                 ? page.error
                                              : strings->format("akira/trophies/percent_complete", page.progress);
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 16.0F } },
        components::Button([model = model] { model->close(); },
            components::Image(components::Image::SystemName{ "arrow_back" }).frame({ .width = 22.0F, .height = 22.0F }))
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .button_border_shape(components::ButtonBorderShape::circle())
            .id(composition::ElementKey{ "trophies.back" }),
        composition::Element(app::arisa::Artwork(page.title.icon_url, "trophy"))
            .frame({ .width = artSize, .height = artSize })
            .clip_shape(theme::Shape::rounded(artRadius)),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 2.0F } },
            components::Text(page.title.name.empty() ? page.title.id.text() : page.title.name)
                .font(theme::Font::system(22.0F, theme::Font::Weight::semibold))
                .line_limit(1),
            components::Text(subtitle)
                .font(theme::Font::subheadline)
                .foreground_style(page.error.empty() ? theme::ColourRole::secondary_label : theme::ColourRole::red)
                .line_limit(1))
            .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }),
        GradeCounts(page.earned, GradeCounts::Size::Large),
        components::Button(std::move(refresh), components::Image::SystemName{ "refresh" },
            [model = model] { model->refreshTitle(); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .disabled(page.refreshing || page.wait.has_value())
            .id(composition::ElementKey{ "trophies.page.refresh" }))
        .frame({ .max_width = composition::infinity })
        .focus_section();
}

auto TrophyPage::chip(std::string label, bool chosen, std::function<void()> pressed, std::string key)
    -> composition::Element
{
    composition::Element button = components::Button(std::move(label), std::move(pressed))
                                      .font(theme::Font::subheadline)
                                      .id(composition::ElementKey{ std::move(key) });
    return chosen ? std::move(button).button_style(components::ButtonStyle::glass_prominent())
                  : std::move(button).button_style(components::ButtonStyle::glass(theme::Glass::clear()));
}

auto TrophyPage::groupChips(const TrophyModel::View& view) const -> composition::Element
{
    const std::optional<TrophyListing::GroupId>& chosen = view.trophyListing.group();
    std::vector<composition::Element> chips;
    chips.push_back(chip(strings->text("akira/trophies/group_all"), !chosen,
        [model = model] { model->selectGroup(std::nullopt); }, "trophies.group.all"));
    for (const TrophyModel::Group& group : view.page->groups)
    {
        const std::string id = group.id.text();
        chips.push_back(chip(group.name.empty() ? id : group.name, chosen && *chosen == group.id,
            [model = model, id] { model->selectGroup(id); }, "trophies.group." + id));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm, .wrap = true }, std::move(chips))
        .focus_section()
        .id(composition::ElementKey{ "trophies.groups" });
}

auto TrophyPage::choiceChips(const TrophyModel::View& view) const -> composition::Element
{
    std::vector<composition::Element> chips;
    for (const TrophyListing::Filter filter : TrophyListing::filters)
        chips.push_back(chip(strings->text(TrophyListing::filterKey(filter)), view.trophyListing.filter() == filter,
            [model = model, filter] { model->setTrophyFilter(filter); },
            std::format("trophies.filter.{}", static_cast<int>(filter))));
    chips.push_back(composition::Element(components::Divider()).frame({ .width = 1.0F, .height = 20.0F }));
    for (const TrophyListing::Sort sort : TrophyListing::sorts)
        chips.push_back(chip(strings->text(TrophyListing::sortKey(sort)), view.trophyListing.sort() == sort,
            [model = model, sort] { model->setTrophySort(sort); },
            std::format("trophies.sort.{}", static_cast<int>(sort))));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm, .wrap = true }, std::move(chips))
        .focus_section()
        .id(composition::ElementKey{ "trophies.choices" });
}

auto TrophyPage::list(const TrophyModel::View& view, theme::Glass glass) const -> composition::Element
{
    const TrophyModel::Page& page = *view.page;
    if (page.loading && page.trophies.empty())
        return composition::Element(controls::ProgressView())
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .glass_effect(glass, components::GlassPanel::shape(components::PanelRadius::large));
    if (page.trophies.empty())
        return components::Text(strings->text(page.groups.empty() || !view.trophyListing.group()
                                        ? "akira/trophies/empty_filter"
                                        : "akira/trophies/empty_group"))
            .font(theme::Font::title3)
            .foreground_style(theme::ColourRole::secondary_label)
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .glass_effect(glass, components::GlassPanel::shape(components::PanelRadius::large))
            .id(composition::ElementKey{ "trophies.page.empty" });
    std::vector<std::pair<TrophyModel::Trophy, bool>> rows;
    for (const TrophyModel::Trophy& trophy : page.trophies)
        rows.emplace_back(trophy, page.revealed.contains(trophy.id.value()));
    return components::ScrollView(
        components::LazyVStack(components::LazyVStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::space::none,
                                   .estimated_extent = iconSize + 20.0F },
            components::ForEach(
                std::move(rows),
                [](const std::pair<TrophyModel::Trophy, bool>& shown) {
                    return composition::ElementKey{ std::format("trophy.{}", shown.first.id.value()) };
                },
                [page = *this](const std::pair<TrophyModel::Trophy, bool>& shown) {
                    return page.row(shown.first, shown.second);
                }))
            .padding(theme::Space{ 6.0F }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .glass_effect(glass, components::GlassPanel::shape(components::PanelRadius::large))
        .focus_section()
        .id(composition::ElementKey{ "trophies.page.rows" });
}

auto TrophyPage::icon(const TrophyModel::Trophy& trophy, bool secret) const -> composition::Element
{
    if (secret)
        return components::ZStack(components::Image(components::Image::SystemName{ "lock" })
                                      .font(theme::Font::title2)
                                      .foreground_style(theme::ColourRole::secondary_label))
            .frame({ .width = iconSize, .height = iconSize })
            .background(theme::ColourValue(theme::ColourRole::label).opacity(0.08F), theme::Shape::rounded(iconRadius));
    return composition::Element(app::arisa::Artwork(trophy.icon_url, "trophy"))
        .frame({ .width = iconSize, .height = iconSize })
        .clip_shape(theme::Shape::rounded(iconRadius))
        .opacity(trophy.earned ? 1.0F : 0.45F);
}

auto TrophyPage::row(const TrophyModel::Trophy& trophy, bool revealed) const -> composition::Element
{
    const bool secret = TrophyListing::concealed(trophy) && !revealed;
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(secret ? strings->text("akira/trophies/hidden_name") : trophy.name)
                        .font(theme::Font::system(15.0F, theme::Font::Weight::semibold))
                        .line_limit(1));
    lines.push_back(components::Text(secret ? strings->text("akira/trophies/hidden_detail") : trophy.detail)
                        .font(theme::Font::footnote)
                        .foreground_style(theme::ColourRole::secondary_label)
                        .line_limit(2));
    const std::uint64_t target = trophy.progress_target.value_or(0);
    if (trophy.progress && target > 0 && !trophy.earned)
    {
        lines.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            composition::Element(controls::ProgressView(static_cast<double>(std::min(trophy.progress->value, target)),
                                     static_cast<double>(target)))
                .frame({ .width = barWidth }),
            components::Text(std::format("{}/{}", trophy.progress->value, target))
                .font(theme::Font::caption)
                .foreground_style(theme::ColourRole::secondary_label)));
    }
    lines.push_back(components::Text(model->rarityText(trophy))
                        .font(theme::Font::caption)
                        .foreground_style(theme::ColourRole::tertiary_label)
                        .line_limit(1));
    std::vector<composition::Element> grade;
    grade.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ 4.0F } },
        components::Image(std::string(TrophyListing::gradeAsset(trophy.grade)))
            .resizable()
            .aspect_ratio(std::nullopt, composition::ContentMode::fit)
            .frame({ .width = 16.0F, .height = 16.0F }),
        components::Text(strings->text(TrophyListing::gradeKey(trophy.grade))).font(theme::Font::footnote)));
    if (trophy.earned)
        grade.push_back(components::Text(model->earnedText(trophy))
                            .font(theme::Font::caption)
                            .foreground_style(theme::ColourRole::accent)
                            .line_limit(1));
    composition::Element label = components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                                        .spacing = theme::Space{ spacing } },
        icon(trophy, secret),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 3.0F } },
            std::move(lines))
            .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::trailing,
                               .spacing = theme::Space{ 4.0F } },
            std::move(grade))
            .frame({ .width = gradeWidth, .alignment = composition::Alignment::trailing }))
                                         .padding(theme::Space{ 10.0F })
                                         .frame({ .max_width = composition::infinity });
    const std::uint32_t id = trophy.id.value();
    const bool reveals = TrophyListing::concealed(trophy);
    return components::Button(
        [model = model, id, reveals, secret] {
            if (reveals && secret)
                model->reveal(id);
            else
                model->showTrophy(id);
        },
        std::move(label))
        .button_style(components::ButtonStyle::plain())
        .button_sizing(components::ButtonSizing::flexible)
        .id(composition::ElementKey{ std::format("trophies.trophy.{}", id) });
}

}
