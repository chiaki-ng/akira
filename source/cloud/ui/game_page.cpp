#include "cloud/ui/game_page.hpp"

#include <arisa/components/async_image.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <optional>
#include <utility>

#include "app/arisa/art_session.hpp"
#include "app/arisa/artwork.hpp"
#include "cloud/game_facts.hpp"

namespace akira::cloud::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace graphics = ::arisa::graphics;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

GamePage::GamePage(CloudModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto GamePage::body(composition::Context& context) const -> composition::Element
{
    const CloudModel::View& view = context.read(model->view());
    if (!view.detail)
        return components::Group();
    const CloudModel::Detail& detail = *view.detail;
    const std::optional<GameActions> shown = model->actions();
    std::vector<composition::Element> info;
    info.push_back(metadata(detail));
    if (shown)
    {
        info.push_back(actionRow(detail, *shown));
        if (shown->showsLaunchChecks() && detail.checks)
            info.push_back(checkRow(*detail.checks));
        info.push_back(notes(detail, *shown));
    }
    std::vector<composition::Element> rows;
    rows.push_back(hero(detail));
    rows.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                          .spacing = theme::Space{ 22.0F } },
        composition::Element(app::arisa::Artwork(Game::sizedArt(detail.game.imageUrl.empty() ? detail.game.artworkUrl()
                                                                                              : detail.game.imageUrl,
                                                     CloudModel::coverWidth),
                                 "sports_esports"))
            .frame({ .width = coverSide, .height = coverSide })
            .clip_shape(theme::Shape::rounded(coverRadius)),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 12.0F } },
            std::move(info))
            .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })));
    if (detail.facts)
        rows.push_back(band(*detail.facts));
    CloudModel* held = model;
    return components::ScrollView(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                         .spacing = theme::Space{ 18.0F } },
                                      std::move(rows))
                                      .padding(theme::Insets{ .bottom = theme::Space{ 24.0F } }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .on_exit_command(strings->text("akira/cloud/start_back"), [held] { held->close(); })
        .id(composition::ElementKey{ "cloud.game" });
}

auto GamePage::hero(const CloudModel::Detail& detail) const -> composition::Element
{
    std::string backdrop = detail.game.landscapeImageUrl;
    if (detail.facts && detail.facts->art != GameDetail::Art::None && !detail.facts->background.empty())
        backdrop = detail.facts->background;
    const graphics::Gradient shade(std::vector<theme::ColourValue>{ theme::ColourValue(theme::Colour::hex(0x000000)).opacity(0.0F),
        theme::ColourValue(theme::Colour::hex(0x000000)).opacity(0.72F) });
    std::vector<composition::Element> layers;
    layers.push_back(composition::Element(app::arisa::Artwork(Game::sizedArt(backdrop, CloudModel::backdropWidth), "image"))
                         .frame({ .height = heroHeight, .max_width = composition::infinity }));
    if (detail.facts && detail.facts->art == GameDetail::Art::Layered && !detail.facts->hero.empty())
        layers.push_back(composition::Element(app::arisa::Artwork(Game::sizedArt(detail.facts->hero, CloudModel::backdropWidth), "image"))
                             .frame({ .height = heroHeight, .max_width = composition::infinity }));
    layers.push_back(composition::Element(components::Group())
                         .frame({ .height = heroHeight, .max_width = composition::infinity })
                         .background(graphics::ShapeStyle(graphics::LinearGradient{ .gradient = shade,
                             .start_point = motion::UnitPoint::top,
                             .end_point = motion::UnitPoint::bottom })));
    layers.push_back(heading(detail)
                         .padding(theme::Space{ 24.0F })
                         .frame({ .height = heroHeight, .max_width = composition::infinity,
                             .alignment = composition::Alignment::bottom_leading }));
    CloudModel* held = model;
    composition::Element back =
        components::Button([held] { held->close(); },
            components::Image(components::Image::SystemName{ "chevron_left" }).frame({ .width = 22.0F, .height = 22.0F }))
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .button_border_shape(components::ButtonBorderShape::circle())
            .id(composition::ElementKey{ "cloud.game.back" });
    layers.push_back(components::VStack(components::HStack(std::move(back), components::Spacer()), components::Spacer())
                         .padding(theme::Space{ 14.0F })
                         .frame({ .height = heroHeight, .max_width = composition::infinity }));
    return components::ZStack(components::ZStackOptions{ .alignment = composition::Alignment::bottom_leading }, std::move(layers))
        .frame({ .height = heroHeight, .max_width = composition::infinity })
        .clip_shape(theme::Shape::rounded(heroRadius))
        .id(composition::ElementKey{ "cloud.game.hero" });
}

auto GamePage::heading(const CloudModel::Detail& detail) const -> composition::Element
{
    std::string platform = detail.game.platform;
    std::ranges::transform(platform, platform.begin(), [](unsigned char letter) { return static_cast<char>(std::toupper(letter)); });
    const std::string eyebrow = platform.empty() ? strings->text("akira/cloud/card_badge")
                                                 : std::format("{} · {}", platform, strings->text("akira/cloud/card_badge"));
    composition::Element title = components::Text(detail.game.name)
                                     .font(theme::Font::large_title)
                                     .font_weight(theme::Font::Weight::bold)
                                     .foreground_style(theme::ColourValue(theme::Colour::hex(0xFFFFFF)))
                                     .line_limit(2);
    if (detail.facts && !detail.facts->logo.empty())
        title = components::AsyncImage(
            app::arisa::ArtSession::request(Game::sizedArt(detail.facts->logo, CloudModel::logoWidth)),
            [](components::Image image) { return composition::Element(std::move(image).resizable().scaled_to_fit()); },
            [name = detail.game.name] {
                return components::Text(name)
                    .font(theme::Font::large_title)
                    .font_weight(theme::Font::Weight::bold)
                    .foreground_style(theme::ColourValue(theme::Colour::hex(0xFFFFFF)));
            })
                    .frame({ .max_width = logoWidth, .max_height = logoHeight, .alignment = composition::Alignment::bottom_leading });
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 6.0F } },
        components::Text(eyebrow)
            .font(theme::Font::caption)
            .font_weight(theme::Font::Weight::semibold)
            .foreground_style(theme::ColourValue(theme::Colour::hex(0xFFFFFF)).opacity(0.72F)),
        std::move(title));
}

auto GamePage::metadata(const CloudModel::Detail& detail) const -> composition::Element
{
    const Game& game = detail.game;
    std::vector<composition::Element> row;
    const GameActions::StandingNote note = GameActions::standingNote(GameActions::standingAfter(game, detail.card), game);
    row.push_back(components::Text(strings->text(CloudModel::standingKey(note)))
                      .font(theme::Font::subheadline)
                      .foreground_style(game.isInCatalog() ? theme::ColourRole::yellow : theme::ColourRole::secondary_label));
    if (detail.release)
        row.push_back(components::Text(*detail.release)
                          .font(theme::Font::subheadline)
                          .foreground_style(theme::ColourRole::secondary_label));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(row))
        .id(composition::ElementKey{ "cloud.game.metadata" });
}

auto GamePage::focusKey(GameActions::Focus focus) -> std::string
{
    switch (focus)
    {
        case GameActions::Focus::Play: return "cloud.game.play";
        case GameActions::Focus::Store: return "cloud.game.store";
        case GameActions::Focus::AddToLibrary: return "cloud.game.add";
        case GameActions::Focus::RefreshTitle: break;
    }
    return "cloud.game.refresh";
}

auto GamePage::refreshText(const CloudModel::Refreshing& refreshing) const -> std::string
{
    switch (refreshing.phase)
    {
        case TitleRefreshCooldown::Phase::Checking: return strings->text("akira/cloud/detail_refresh_checking");
        case TitleRefreshCooldown::Phase::CoolingDown: return strings->format("akira/cloud/detail_refresh_wait", refreshing.seconds);
        case TitleRefreshCooldown::Phase::Idle: break;
    }
    return strings->text("akira/cloud/detail_refresh");
}

auto GamePage::actionRow(const CloudModel::Detail& detail, const GameActions& shown) const -> composition::Element
{
    CloudModel* held = model;
    std::vector<composition::Element> row;
    const auto add = [&row](GameActions::Focus focus, composition::Element button, bool prominent, bool enabled) {
        button = prominent ? std::move(button).button_style(components::ButtonStyle::glass_prominent())
                           : std::move(button).button_style(components::ButtonStyle::glass(theme::Glass::clear()));
        row.push_back(std::move(button).control_size(components::ControlSize::large).disabled(!enabled).id(
            composition::ElementKey{ focusKey(focus) }));
    };
    if (const auto& play = shown.play())
    {
        const bool choose = play->offer == GameActions::Offer::ChooseGame;
        const bool running = detail.checks && detail.checks->isRunning();
        add(GameActions::Focus::Play,
            components::Button(strings->text(choose ? "akira/cloud/detail_choose_game" : "akira/cloud/detail_check_and_play"),
                components::Image::SystemName{ "play_arrow" }, [held] { held->play(); }),
            true, play->enabled() && !running);
    }
    if (const auto& store = shown.store())
    {
        add(GameActions::Focus::Store,
            components::Button(strings->text(*store == GameActions::StoreTitle::ViewInStore ? "akira/cloud/detail_store_qr"
                                                                                           : "akira/cloud/detail_store_page_qr"),
                [held] { held->showStore(false); }),
            false, true);
    }
    else if (shown.addsToLibrary())
    {
        add(GameActions::Focus::AddToLibrary,
            components::Button(strings->text("akira/cloud/detail_add_to_library_qr"), [held] { held->showStore(true); }), true,
            true);
        add(GameActions::Focus::RefreshTitle,
            components::Button(refreshText(detail.refreshing), components::Image::SystemName{ "refresh" },
                [held] { held->refreshTitle(); }),
            false, shown.refreshTitleFocusable());
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } }, std::move(row))
        .focus_section()
        .id(composition::ElementKey{ "cloud.game.actions" });
}

auto GamePage::markSymbol(LaunchChecks::Mark mark) -> std::string
{
    switch (mark)
    {
        case LaunchChecks::Mark::Checking: return "sync";
        case LaunchChecks::Mark::Passed: return "check_circle";
        case LaunchChecks::Mark::Refused: return "cancel";
        case LaunchChecks::Mark::Unchecked: break;
    }
    return "radio_button_unchecked";
}

auto GamePage::markColour(LaunchChecks::Mark mark) -> theme::ColourRole
{
    switch (mark)
    {
        case LaunchChecks::Mark::Checking: return theme::ColourRole::accent;
        case LaunchChecks::Mark::Passed: return theme::ColourRole::green;
        case LaunchChecks::Mark::Refused: return theme::ColourRole::red;
        case LaunchChecks::Mark::Unchecked: break;
    }
    return theme::ColourRole::tertiary_label;
}

auto GamePage::checkRow(const LaunchChecks& checks) const -> composition::Element
{
    std::vector<composition::Element> steps;
    for (const LaunchChecks::Step step : checks.steps())
    {
        const LaunchChecks::Mark mark = checks.mark(step);
        steps.push_back(components::VStack(components::VStackOptions{ .spacing = theme::Space{ 4.0F } },
            components::Image(components::Image::SystemName{ markSymbol(mark) })
                .font(theme::Font::title3)
                .foreground_style(markColour(mark)),
            components::Text(strings->text(CloudModel::stepKey(step)))
                .font(theme::Font::caption)
                .foreground_style(mark == LaunchChecks::Mark::Unchecked ? theme::ColourRole::tertiary_label
                                                                         : theme::ColourRole::label))
                            .frame({ .max_width = composition::infinity }));
    }
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(steps))
        .frame({ .max_width = notesWidth })
        .id(composition::ElementKey{ "cloud.game.checks" });
}

auto GamePage::cardText(const CardCheck& check) const -> std::string
{
    switch (check.kind())
    {
        case CardCheck::Kind::Checking: return strings->text("akira/cloud/card_check_checking");
        case CardCheck::Kind::NotStreamable: return strings->text("akira/cloud/card_check_not_streamable");
        case CardCheck::Kind::Refused: return strings->text("akira/cloud/card_check_refused");
        case CardCheck::Kind::Unavailable: return strings->format("akira/cloud/card_check_unavailable", check.detail());
        case CardCheck::Kind::Streams: break;
    }
    return {};
}

auto GamePage::titleNotice(const CloudModel::Refreshing& refreshing) const -> std::string
{
    if (!refreshing.outcome)
        return {};
    switch (*refreshing.outcome)
    {
        case TitleRefresh::Outcome::Added: return strings->text("akira/cloud/title_added");
        case TitleRefresh::Outcome::NotInLibrary: return strings->text("akira/cloud/title_not_in_library");
        case TitleRefresh::Outcome::Failed: break;
    }
    return refreshing.message;
}

auto GamePage::notes(const CloudModel::Detail& detail, const GameActions& shown) const -> composition::Element
{
    std::vector<composition::Element> lines;
    if (shown.showsLaunchChecks())
    {
        if (detail.checks && detail.checks->reason())
            lines.push_back(components::Text(*detail.checks->reason()).font(theme::Font::callout).foreground_style(theme::ColourRole::red));
        if (detail.card)
            if (const std::string text = cardText(*detail.card); !text.empty())
                lines.push_back(components::Text(text).font(theme::Font::callout).foreground_style(theme::ColourRole::secondary_label));
    }
    if (shown.showsLibraryAddHint())
        lines.push_back(components::Text(strings->text("akira/cloud/detail_library_hint"))
                            .font(theme::Font::callout)
                            .foreground_style(theme::ColourRole::secondary_label));
    if (shown.showsTitleNotice())
        if (const std::string notice = titleNotice(detail.refreshing); !notice.empty())
            lines.push_back(components::Text(notice)
                                .font(theme::Font::callout)
                                .foreground_style(detail.refreshing.outcome == TitleRefresh::Outcome::Added
                                        ? theme::ColourRole::green
                                        : theme::ColourRole::secondary_label));
    if (lines.empty())
        return components::Group();
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 4.0F } },
        std::move(lines))
        .frame({ .max_width = notesWidth, .alignment = composition::Alignment::leading })
        .id(composition::ElementKey{ "cloud.game.notes" });
}

auto GamePage::band(const GameDetail& facts) const -> composition::Element
{
    std::vector<composition::Element> parts;
    if (facts.rating)
        parts.push_back(rating(*facts.rating));
    if (facts.age)
        parts.push_back(age(*facts.age));
    if (!GameFacts(facts).entries().empty())
        parts.push_back(factList(facts));
    if (parts.empty())
        return components::Group();
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                  .spacing = theme::Space{ 36.0F } },
        std::move(parts))
        .padding(theme::Space{ 22.0F })
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .glass_panel(components::PanelRadius::medium)
        .id(composition::ElementKey{ "cloud.game.band" });
}

auto GamePage::rating(const GameDetail::Rating& rated) const -> composition::Element
{
    const int filled = std::clamp(static_cast<int>(std::lround(rated.average)), 0, 5);
    std::vector<composition::Element> stars;
    for (int star = 1; star <= 5; ++star)
        stars.push_back(components::Image(components::Image::SystemName{ "star" })
                            .symbol_variant(star <= filled ? composition::SymbolVariants::fill : composition::SymbolVariants::none)
                            .foreground_style(star <= filled ? theme::ColourRole::yellow : theme::ColourRole::tertiary_label));
    std::vector<composition::Element> bars;
    for (int count = 5; count >= 1; --count)
    {
        const int percent = std::clamp(rated.percentByStars[static_cast<std::size_t>(count - 1)], 0, 100);
        bars.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            components::Text(std::format("{}", count)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label),
            controls::ProgressView(static_cast<double>(percent) / 100.0)
                .progress_view_style(controls::ProgressViewStyle::linear())
                .frame({ .width = barWidth }),
            components::Text(std::format("{}%", percent)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label)));
    }
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                  .spacing = theme::Space{ 22.0F } },
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 4.0F } },
            components::Text(rated.averageText).font(theme::Font::system(40.0F)).font_weight(theme::Font::Weight::semibold),
            components::HStack(components::HStackOptions{ .spacing = theme::Space{ 2.0F } }, std::move(stars)),
            components::Text(strings->counted("akira/cloud/rating_count", rated.count))
                .font(theme::Font::footnote)
                .foreground_style(theme::ColourRole::secondary_label)),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 3.0F } },
            std::move(bars)))
        .id(composition::ElementKey{ "cloud.game.rating" });
}

auto GamePage::age(const GameDetail::Age& rated) const -> composition::Element
{
    std::vector<composition::Element> text;
    if (!rated.name.empty())
        text.push_back(components::Text(rated.name).font(theme::Font::headline));
    if (!rated.descriptors.empty())
        text.push_back(components::Text(rated.descriptors).font(theme::Font::footnote).foreground_style(theme::ColourRole::secondary_label));
    std::vector<composition::Element> row;
    if (!rated.icon.empty())
        row.push_back(components::AsyncImage(app::arisa::ArtSession::request(rated.icon),
            [](components::Image image) { return composition::Element(std::move(image).resizable().scaled_to_fit()); },
            [] { return composition::Element(components::Group()); })
                          .frame({ .width = 52.0F, .height = 62.0F }));
    row.push_back(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                         .spacing = theme::Space{ 4.0F } },
        std::move(text))
                      .frame({ .width = 200.0F, .alignment = composition::Alignment::leading }));
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                  .spacing = theme::space::md },
        std::move(row))
        .id(composition::ElementKey{ "cloud.game.age" });
}

auto GamePage::factList(const GameDetail& facts) const -> composition::Element
{
    std::vector<composition::Element> rows;
    for (const GameFacts::Entry& entry : GameFacts(facts).entries())
    {
        const char* key = "akira/cloud/fact_genres";
        switch (entry.fact)
        {
            case GameFacts::Fact::Publisher: key = "akira/cloud/fact_publisher"; break;
            case GameFacts::Fact::Released: key = "akira/cloud/fact_released"; break;
            case GameFacts::Fact::Genres: break;
        }
        rows.push_back(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                              .spacing = theme::Space{ 1.0F } },
            components::Text(strings->text(key)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label),
            components::Text(entry.value).font(theme::Font::subheadline)));
    }
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 10.0F } },
        std::move(rows))
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .id(composition::ElementKey{ "cloud.game.facts" });
}

}
