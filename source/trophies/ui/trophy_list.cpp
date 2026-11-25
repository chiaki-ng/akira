#include "trophies/ui/trophy_list.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>
#include <vector>

#include "app/arisa/artwork.hpp"
#include "trophies/title_listing.hpp"
#include "trophies/ui/grade_counts.hpp"

namespace akira::trophies::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

TrophyList::TrophyList(TrophyModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto TrophyList::dated(const std::optional<libchiaking::trophies::Time>& at) -> std::string
{
    if (!at)
        return {};
    return std::format("{:%F}", std::chrono::floor<std::chrono::days>(*at));
}

auto TrophyList::body(composition::Context& context) const -> composition::Element
{
    const TrophyModel::View& view = context.read(model->view());
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ spacing } },
        toolbar(view), summary(view), rows(view))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading });
}

auto TrophyList::toolbar(const TrophyModel::View& view) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::space::md },
        components::Text(view.updated)
            .font(theme::Font::footnote)
            .foreground_style(theme::ColourRole::secondary_label)
            .line_limit(1)
            .id(composition::ElementKey{ "trophies.updated" }),
        components::Spacer(), sortMenu(view), filterMenu(view), refreshButton(view))
        .frame({ .max_width = composition::infinity })
        .focus_section();
}

auto TrophyList::sortMenu(const TrophyModel::View& view) const -> composition::Element
{
    std::vector<std::pair<TitleListing::Sort, std::string>> choices;
    for (const TitleListing::Sort sort : TitleListing::sorts)
        choices.emplace_back(sort, strings->text(TitleListing::sortKey(sort)));
    const TitleListing::Sort current = view.listing.sort();
    return controls::Menu(strings->format("akira/trophies/sort_btn_value", strings->text(TitleListing::sortKey(current))),
        [model = model, choices = std::move(choices), current] {
            std::vector<composition::Element> items;
            for (const auto& [sort, label] : choices)
                items.push_back(components::Button(sort == current ? label + "  \xe2\x9c\x93" : label,
                    [model, sort] { model->setSort(sort); }));
            return composition::Element(components::Group(std::move(items)));
        })
        .menu_style(controls::MenuStyle::button())
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .id(composition::ElementKey{ "trophies.sort" });
}

auto TrophyList::filterMenu(const TrophyModel::View& view) const -> composition::Element
{
    std::vector<std::pair<TitleListing::Filter, std::string>> choices;
    for (const TitleListing::Filter filter : TitleListing::filters)
        choices.emplace_back(filter, strings->text(TitleListing::filterKey(filter)));
    const TitleListing::Filter current = view.listing.filter();
    return controls::Menu(
        strings->format("akira/trophies/filter_btn_value", strings->text(TitleListing::filterKey(current))),
        [model = model, choices = std::move(choices), current] {
            std::vector<composition::Element> items;
            for (const auto& [filter, label] : choices)
                items.push_back(components::Button(filter == current ? label + "  \xe2\x9c\x93" : label,
                    [model, filter] { model->setFilter(filter); }));
            return composition::Element(components::Group(std::move(items)));
        })
        .menu_style(controls::MenuStyle::button())
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .id(composition::ElementKey{ "trophies.filter" });
}

auto TrophyList::refreshButton(const TrophyModel::View& view) const -> composition::Element
{
    std::string label = strings->text("akira/trophies/refresh");
    if (view.refreshing)
        label = strings->text("akira/trophies/force_refresh_busy");
    else if (view.wait)
        label = model->waitText(*view.wait);
    return components::Button(std::move(label), components::Image::SystemName{ "refresh" },
        [model = model] { model->refresh(); })
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .disabled(view.refreshing || view.wait.has_value())
        .id(composition::ElementKey{ "trophies.refresh" });
}

auto TrophyList::summary(const TrophyModel::View& view) const -> composition::Element
{
    composition::Element level = components::Group();
    if (view.summary)
    {
        const TrophyModel::Summary& shown = *view.summary;
        level = components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                       .spacing = theme::Space{ 6.0F } },
            components::Text(strings->format("akira/trophies/summary_level", shown.level, shown.tier, shown.progress))
                .font(theme::Font::system(22.0F, theme::Font::Weight::semibold))
                .line_limit(1),
            composition::Element(controls::ProgressView(std::min<double>(shown.progress, 100.0), 100.0))
                .frame({ .width = summaryBarWidth }));
    }
    else if (view.summaryFailed)
        level = components::Text(strings->text("akira/trophies/summary_unavailable"))
                    .font(theme::Font::headline)
                    .foreground_style(theme::ColourRole::secondary_label);
    const libchiaking::trophies::TrophyCounts earned = view.summary ? view.summary->earned : libchiaking::trophies::TrophyCounts{};
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 24.0F } }, std::move(level),
        components::Spacer(), GradeCounts(earned, GradeCounts::Size::Large))
        .padding(theme::Space{ 20.0F })
        .frame({ .max_width = composition::infinity })
        .glass_panel(components::PanelRadius::large)
        .id(composition::ElementKey{ "trophies.summary" });
}

auto TrophyList::rows(const TrophyModel::View& view) const -> composition::Element
{
    if (view.rows.empty())
        return composition::Element(components::ContentUnavailableView(
                   strings->text(view.anyTitles || view.listing.filter() != TitleListing::Filter::All
                           ? "akira/trophies/empty_filter"
                           : "akira/trophies/empty"),
                   "trophy"))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .id(composition::ElementKey{ "trophies.empty" });
    return components::ScrollView(
        components::LazyVStack(components::LazyVStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::space::none,
                                   .estimated_extent = iconSize + (2.0F * rowPadding) },
            components::ForEach(
                view.rows, [](const TrophyModel::Row& shown) { return composition::ElementKey{ shown.title.id.text() }; },
                [model = model](const TrophyModel::Row& shown) { return row(shown, model); }))
            .padding(theme::Space{ listPadding }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .glass_panel(components::PanelRadius::large)
        .focus_section()
        .id(composition::ElementKey{ "trophies.rows" });
}

auto TrophyList::platforms(const TrophyModel::Title& title) -> composition::Element
{
    std::vector<composition::Element> chips;
    for (const TitleListing::Platform platform : title.platforms)
        chips.push_back(components::Text(std::string(TitleListing::platformName(platform)))
                            .font(theme::Font::system(10.0F, theme::Font::Weight::semibold))
                            .padding(theme::Insets::symmetric(theme::Space{ 6.0F }, theme::Space{ 2.0F }))
                            .background(theme::ColourValue(theme::ColourRole::label).opacity(0.12F), theme::Shape::capsule()));
    chips.push_back(components::Text(dated(title.last_updated))
                        .font(theme::Font::caption)
                        .foreground_style(theme::ColourRole::secondary_label));
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 6.0F } }, std::move(chips));
}

auto TrophyList::row(const TrophyModel::Row& shown, TrophyModel* model) -> composition::Element
{
    const TrophyModel::Title& title = shown.title;
    std::vector<composition::Element> lines;
    lines.push_back(components::Text(title.name.empty() ? title.id.text() : title.name)
                        .font(theme::Font::system(15.0F, theme::Font::Weight::semibold))
                        .line_limit(1));
    lines.push_back(platforms(title));
    if (!shown.played.empty())
        lines.push_back(components::Text(shown.played)
                            .font(theme::Font::caption)
                            .foreground_style(theme::ColourRole::accent)
                            .line_limit(1));
    composition::Element label = components::HStack(components::HStackOptions{ .spacing = theme::Space{ spacing } },
        composition::Element(app::arisa::Artwork(title.icon_url, "trophy"))
            .frame({ .width = iconSize, .height = iconSize })
            .clip_shape(theme::Shape::rounded(iconRadius)),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 6.0F } },
            std::move(lines))
            .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }),
        GradeCounts(title.earned, GradeCounts::Size::Compact),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::trailing,
                               .spacing = theme::Space{ 4.0F } },
            components::Text(std::format("{}%", title.progress))
                .font(theme::Font::system(14.0F, theme::Font::Weight::semibold)),
            composition::Element(controls::ProgressView(std::min<double>(title.progress, 100.0), 100.0))
                .frame({ .width = barWidth }))
            .frame({ .width = barWidth }))
                                         .padding(theme::Space{ rowPadding })
                                         .frame({ .max_width = composition::infinity });
    return components::Button([model, id = title.id.text()] { model->open(id); }, std::move(label))
        .button_style(components::ButtonStyle::plain())
        .button_sizing(components::ButtonSizing::flexible)
        .id(composition::ElementKey{ "trophies.title." + title.id.text() });
}

}
