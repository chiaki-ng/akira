#include "play/ui/players_panel.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>

namespace akira::play::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace theme = ::arisa::theme;

PlayersPanel::PlayersPanel(Couch& couch, const app::Strings& strings)
    : couch(&couch)
    , strings(&strings)
{
}

auto PlayersPanel::playerColour(int player) -> theme::ColourValue
{
    switch (player)
    {
        case 1: return theme::ColourRole::blue;
        case 2: return theme::ColourRole::red;
        case 3: return theme::ColourRole::green;
        case 4: return theme::ColourRole::pink;
        default: break;
    }
    return theme::ColourRole::gray;
}

auto PlayersPanel::tone(SeatBoard::Tone shown) -> theme::ColourValue
{
    switch (shown)
    {
        case SeatBoard::Tone::Ready: return theme::ColourRole::green;
        case SeatBoard::Tone::Failed: return theme::ColourRole::red;
        case SeatBoard::Tone::Waiting: return theme::ColourRole::yellow;
        case SeatBoard::Tone::Neutral: break;
    }
    return theme::ColourRole::secondary_label;
}

auto PlayersPanel::led(int player, bool open) -> composition::Element
{
    composition::Element number = components::Text(std::format("{}", player))
                                      .font(theme::Font::system(15.0F, theme::Font::Weight::semibold))
                                      .frame({ .width = ledSize, .height = ledSize });
    if (open)
        return std::move(number)
            .foreground_style(theme::ColourRole::tertiary_label)
            .background(theme::ColourValue(theme::ColourRole::label).opacity(0.06F), theme::Shape::circle());
    return std::move(number)
        .foreground_style(theme::Colour::hex(0x0B0D12))
        .background(playerColour(player), theme::Shape::circle());
}

auto PlayersPanel::action(std::string label, std::string key, std::function<void()> perform) -> composition::Element
{
    return components::Button(std::move(label), [perform = std::move(perform)] { perform(); })
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .id(composition::ElementKey{ std::move(key) });
}

auto PlayersPanel::heading(const Couch::View& view) const -> composition::Element
{
    std::string title = strings->text("akira/couch/players");
    if (view.mode == Couch::Mode::Claiming)
        title = strings->text("akira/couch/choose_controller");
    else if (view.mode == Couch::Mode::ChoosingProfile)
        title = strings->format("akira/couch/choose_profile_for", view.profileSlot + 1);
    std::vector<composition::Element> parts;
    parts.push_back(components::Text(std::move(title)).font(theme::Font::title2).font_weight(theme::Font::Weight::semibold));
    parts.push_back(components::Spacer());
    if (view.mode == Couch::Mode::Roster)
        parts.push_back(components::Text(strings->format("akira/couch/seated_count", view.seated, int{ CouchTable::seatCount }))
                            .font(theme::Font::callout)
                            .foreground_style(theme::ColourRole::accent)
                            .id(composition::ElementKey{ "players.count" }));
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(parts));
}

auto PlayersPanel::row(const SeatBoard::Row& shown) const -> composition::Element
{
    Couch* held = couch;
    const std::string slot = std::format("{}", shown.slot);
    std::vector<composition::Element> parts;
    parts.push_back(led(shown.player, shown.open));
    std::vector<composition::Element> label;
    label.push_back(components::Text(shown.device)
                        .font(theme::Font::headline)
                        .foreground_style(shown.open ? theme::ColourValue(theme::ColourRole::secondary_label)
                                                     : theme::ColourValue(theme::ColourRole::label))
                        .line_limit(1));
    if (!shown.status.empty())
        label.push_back(components::Text(shown.status).font(theme::Font::caption).foreground_style(tone(shown.tone)).line_limit(1));
    parts.push_back(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                           .spacing = theme::Space{ 2.0F } },
        std::move(label)));
    parts.push_back(components::Spacer());
    const std::uint8_t at = shown.slot;
    switch (shown.action)
    {
        case SeatBoard::Action::ChooseProfile:
            parts.push_back(action(strings->text("akira/couch/choose_profile"), "players.choose." + slot,
                [held, at] { held->chooseProfile(at, true); }));
            break;
        case SeatBoard::Action::Retry:
            parts.push_back(action(strings->text("akira/couch/retry"), "players.retry." + slot, [held, at] { held->retry(at); }));
            break;
        case SeatBoard::Action::Invite:
            parts.push_back(components::Button(strings->text("akira/couch/add_player"), components::Image::SystemName{ "person_add" },
                [held] { held->invite(); })
                                .button_style(components::ButtonStyle::glass_prominent())
                                .id(composition::ElementKey{ "players.add" }));
            break;
        case SeatBoard::Action::None: break;
    }
    if (shown.pad && shown.slot != 0)
        parts.push_back(action(strings->text("akira/couch/leave"), "players.leave." + slot,
            [held, pad = *shown.pad] { held->leave(pad); }));
    return components::HStack(components::HStackOptions{ .spacing = theme::Space{ 12.0F } }, std::move(parts))
        .padding(theme::Insets::symmetric(theme::Space{ 12.0F }, theme::Space{ 8.0F }))
        .frame({ .max_width = composition::infinity, .min_height = 54.0F })
        .background(theme::ColourValue(theme::ColourRole::label).opacity(shown.open ? 0.03F : 0.07F),
            theme::Shape::rounded(12.0F))
        .id(composition::ElementKey{ "players.row." + slot });
}

auto PlayersPanel::roster(const Couch::View& view) const -> composition::Element
{
    std::vector<composition::Element> rows;
    for (const SeatBoard::Row& shown : view.rows)
        rows.push_back(row(shown));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ rowSpacing } }, std::move(rows));
}

auto PlayersPanel::claiming(const Couch::View& view) const -> composition::Element
{
    if (view.candidates.empty())
        return components::ContentUnavailableView(strings->text("akira/couch/no_controllers"), "sports_esports",
            components::Text(strings->text("akira/couch/connect_pad")))
            .id(composition::ElementKey{ "players.waiting" });
    std::vector<composition::Element> rows;
    for (const SeatBoard::Candidate& candidate : view.candidates)
    {
        std::vector<composition::Element> label;
        label.push_back(components::Text(candidate.label).font(theme::Font::headline));
        label.push_back(components::Text(view.claimPercent > 0 ? strings->format("akira/couch/claiming", view.claimPercent)
                                                               : strings->text("akira/couch/hold_to_claim"))
                            .font(theme::Font::caption)
                            .foreground_style(theme::ColourRole::secondary_label));
        if (view.claimPercent > 0)
            label.push_back(controls::ProgressView(view.claimPercent / 100.0, 1.0).progress_view_style(controls::ProgressViewStyle::linear()));
        rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ 12.0F } },
            components::Image(components::Image::SystemName{ "sports_esports" })
                .font(theme::Font::title2)
                .foreground_style(theme::ColourRole::accent),
            components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::Space{ 4.0F } },
                std::move(label)),
            components::Spacer())
                           .padding(theme::Insets::symmetric(theme::Space{ 12.0F }, theme::Space{ 10.0F }))
                           .frame({ .max_width = composition::infinity })
                           .overlay(components::ShapeView(theme::Shape::rounded(12.0F)).stroke_border(theme::ColourRole::accent, 1.5F))
                           .id(composition::ElementKey{ std::format("players.candidate.{}", static_cast<std::uint32_t>(candidate.pad)) }));
    }
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ rowSpacing } }, std::move(rows));
}

auto PlayersPanel::tile(const Couch::Profile& profile) const -> composition::Element
{
    Couch* held = couch;
    const std::string initial = profile.label.empty()
        ? std::string("?")
        : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(profile.label.front()))));
    return components::Button([held, id = profile.id] { held->selectProfile(id); },
        components::VStack(components::VStackOptions{ .spacing = theme::Space{ 8.0F } },
            components::Text(initial)
                .font(theme::Font::system(24.0F, theme::Font::Weight::semibold))
                .foreground_style(theme::ColourRole::on_accent)
                .frame({ .width = avatarSize, .height = avatarSize })
                .background(theme::ColourRole::accent, theme::Shape::circle()),
            components::Text(profile.label).font(theme::Font::callout).line_limit(1))
            .frame({ .width = tileWidth, .height = tileHeight }))
        .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
        .id(composition::ElementKey{ std::format("players.profile.{}", profile.id) });
}

auto PlayersPanel::profiles(const Couch::View& view) const -> composition::Element
{
    std::vector<composition::Element> rows;
    if (view.rejected)
        rows.push_back(components::Text(strings->text("akira/couch/profile_rejected"))
                           .font(theme::Font::callout)
                           .foreground_style(theme::ColourRole::red)
                           .id(composition::ElementKey{ "players.rejected" }));
    if (view.profiles.empty())
        rows.push_back(components::ContentUnavailableView(strings->text("akira/couch/no_profiles"), "person",
            components::Text(strings->text("akira/couch/no_profiles_hint")))
                           .id(composition::ElementKey{ "players.no-profiles" }));
    for (std::size_t first = 0; first < view.profiles.size(); first += tilesPerRow)
    {
        std::vector<composition::Element> tiles;
        for (std::size_t index = first; index < std::min(first + tilesPerRow, view.profiles.size()); ++index)
            tiles.push_back(tile(view.profiles[index]));
        rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::Space{ 12.0F } }, std::move(tiles)));
    }
    rows.push_back(components::Text(strings->text("akira/couch/profile_requirement"))
                       .font(theme::Font::footnote)
                       .foreground_style(theme::ColourRole::secondary_label));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 12.0F } },
        std::move(rows));
}

auto PlayersPanel::body(composition::Context& context) const -> composition::Element
{
    const Couch::View& view = context.read(couch->view());
    composition::Element content = roster(view);
    if (view.mode == Couch::Mode::Claiming)
        content = claiming(view);
    else if (view.mode == Couch::Mode::ChoosingProfile)
        content = profiles(view);
    composition::Element panel = components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                        .spacing = theme::Space{ 14.0F } },
        heading(view), std::move(content))
                                     .id(composition::ElementKey{ "players" });
    if (view.mode == Couch::Mode::Roster)
        return panel;
    Couch* held = couch;
    return std::move(panel).on_exit_command(strings->text("akira/common/back"), [held] { static_cast<void>(held->back()); });
}

}
