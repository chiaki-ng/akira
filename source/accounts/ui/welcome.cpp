#include "accounts/ui/welcome.hpp"

#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/structure/tile_row.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>
#include <vector>

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
        .hint = strings->text("akira/profile_setup/qr_body"),
        .action = [held] { held->startQr(); } });
    tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/companion_tile"),
        .system_image = "computer",
        .hint = strings->text("akira/profile_setup/companion_body"),
        .action = [held] { held->startCompanion(); } });
    tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/legacy_tile"),
        .system_image = "tag",
        .hint = strings->text("akira/profile_setup/legacy_body"),
        .action = [held] { held->startLegacy(); } });
    if (!view.entries.empty())
        tiles.push_back(structure::TileRow::Tile{ .title = strings->text("akira/profile_setup/back_to_profiles"),
            .system_image = "undo",
            .action = [held] { held->closeFlow(); } });
    composition::Element screen = components::VStack(components::VStackOptions{ .spacing = theme::Space{ 26.0F } },
        components::Image("img/icon.jpg")
            .resizable()
            .aspect_ratio(std::nullopt, composition::ContentMode::fit)
            .frame({ .width = 96.0F, .height = 96.0F })
            .clip_shape(theme::Shape::rounded(22.0F)),
        components::Text(strings->text("akira/profile_setup/title"))
            .font(theme::Font::title)
            .font_weight(theme::Font::Weight::semibold),
        structure::TileRow(std::move(tiles),
            structure::TileRow::Options{ .tile_width = tileSide, .tile_height = tileSide, .default_focus = 0 })
            .frame({ .width = hintWidth }))
                                      .id(composition::ElementKey{ "welcome" });
    if (view.entries.empty())
        return screen;
    return std::move(screen).on_exit_command(strings->text("akira/common/back"), [held] { held->closeFlow(); });
}

}
