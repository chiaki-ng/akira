#include "settings/ui/config_page.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

ConfigPage::ConfigPage(SettingsModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto ConfigPage::tone(ConfigListing::Token token) -> theme::ColourValue
{
    switch (token)
    {
        case ConfigListing::Token::Header: return theme::ColourRole::accent;
        case ConfigListing::Token::Key: return theme::ColourRole::label;
        case ConfigListing::Token::Punct:
        case ConfigListing::Token::Comment: return theme::ColourRole::tertiary_label;
        case ConfigListing::Token::String: return theme::ColourRole::green;
        case ConfigListing::Token::Number: return theme::ColourRole::orange;
        case ConfigListing::Token::Boolean: return theme::ColourRole::purple;
        case ConfigListing::Token::Plain: break;
    }
    return theme::ColourRole::secondary_label;
}

auto ConfigPage::line(const ConfigListing::Line& shown) -> composition::Element
{
    if (shown.pieces.empty())
        return composition::Element(components::Group()).frame({ .height = 8.0F });
    std::vector<composition::Element> pieces;
    for (const ConfigListing::Piece& piece : shown.pieces)
        pieces.push_back(components::Text(piece.text).font(theme::Font::footnote.monospaced()).foreground_style(tone(piece.token)));
    composition::Element row = components::HStack(components::HStackOptions{ .spacing = theme::space::none }, std::move(pieces));
    if (shown.header)
        row = std::move(row).padding(theme::Edge::top, theme::Space{ 10.0F });
    return row;
}

auto ConfigPage::body(composition::Context& context) const -> composition::Element
{
    const auto reveal = context.state(revealed);
    const std::optional<std::vector<ConfigListing::Line>> listed = ConfigListing::read(model->features().configFile, reveal.get());
    std::vector<composition::Element> lines;
    if (!listed)
    {
        lines.push_back(components::Text(strings->text("akira/config_view/file_not_found")));
        lines.push_back(components::Text(strings->text("akira/config_view/file_path")).foreground_style(theme::ColourRole::secondary_label));
    }
    else
        for (const ConfigListing::Line& shown : *listed)
            lines.push_back(line(shown));
    return components::ScrollView(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                     .spacing = theme::Space{ 2.0F } },
        components::Button(strings->text(reveal.get() ? "akira/config_view/hide_secrets" : "akira/config_view/reveal_secrets"),
            components::Image::SystemName{ reveal.get() ? "visibility_off" : "visibility" },
            [reveal] { reveal.set(!reveal.get()); })
            .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
            .padding(theme::Edge::bottom, theme::Space{ 10.0F })
            .id(composition::ElementKey{ "config.reveal" }),
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 2.0F } },
            std::move(lines)))
                                      .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading }))
        .id(composition::ElementKey{ "config.lines" });
}

}
