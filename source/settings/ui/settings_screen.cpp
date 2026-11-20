#include "settings/ui/settings_screen.hpp"

#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>

#include "network/ui/nat_section.hpp"
#include "settings/ui/controls_section.hpp"
#include "settings/ui/picture_section.hpp"

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace provider = ::arisa::provider;
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
    constexpr std::array pages{ Page::Picture, Page::Controls, Page::Network };
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
    const std::string pictureLabel = strings->text("akira/settings/menu_picture");
    const std::string controlsLabel = strings->text("akira/settings/menu_controls");
    const std::string networkLabel = strings->text("akira/settings/menu_network");
    return components::VStack(
        components::VStackOptions{ .alignment = components::HorizontalAlignment::leading, .spacing = theme::space::lg },
        components::Text(strings->text("akira/tabs/settings")).font(theme::Font::large_title),
        controls::Picker(strings->text("akira/tabs/settings"), composition::Binding<Page>(shown),
            [pictureLabel, controlsLabel, networkLabel] {
                return composition::Element(components::Group(components::Text(pictureLabel).tag(Page::Picture),
                    components::Text(controlsLabel).tag(Page::Controls),
                    components::Text(networkLabel).tag(Page::Network)));
            })
            .picker_style(controls::PickerStyle::segmented())
            .labels_hidden()
            .id(composition::ElementKey{ "settings.pages" }),
        content(shown.get()).frame({ .max_width = composition::infinity, .max_height = composition::infinity }))
        .padding(theme::Insets::symmetric(theme::space::xl, theme::space::lg))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
            .alignment = composition::Alignment::top_leading })
        .on_command(provider::Button::l, strings->text("akira/settings/previous_page"),
            [shown] { shown.set(next(shown.get(), -1)); })
        .on_command(provider::Button::r, strings->text("akira/settings/next_page"),
            [shown] { shown.set(next(shown.get(), 1)); });
}

}
