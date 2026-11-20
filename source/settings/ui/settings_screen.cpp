#include "settings/ui/settings_screen.hpp"

#include <arisa/components/divider.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/dock_bar.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <vector>

#include "network/ui/nat_section.hpp"
#include "settings/ui/controls_section.hpp"
#include "settings/ui/picture_section.hpp"

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

SettingsScreen::SettingsScreen(PictureOptions& picture, ControlOptions& controlOptions, network::ui::NatCheck& nat,
    const HelpCatalog& help, const app::Strings& strings)
    : picture(&picture)
    , controlOptions(&controlOptions)
    , nat(&nat)
    , help(&help)
    , strings(&strings)
{
}

auto SettingsScreen::next(Page current, int step) -> Page
{
    const int count = static_cast<int>(pages.size());
    const int index = (static_cast<int>(current) + step % count + count) % count;
    return pages[static_cast<std::size_t>(index)];
}

auto SettingsScreen::content(Page shown) const -> composition::Element
{
    switch (shown)
    {
        case Page::Controls: return ControlsSection(*controlOptions, *help, *strings);
        case Page::Network: return network::ui::NatSection(*nat, *help, *strings);
        case Page::Picture: break;
    }
    return PictureSection(*picture, *help, *strings);
}

auto SettingsScreen::body(composition::Context& context) const -> composition::Element
{
    const auto shown = context.state(page);
    composition::Binding<std::size_t> selection([shown] { return static_cast<std::size_t>(shown.get()); },
        [shown](std::size_t index) {
            if (index < pages.size())
                shown.set(pages[index]);
        });
    std::vector<structure::DockBar::Item> tabs{
        { strings->text("akira/settings/menu_picture"), "tv" },
        { strings->text("akira/settings/menu_controls"), "sports_esports" },
        { strings->text("akira/settings/menu_network"), "wifi" },
    };
    return components::VStack(components::VStackOptions{ .spacing = theme::space::none },
        composition::Element(structure::DockBar(std::move(selection), std::move(tabs)))
            .padding(theme::Edge::top, theme::Space{ 8.0F })
            .id(composition::ElementKey{ "settings.pages" }),
        composition::Element(components::Group()).frame({ .height = 14.0F }),
        components::Divider(),
        content(shown.get())
            .form_style(structure::FormStyle::cards())
            .toggle_style(controls::ToggleStyle::checkbox())
            .scroll_content_background(composition::Visibility::hidden)
            .frame({ .width = contentWidth, .max_height = composition::infinity })
            .padding(theme::Edge::top, theme::Space{ 6.0F })
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top }))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top })
        .on_command(provider::Button::l, strings->text("akira/settings/previous_page"),
            [shown] { shown.set(next(shown.get(), -1)); })
        .on_command(provider::Button::r, strings->text("akira/settings/next_page"),
            [shown] { shown.set(next(shown.get(), 1)); });
}

}
