#include "cloud/ui/shortcuts_rail.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <utility>
#include <vector>

#include "app/arisa/artwork.hpp"

namespace akira::cloud::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

ShortcutsRail::ShortcutsRail(CloudModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto ShortcutsRail::body(composition::Context& context) const -> composition::Element
{
    const CloudModel::View& view = context.read(model->view());
    std::vector<composition::Element> tiles;
    const std::size_t count = std::min(shown, view.shortcuts.size());
    for (std::size_t index = 0; index < count; ++index)
        tiles.push_back(tile(view.shortcuts[index], model, strings));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::space::sm },
        components::Text(strings->text("akira/hosts/shortcuts"))
            .font(theme::Font::headline)
            .foreground_style(theme::ColourRole::secondary_label),
        components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(tiles)))
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .focus_section()
        .id(composition::ElementKey{ "cloud.shortcuts" });
}

auto ShortcutsRail::tile(const Game& game, CloudModel* model, const app::Strings* strings) -> composition::Element
{
    composition::Element label =
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
            composition::Element(app::arisa::Artwork(Game::sizedArt(game.imageUrl, CloudModel::coverWidth), "sports_esports"))
                .frame({ .width = 44.0F, .height = 44.0F })
                .clip_shape(theme::Shape::rounded(tileRadius)),
            components::Text(game.name).font(theme::Font::subheadline).line_limit(2))
            .frame({ .width = tileWidth + 44.0F, .alignment = composition::Alignment::leading });
    return components::Button(
        [model, game] {
            model->open(game);
            model->play();
        },
        std::move(label))
        .button_style(components::ButtonStyle::card())
        .on_command(provider::Button::x, strings->text("akira/hosts/shortcut_remove"),
            [model, game] { model->toggleShortcut(game); })
        .id(composition::ElementKey{ "cloud.shortcut." + game.productId });
}

}
