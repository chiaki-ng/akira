#include "app/arisa/stream_panel.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

#include "app/arisa/stream_driver.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

StreamPanel::StreamPanel(StreamDriver& driver, const app::Strings& strings, bool sleepOffered)
    : driver(&driver)
    , strings(&strings)
    , sleepOffered(sleepOffered)
{
}

bool StreamPanel::operator==(const StreamPanel& other) const
{
    return driver == other.driver && strings == other.strings && sleepOffered == other.sleepOffered;
}

auto StreamPanel::body(composition::Context&) const -> composition::Element
{
    std::vector<composition::Element> rows;
    rows.push_back(components::Text(strings->text("akira/stream_menu/panel_power_title")).font(theme::Font::headline));
    rows.push_back(components::Button(strings->text("akira/stream_menu/hint_resume"), [driver = driver] {
        driver->resume();
    }).id(composition::ElementKey{"resume"}));
    rows.push_back(components::Button(strings->text("akira/stream_menu/opt_disconnect"), [driver = driver] {
        driver->leave(false);
    }).id(composition::ElementKey{"disconnect"}));
    if (sleepOffered)
    {
        rows.push_back(components::Button(strings->text("akira/stream_menu/opt_disconnect_sleep"), [driver = driver] {
            driver->leave(true);
        }).id(composition::ElementKey{"disconnect_sleep"}));
    }
    return components::VStack(components::VStackOptions{ .spacing = theme::space::md }, std::move(rows))
        .padding(theme::space::xl)
        .background(theme::Material::thick);
}

}
