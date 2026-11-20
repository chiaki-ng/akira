#include "network/ui/nat_section.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <vector>

namespace akira::network::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

NatSection::NatSection(NatCheck& check, const settings::HelpCatalog& help, const app::Strings& strings)
    : check(&check)
    , help(&help)
    , strings(&strings)
{
}

auto NatSection::body(composition::Context& context) const -> composition::Element
{
    const NatCheck::Status& status = context.read(check->view());
    std::vector<composition::Element> rows;
    std::vector<composition::Element> action;
    action.push_back(components::Button(strings->text(status.checking ? "akira/network/testing" : "akira/network/check_nat"),
        components::Image::SystemName{ "lan" }, [check = check] { check->start(); })
                         .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                         .disabled(status.checking)
                         .help(help->body("settings/natType"))
                         .id(composition::ElementKey{ "nat.check" }));
    if (status.checking)
        action.push_back(controls::ProgressView());
    rows.push_back(components::HStack(components::HStackOptions{ .spacing = theme::space::md }, std::move(action)));
    if (status.finished)
    {
        if (status.type)
        {
            theme::ColourValue tone = theme::ColourRole::green;
            if (status.tone == NatReport::Tone::Warning)
                tone = theme::ColourRole::orange;
            else if (status.tone == NatReport::Tone::Bad)
                tone = theme::ColourRole::red;
            rows.push_back(components::LabeledContent(strings->text("akira/network/nat_type"),
                components::Text(strings->format("akira/network/nat_type_value", *status.type))
                    .font_weight(theme::Font::Weight::semibold)
                    .foreground_style(tone))
                    .id(composition::ElementKey{ "nat.type" }));
        }
        rows.push_back(components::Text(status.detail)
                           .font(theme::Font::footnote)
                           .foreground_style(status.type ? theme::ColourValue(theme::ColourRole::secondary_label)
                                                         : theme::ColourValue(theme::ColourRole::red))
                           .id(composition::ElementKey{ "nat.detail" }));
    }
    return structure::Form(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/network/nat_type")) },
        std::move(rows)));
}

}
