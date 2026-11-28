#include "app/arisa/link_code.hpp"

#include <optional>
#include <utility>
#include <vector>

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include "app/arisa/qr_code.hpp"
#include "app/qr_matrix.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

LinkCode::LinkCode(std::string url, std::function<void()> closed)
    : url(std::move(url))
    , closed(std::move(closed))
{
}

auto LinkCode::card(const app::Strings& strings) const -> composition::Element
{
    std::vector<composition::Element> rows;
    if (const std::optional<QrMatrix> matrix = QrMatrix::encode(url))
        rows.push_back(QrCode(*matrix, side));
    rows.push_back(components::Text(url).font(theme::Font::callout.monospaced()).line_limit(2));
    rows.push_back(components::Button(strings.text("akira/common/close"), components::ButtonRole::cancel, [closed = closed] { closed(); })
                       .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                       .id(composition::ElementKey{ "link-code.close" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ spacing } }, std::move(rows))
        .padding(theme::Space{ padding })
        .glass_panel(components::PanelRadius::modal)
        .on_exit_command(strings.text("akira/common/close"), [closed = closed] { closed(); })
        .id(composition::ElementKey{ "link-code" });
}

auto LinkCode::dimmed(const app::Strings& strings) const -> composition::Element
{
    return components::VStack(card(strings))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .background(theme::ColourValue(theme::Colour::hex(0x000000)).opacity(scrim))
        .id(composition::ElementKey{ "link-code.scrim" });
}

}
