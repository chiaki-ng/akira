#include "accounts/ui/welcome.hpp"

#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/structure/tile_row.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

#include "app/arisa/brand_lockup.hpp"

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

Welcome::Welcome(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto Welcome::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    AccountModel* held = model;
    std::vector<structure::TileRow::Tile> tiles;
    tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/qr_tile"),
        .system_image = "smartphone",
        .hint = strings->text("akira/profile_setup/qr_hint"),
        .action = [held] { held->startQr(); },
        .prominent = true });
    tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/companion_tile"),
        .system_image = "computer",
        .hint = strings->text("akira/profile_setup/companion_hint"),
        .action = [held] { held->startCompanion(); } });
    tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/legacy_tile"),
        .system_image = "tag",
        .hint = strings->text("akira/profile_setup/legacy_hint"),
        .action = [held] { held->startLegacy(); } });
    if (!view.entries.empty())
        tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/back_to_profiles"),
            .system_image = "undo",
            .hint = strings->text("akira/profile_setup/back_hint"),
            .action = [held] { held->closeFlow(); } });
    const float rowWidth = (tileSide * static_cast<float>(tiles.size()))
        + (structure::TileRow::tile_spacing * static_cast<float>(tiles.size() - 1));
    composition::Element screen = components::VStack(components::VStackOptions{ .spacing = theme::space::none },
        composition::Element(app::arisa::BrandLockup(logoHeight, mascotSide, app::arisa::BrandLockup::Motion::Idle))
            .padding(theme::Edge::bottom, theme::Space{ 26.0F }),
        components::Text(strings->text("akira/profile_setup/heading"))
            .font(theme::Font::system(titleSize, theme::Font::Weight::medium))
            .tracking(titleTracking)
            .multiline_text_alignment(composition::TextAlignment::center)
            .id(composition::ElementKey{ "welcome.title" }),
        components::Text(strings->text("akira/profile_setup/heading_detail"))
            .font(theme::Font::system(detailSize))
            .foreground_style(theme::ColourRole::secondary_label)
            .multiline_text_alignment(composition::TextAlignment::center)
            .padding(theme::Insets{ .top = theme::Space{ 10.0F }, .bottom = theme::Space{ 26.0F } })
            .id(composition::ElementKey{ "welcome.detail" }),
        structure::TileRow(std::move(tiles),
            structure::TileRow::Options{ .tile_width = tileSide,
                .tile_height = tileSide,
                .default_focus = 0,
                .label_font = theme::Font::system(labelSize, theme::Font::Weight::semibold),
                .label_lines = labelLines })
            .frame({ .width = rowWidth }))
                                      .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
                                      .id(composition::ElementKey{ "welcome" });
    if (view.entries.empty())
        return screen;
    return std::move(screen).on_exit_command(strings->text("akira/common/back"), [held] { held->closeFlow(); });
}

}
