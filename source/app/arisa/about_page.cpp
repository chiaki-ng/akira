#include "app/arisa/about_page.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>
#include <vector>

#include "app/arisa/qr_code.hpp"
#include "app/qr_matrix.hpp"
#include "app/version.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<std::string> AboutPage::shown{ "about-shown-link", std::string() };

AboutPage::AboutPage(const app::Strings& strings)
    : strings(&strings)
{
}

auto AboutPage::figure(std::string label, std::string value) -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 4.0F } },
        components::Text(std::move(label)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label),
        components::Text(std::move(value)).font(theme::Font::headline).line_limit(1))
        .padding(theme::Space{ 12.0F })
        .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
        .background(theme::ColourValue(theme::ColourRole::label).opacity(0.07F), theme::Shape::rounded(12.0F));
}

auto AboutPage::figures() const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
        figure(strings->text("akira/about/version"), version::string()),
        figure(strings->text("akira/about/commit"), version::commit()),
        figure(strings->text("akira/about/built"), version::date()),
        figure(strings->text("akira/about/license"), "AGPL-3.0"))
        .id(composition::ElementKey{ "about.figures" });
}

auto AboutPage::link(const Link& target, composition::StateHandle<std::string> open) const -> composition::Element
{
    std::vector<composition::Element> parts;
    parts.push_back(components::Text(std::string(target.name)).foreground_style(theme::ColourRole::accent));
    if (!target.author.empty())
        parts.push_back(components::Text(strings->format("akira/about/by", std::string(target.author)))
                            .foreground_style(theme::ColourRole::secondary_label));
    parts.push_back(components::Spacer());
    if (!target.noteKey.empty())
        parts.push_back(components::Text(strings->text(target.noteKey))
                            .font(theme::Font::footnote)
                            .foreground_style(theme::ColourRole::secondary_label)
                            .line_limit(1));
    parts.push_back(components::Image(components::Image::SystemName{ "grid_view" }).foreground_style(theme::ColourRole::tertiary_label));
    return components::Button([open, url = std::string(target.url)] { open.set(url); },
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(parts)))
        .button_sizing(components::ButtonSizing::flexible)
        .id(composition::ElementKey{ "about.link." + std::string(target.name) });
}

auto AboutPage::code(const std::string& url, composition::StateHandle<std::string> open, const app::Strings& strings)
    -> composition::Element
{
    std::vector<composition::Element> rows;
    if (const std::optional<QrMatrix> matrix = QrMatrix::encode(url))
        rows.push_back(QrCode(*matrix, codeSide));
    rows.push_back(components::Text(url).font(theme::Font::callout.monospaced()).line_limit(2));
    rows.push_back(components::Button(strings.text("akira/common/close"), components::ButtonRole::cancel,
        [open] { open.set(std::string()); })
                       .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                       .id(composition::ElementKey{ "about.code.close" }));
    return components::VStack(components::VStackOptions{ .spacing = theme::Space{ 16.0F } }, std::move(rows))
        .padding(theme::Space{ 24.0F })
        .id(composition::ElementKey{ "about.code" });
}

auto AboutPage::body(composition::Context& context) const -> composition::Element
{
    const auto open = context.state(shown);
    std::vector<composition::Element> community;
    for (const Link& target : AboutPage::community)
        community.push_back(link(target, open));
    std::vector<composition::Element> thanked;
    for (const Link& target : thanks)
        thanked.push_back(link(target, open));
    composition::Binding<bool> presented([open] { return !open.get().empty(); },
        [open](bool visible) {
            if (!visible)
                open.set(std::string());
        });
    const std::string url = open.get();
    std::vector<composition::Element> notice;
    notice.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                            .spacing = theme::space::sm },
        components::Image(components::Image::SystemName{ "warning" }).foreground_style(theme::ColourRole::orange),
        components::Text(strings->text("akira/about/notice")).font(theme::Font::callout)));
    std::vector<composition::Element> maintained;
    maintained.push_back(link(maintainer, open));
    std::vector<composition::Element> licence;
    licence.push_back(components::Text(strings->text("akira/about/license_text"))
                          .font(theme::Font::callout)
                          .foreground_style(theme::ColourRole::secondary_label));
    std::vector<composition::Element> sections;
    sections.push_back(structure::Section(figures()));
    sections.push_back(structure::Section(structure::Section::Labels{}, std::move(notice)));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/maintainer")) }, std::move(maintained)));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/community")) }, std::move(community)));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/thanks")) }, std::move(thanked)));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/license")) }, std::move(licence)));
    return composition::Element(structure::Form(std::move(sections)))
        .sheet(std::move(presented),
            [words = strings, url, open] {
                return composition::Element(code(url, open, *words)).presentation_sizing(composition::PresentationSizing::fitted);
            })
        .id(composition::ElementKey{ "about" });
}

}
