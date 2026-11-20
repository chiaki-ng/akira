#include "cloud/ui/library_page.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>
#include <vector>

#include "app/arisa/artwork.hpp"
#include "cloud/game_actions.hpp"
#include "cloud/game_badges.hpp"
#include "cloud/library_listing.hpp"
#include "cloud/ui/shortcuts_rail.hpp"

namespace akira::cloud::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

LibraryPage::LibraryPage(CloudModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto LibraryPage::body(composition::Context& context) const -> composition::Element
{
    const CloudModel::View& view = context.read(model->view());
    std::vector<composition::Element> rows;
    rows.push_back(toolbar(view));
    if (!view.hasCatalog)
    {
        rows.push_back(statePanel(view));
        return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                      .spacing = theme::Space{ spacing } },
            std::move(rows))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top_leading })
            .id(composition::ElementKey{ "cloud.library" });
    }
    rows.push_back(filters(view));
    if (!view.shortcuts.empty())
        rows.push_back(ShortcutsRail(*model, *strings));
    rows.push_back(grid(view));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 14.0F } },
        std::move(rows))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .id(composition::ElementKey{ "cloud.library" });
}

auto LibraryPage::toolbar(const CloudModel::View& view) const -> composition::Element
{
    std::vector<composition::Element> row;
    row.push_back(statusChip(view));
    if (view.listed > 0)
    {
        const std::string count = view.games.size() != view.listed
            ? strings->format("akira/cloud/count_filtered", view.games.size(), view.listed)
            : strings->format("akira/cloud/count_all", view.listed);
        row.push_back(components::Text(count).font(theme::Font::subheadline).foreground_style(theme::ColourRole::secondary_label));
    }
    row.push_back(components::Spacer());
    if (view.hasCatalog)
    {
        CloudModel* held = model;
        row.push_back(controls::TextField(strings->text("akira/cloud/search"),
            composition::Binding<std::string>([query = view.query] { return query; },
                [held](std::string text) { held->search(std::move(text)); }),
            controls::TextFieldOptions{ .prompt = strings->text("akira/cloud/search") })
                          .labels_hidden()
                          .frame({ .width = searchWidth })
                          .id(composition::ElementKey{ "cloud.search" }));
        row.push_back(sortPicker(view));
    }
    row.push_back(optionsMenu(view));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(row))
        .frame({ .max_width = composition::infinity })
        .id(composition::ElementKey{ "cloud.controls" });
}

auto LibraryPage::chipColour(LibraryStatus::Chip chip) -> theme::ColourValue
{
    switch (chip)
    {
        case LibraryStatus::Chip::Ready: return theme::ColourRole::green;
        case LibraryStatus::Chip::Catalog:
        case LibraryStatus::Chip::Warning: return theme::ColourRole::orange;
        case LibraryStatus::Chip::Unavailable:
        case LibraryStatus::Chip::Error: return theme::ColourRole::red;
        case LibraryStatus::Chip::Pair:
        case LibraryStatus::Chip::Checking: return theme::ColourRole::accent;
        case LibraryStatus::Chip::Empty: break;
    }
    return theme::ColourRole::secondary_label;
}

auto LibraryPage::statusChip(const CloudModel::View& view) const -> composition::Element
{
    const auto chip = view.library.chip();
    if (!chip)
        return components::Group();
    const theme::ColourValue tint = chipColour(*chip);
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
        components::Image(components::Image::SystemName{ "circle" })
            .symbol_variant(composition::SymbolVariants::fill)
            .font(theme::Font::caption2)
            .foreground_style(tint),
        components::Text(strings->text(CloudModel::statusKey(*chip))).font(theme::Font::subheadline))
        .padding(theme::Insets::symmetric(theme::Space{ 12.0F }, theme::Space{ 5.0F }))
        .background(theme::ColourValue(tint).opacity(0.18F), theme::Shape::capsule())
        .id(composition::ElementKey{ "cloud.status" });
}

auto LibraryPage::sortPicker(const CloudModel::View& view) const -> composition::Element
{
    CloudModel* held = model;
    const std::vector<std::pair<int, std::string>> choices{
        { 0, strings->text(CloudModel::sortKey(LibraryListing::orderOf(0))) },
        { 1, strings->text(CloudModel::sortKey(LibraryListing::orderOf(1))) },
        { 2, strings->text(CloudModel::sortKey(LibraryListing::orderOf(2))) },
    };
    return controls::Picker(strings->text("akira/cloud/sort"),
        composition::Binding<int>([sort = view.sort] { return sort; }, [held](int sort) { held->sortBy(sort); }),
        [choices] {
            std::vector<composition::Element> items;
            for (const auto& [state, label] : choices)
                items.push_back(components::Text(label).tag(state));
            return composition::Element(components::Group(std::move(items)));
        })
        .picker_style(controls::PickerStyle::menu())
        .labels_hidden()
        .id(composition::ElementKey{ "cloud.sort" });
}

auto LibraryPage::optionsMenu(const CloudModel::View& view) const -> composition::Element
{
    const std::vector<LibraryStatus::Option> options = view.library.options();
    CloudModel* held = model;
    const std::string refreshText = strings->text(view.library.checking() ? "akira/cloud/refresh_busy" : "akira/cloud/refresh");
    const std::string pairText = strings->text("akira/cloud/pair");
    const std::string clearText = strings->text("akira/cloud/cache_clear");
    return controls::Menu(
        [options, held, refreshText, pairText, clearText] {
            std::vector<composition::Element> items;
            for (const LibraryStatus::Option option : options)
            {
                switch (option)
                {
                    case LibraryStatus::Option::Refresh:
                        items.push_back(components::Button(refreshText, [held] { held->refresh(true); }));
                        break;
                    case LibraryStatus::Option::Pair:
                        items.push_back(components::Button(pairText, [held] { held->signIn(); }));
                        break;
                    case LibraryStatus::Option::ClearCache:
                        items.push_back(components::Button(clearText, [held] { held->clearCache(); }));
                        break;
                }
            }
            return composition::Element(components::Group(std::move(items)));
        },
        components::Image(components::Image::SystemName{ "more_horiz" }).frame({ .width = 22.0F, .height = 22.0F }))
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .button_border_shape(components::ButtonBorderShape::circle())
        .id(composition::ElementKey{ "cloud.options" });
}

auto LibraryPage::platformTitle(PlatformFilter platform, const app::Strings* strings) -> std::string
{
    switch (platform)
    {
        case PlatformFilter::PS5: return "PS5";
        case PlatformFilter::PS4: return "PS4";
        case PlatformFilter::PS3: return "PS3";
        case PlatformFilter::All: break;
    }
    return strings->text("akira/cloud/platform_all");
}

auto LibraryPage::filters(const CloudModel::View& view) const -> composition::Element
{
    CloudModel* held = model;
    std::vector<std::pair<LibraryFilter, std::string>> shown;
    for (const LibraryFilter filter : view.filters)
        shown.emplace_back(filter, strings->text(CloudModel::filterKey(filter)));
    std::vector<std::pair<PlatformFilter, std::string>> platforms;
    for (const PlatformFilter platform : LibraryListing::allPlatforms)
        platforms.emplace_back(platform, platformTitle(platform, strings));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::md },
        controls::Picker(strings->text("akira/cloud/options"),
            composition::Binding<LibraryFilter>([filter = view.filter] { return filter; },
                [held](LibraryFilter filter) { held->pickFilter(filter); }),
            [shown] {
                std::vector<composition::Element> items;
                for (const auto& [filter, label] : shown)
                    items.push_back(components::Text(label).tag(filter));
                return composition::Element(components::Group(std::move(items)));
            })
            .picker_style(controls::PickerStyle::segmented())
            .labels_hidden()
            .id(composition::ElementKey{ "cloud.filters" }),
        components::Spacer(),
        controls::Picker(strings->text("akira/cloud/platform_all"),
            composition::Binding<PlatformFilter>([platform = view.platform] { return platform; },
                [held](PlatformFilter platform) { held->pickPlatform(platform); }),
            [platforms] {
                std::vector<composition::Element> items;
                for (const auto& [platform, label] : platforms)
                    items.push_back(components::Text(label).tag(platform));
                return composition::Element(components::Group(std::move(items)));
            })
            .picker_style(controls::PickerStyle::segmented())
            .labels_hidden()
            .id(composition::ElementKey{ "cloud.platforms" }))
        .frame({ .max_width = composition::infinity })
        .focus_section();
}

auto LibraryPage::grid(const CloudModel::View& view) const -> composition::Element
{
    if (view.games.empty())
    {
        composition::Element empty = view.query.empty()
            ? composition::Element(components::ContentUnavailableView(strings->text("akira/cloud/chip_empty"), "cloud"))
            : composition::Element(components::ContentUnavailableView::search(view.query));
        return std::move(empty)
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
            .id(composition::ElementKey{ "cloud.empty" });
    }
    CloudModel* held = model;
    const app::Strings* shown = strings;
    return components::ScrollView(
        components::LazyVGrid(
            components::LazyVGridOptions{
                .columns = components::GridItem::repeated(columns,
                    components::GridItem{ components::GridItem::Size::flexible(), theme::Space{ spacing } }),
                .alignment = components::HorizontalAlignment::leading,
                .spacing = theme::Space{ spacing },
                .estimated_extent = rowExtent },
            components::ForEach(
                view.games, [](const Game& game) { return composition::ElementKey{ "cloud.game." + game.productId }; },
                [held, shown](const Game& game) { return card(game, held, shown); }))
            .padding(theme::Insets{ .top = theme::Space{ 4.0F }, .bottom = theme::Space{ 24.0F } }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .focus_section()
        .id(composition::ElementKey{ "cloud.grid" });
}

auto LibraryPage::card(const Game& game, CloudModel* model, const app::Strings* strings) -> composition::Element
{
    const GameBadges badges(game);
    const std::string platform = platformBadge(game);
    composition::Element cover = composition::Element(app::arisa::Artwork(Game::sizedArt(game.imageUrl.empty()
                                                                                ? game.artworkUrl()
                                                                                : game.imageUrl,
                                                                              CloudModel::coverWidth),
                                                          "sports_esports"))
                                     .frame({ .width = coverSide, .height = coverSide })
                                     .clip_shape(theme::Shape::rounded(coverRadius));
    if (!platform.empty())
        cover = std::move(cover).overlay(composition::Alignment::top_leading,
            components::Text(platform)
                .font(theme::Font::caption2)
                .font_weight(theme::Font::Weight::bold)
                .padding(theme::Insets::symmetric(theme::Space{ 9.0F }, theme::Space{ 4.0F }))
                .glass_effect(theme::Glass::regular(), theme::Shape::capsule())
                .padding(theme::Space{ 8.0F }));
    if (!badges.playable())
        cover = std::move(cover).opacity(0.5F);
    composition::Element label =
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 3.0F } },
            std::move(cover),
            components::Text(game.name)
                .font(theme::Font::headline)
                .foreground_style(badges.playable() ? theme::ColourRole::label : theme::ColourRole::secondary_label)
                .line_limit(1)
                .padding(theme::Insets{ .top = theme::Space{ 7.0F } }),
            statusLine(game, model, strings))
            .frame({ .width = coverSide, .alignment = composition::Alignment::leading });
    const std::string pin = strings->text(model->pinned(game) ? "akira/hosts/shortcut_remove" : "akira/cloud/pin_shortcut");
    return components::Button([model, game] { model->open(game); }, std::move(label))
        .button_style(components::ButtonStyle::card())
        .on_command(provider::Button::x, pin, [model, game] { model->toggleShortcut(game); })
        .id(composition::ElementKey{ "cloud.game." + game.productId });
}

auto LibraryPage::statusLine(const Game& game, CloudModel* model, const app::Strings* strings) -> composition::Element
{
    if (model->lastPlayed(game))
        return components::Text(strings->text("akira/cloud/filter_recent"))
            .font(theme::Font::footnote)
            .foreground_style(theme::ColourRole::accent)
            .line_limit(1);
    const GameActions::StandingNote note = GameActions::standingNote(GameActions::standingAfter(game, std::nullopt), game);
    return components::Text(strings->text(CloudModel::standingKey(note)))
        .font(theme::Font::footnote)
        .foreground_style(theme::ColourRole::secondary_label)
        .line_limit(1);
}

auto LibraryPage::loadingLines(const CloudModel::View& view) const -> composition::Element
{
    std::vector<composition::Element> lines;
    const std::optional<FetchProgress>& progress = view.fetch;
    const auto stepKey = progress ? progress->stepKey() : std::nullopt;
    const auto detailKey = progress ? progress->detailKey() : std::nullopt;
    const auto pageKey = progress ? progress->pageKey() : std::nullopt;
    lines.push_back(components::Text(stepKey ? strings->text(*stepKey) : view.status.detail)
                        .font(theme::Font::callout)
                        .foreground_style(theme::ColourRole::secondary_label));
    if (detailKey)
        lines.push_back(components::Text(progress->timeServer() ? strings->format(*detailKey, *progress->timeServer())
                                                                : strings->text(*detailKey))
                            .font(theme::Font::subheadline)
                            .foreground_style(theme::ColourRole::tertiary_label));
    if (pageKey)
        lines.push_back(components::Text(progress->pagesExpected()
                                             ? strings->format(*pageKey, progress->pagesRead(), *progress->pagesExpected())
                                             : strings->format(*pageKey, progress->pagesRead()))
                            .font(theme::Font::footnote)
                            .foreground_style(theme::ColourRole::tertiary_label));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 4.0F } }, std::move(lines))
        .frame({ .max_width = stateWidth })
        .id(composition::ElementKey{ "cloud.loading" });
}

auto LibraryPage::statePanel(const CloudModel::View& view) const -> composition::Element
{
    const LibraryStatus& shown = view.library;
    std::string symbol = "cloud";
    switch (shown.glyph())
    {
        case LibraryStatus::Glyph::Pair: symbol = "link"; break;
        case LibraryStatus::Glyph::Alert: symbol = "error"; break;
        case LibraryStatus::Glyph::Cloud: break;
    }
    theme::ColourValue tone = theme::ColourRole::secondary_label;
    switch (shown.tone())
    {
        case LibraryStatus::Tone::Accent: tone = theme::ColourRole::accent; break;
        case LibraryStatus::Tone::Danger: tone = theme::ColourRole::red; break;
        case LibraryStatus::Tone::Dim: break;
    }
    std::string title = view.status.title;
    std::string detail = view.status.detail;
    if (shown.emptyLibrary())
    {
        title = strings->text("akira/cloud/empty_none_title");
        detail = strings->text("akira/cloud/empty_none_detail");
    }
    std::vector<composition::Element> rows;
    rows.push_back(components::Image(components::Image::SystemName{ symbol }).font(theme::Font::system(56.0F)).foreground_style(tone));
    rows.push_back(components::Text(title).font(theme::Font::title2).font_weight(theme::Font::Weight::semibold));
    if (shown.checking())
        rows.push_back(loadingLines(view));
    else if (!detail.empty())
        rows.push_back(components::Text(detail)
                           .font(theme::Font::callout)
                           .foreground_style(theme::ColourRole::secondary_label)
                           .frame({ .max_width = stateWidth }));
    CloudModel* held = model;
    switch (shown.action())
    {
        case LibraryStatus::Action::Pair:
            rows.push_back(components::Button(strings->text("akira/cloud/pair"), components::Image::SystemName{ "person_add" },
                [held] { held->signIn(); })
                               .button_style(components::ButtonStyle::glass_prominent())
                               .control_size(components::ControlSize::large)
                               .id(composition::ElementKey{ "cloud.state.pair" }));
            break;
        case LibraryStatus::Action::Refresh:
            rows.push_back(components::Button(strings->text("akira/cloud/check_again"), components::Image::SystemName{ "refresh" },
                [held] { held->refresh(true); })
                               .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                               .control_size(components::ControlSize::large)
                               .id(composition::ElementKey{ "cloud.state.refresh" }));
            break;
        case LibraryStatus::Action::None: break;
    }
    return components::VStack(components::VStackOptions{ .spacing = theme::space::md }, std::move(rows))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .id(composition::ElementKey{ "cloud.state" });
}

}
