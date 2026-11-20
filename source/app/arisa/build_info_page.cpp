#include "app/arisa/build_info_page.hpp"

#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

BuildInfoPage::BuildInfoPage(std::filesystem::path file, const app::Strings& strings)
    : file(std::move(file))
    , strings(&strings)
{
}

auto BuildInfoPage::line(const std::string& text) -> composition::Element
{
    const std::size_t split = text.find(": ");
    if (split == std::string::npos)
        return components::Text(text).font(theme::Font::callout.monospaced());
    std::size_t value = split + 2;
    while (value < text.size() && text[value] == ' ')
        ++value;
    return components::LabeledContent(text.substr(0, split), text.substr(value));
}

auto BuildInfoPage::section(const BuildInfo::Section& shown) -> composition::Element
{
    std::vector<composition::Element> rows;
    for (const std::string& text : shown.lines)
        rows.push_back(line(text));
    if (shown.title.empty())
        return structure::Section(structure::Section::Labels{}, std::move(rows));
    return structure::Section(structure::Section::Labels{ .header = components::Text(shown.title) }, std::move(rows));
}

auto BuildInfoPage::body(composition::Context&) const -> composition::Element
{
    const std::optional<BuildInfo> read = BuildInfo::read(file);
    if (!read || read->sections().empty())
        return structure::Form(structure::Section(components::Text(strings->text("akira/build_info/load_error"))
                                                      .foreground_style(theme::ColourRole::red)))
            .id(composition::ElementKey{ "build-info" });
    std::vector<composition::Element> sections;
    for (const BuildInfo::Section& shown : read->sections())
        sections.push_back(section(shown));
    return composition::Element(structure::Form(std::move(sections))).id(composition::ElementKey{ "build-info" });
}

}
