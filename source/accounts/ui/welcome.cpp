#include "accounts/ui/welcome.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace akira::accounts::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

Welcome::Welcome(AccountModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto Welcome::tile(Tile shown) -> composition::Element
{
    composition::Element label = components::VStack(components::VStackOptions{ .spacing = theme::Space{ 10.0F } },
        components::Image(components::Image::SystemName{ std::move(shown.symbol) }).font(theme::Font::system(30.0F)),
        components::Text(std::move(shown.label))
            .font(theme::Font::callout)
            .font_weight(theme::Font::Weight::semibold)
            .line_limit(2))
                                     .padding(theme::Space{ 8.0F })
                                     .frame({ .width = tileSide, .height = tileSide });
    return components::Button([chosen = std::move(shown.chosen)] { chosen(); }, std::move(label))
        .button_style(shown.prominent ? components::ButtonStyle::glass_prominent()
                                      : components::ButtonStyle::glass(theme::Glass::clear()))
        .help(std::move(shown.helpTitle), std::move(shown.helpBody))
        .id(composition::ElementKey{ std::move(shown.key) });
}

auto Welcome::body(composition::Context& context) const -> composition::Element
{
    const AccountModel::View& view = context.read(model->view());
    AccountModel* held = model;
    std::vector<composition::Element> tiles;
    tiles.push_back(tile(Tile{ .key = "welcome.qr",
        .symbol = "smartphone",
        .label = strings->text("akira/profile_setup/qr_tile"),
        .helpTitle = strings->text("akira/profile_setup/qr_title"),
        .helpBody = strings->text("akira/profile_setup/qr_body"),
        .prominent = true,
        .chosen = [held] { held->startQr(); } }));
    tiles.push_back(tile(Tile{ .key = "welcome.companion",
        .symbol = "computer",
        .label = strings->text("akira/profile_setup/companion_tile"),
        .helpTitle = strings->text("akira/profile_setup/companion_title"),
        .helpBody = strings->text("akira/profile_setup/companion_body"),
        .chosen = [held] { held->startCompanion(); } }));
    tiles.push_back(tile(Tile{ .key = "welcome.legacy",
        .symbol = "tag",
        .label = strings->text("akira/profile_setup/legacy_tile"),
        .helpTitle = strings->text("akira/profile_setup/legacy_title"),
        .helpBody = strings->text("akira/profile_setup/legacy_body"),
        .chosen = [held] { held->startLegacy(); } }));
    if (!view.entries.empty())
        tiles.push_back(tile(Tile{ .key = "welcome.back",
            .symbol = "undo",
            .label = strings->text("akira/profile_setup/back_to_profiles"),
            .helpTitle = strings->text("akira/profile_setup/back_to_profiles"),
            .chosen = [held] { held->closeFlow(); } }));
    composition::Element screen = components::VStack(components::VStackOptions{ .spacing = theme::Space{ 26.0F } },
        components::Image("img/icon.jpg")
            .resizable()
            .aspect_ratio(std::nullopt, composition::ContentMode::fit)
            .frame({ .width = 96.0F, .height = 96.0F })
            .clip_shape(theme::Shape::rounded(22.0F)),
        components::Text(strings->text("akira/profile_setup/title"))
            .font(theme::Font::title)
            .font_weight(theme::Font::Weight::semibold),
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ tileSpacing } }, std::move(tiles))
            .focus_section())
                                      .id(composition::ElementKey{ "welcome" });
    if (view.entries.empty())
        return screen;
    return std::move(screen).on_exit_command(strings->text("akira/common/back"), [held] { held->closeFlow(); });
}

}
