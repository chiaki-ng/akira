#include "network/ui/tunnel_section.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/typography.hpp>

#include <vector>

namespace akira::network::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

TunnelSection::TunnelSection(TunnelControl& control, const app::Strings& strings)
    : control(&control)
    , strings(&strings)
{
}

auto TunnelSection::body(composition::Context& context) const -> composition::Element
{
    const TunnelControl::Status& status = context.read(control->view());
    std::vector<composition::Element> rows;
    if (!status.configured)
    {
        rows.push_back(components::Text(strings->text("akira/network/wg_no_config")).foreground_style(theme::ColourRole::orange));
        rows.push_back(components::Text(strings->text("akira/network/wg_config_path"))
                           .font(theme::Font::footnote)
                           .foreground_style(theme::ColourRole::secondary_label));
    }
    else
    {
        std::string shown = strings->text("akira/network/wg_disconnected");
        theme::ColourValue tone = theme::ColourRole::red;
        if (status.connecting)
        {
            shown = strings->text("akira/network/wg_connecting");
            tone = theme::ColourRole::orange;
        }
        else if (status.connected)
        {
            shown = strings->text("akira/network/wg_connected");
            tone = theme::ColourRole::green;
        }
        rows.push_back(components::LabeledContent(strings->text("akira/network/wg_status_label"),
            components::Text(shown).foreground_style(tone))
                           .id(composition::ElementKey{ "tunnel.status" }));
        if (status.connected)
            rows.push_back(components::LabeledContent(strings->text("akira/network/wg_tunnel_ip_label"), status.address));
        if (!status.connected && !status.error.empty())
            rows.push_back(components::Text(strings->format("akira/network/wg_error", status.error))
                               .font(theme::Font::footnote)
                               .foreground_style(theme::ColourRole::red));
        if (status.connecting)
            rows.push_back(controls::ProgressView());
        else if (status.connected)
            rows.push_back(components::Button(strings->text("akira/network/wg_disconnect"),
                components::Image::SystemName{ "logout" }, [control = control] { control->disconnect(); })
                               .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                               .id(composition::ElementKey{ "tunnel.disconnect" }));
        else
            rows.push_back(components::Button(strings->text("akira/network/wg_connect"),
                components::Image::SystemName{ "lock" }, [control = control] { control->connect(); })
                               .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                               .id(composition::ElementKey{ "tunnel.connect" }));
    }
    return structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/network/wireguard")) }, std::move(rows));
}

}
