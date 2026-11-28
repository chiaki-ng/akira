#include "app/arisa/about_page.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <optional>
#include <random>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include <libchiaking/version.hpp>

#include "app/arisa/artwork.hpp"
#include "app/version.hpp"

namespace akira::app::arisa {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace provider = ::arisa::provider;
namespace structure = ::arisa::structure;
namespace theme = ::arisa::theme;

const composition::StateKey<bool> AboutPage::contributorsShown{ "about-contributors-shown", false };

AboutPage::AboutPage(const app::Strings& strings, const std::vector<std::string>& contributors)
    : strings(&strings)
    , logins(&contributors)
{
}

auto AboutPage::contributors(const provider::AssetProvider& assets) -> std::vector<std::string>
{
    const auto path = provider::AssetPath::create(contributorsFile);
    if (!path)
        return {};
    const auto blob = assets.read(*path);
    if (!blob)
        return {};
    const std::span<const std::byte> bytes = blob->bytes();
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::vector<std::string> found;
    for (const auto line : text | std::views::split('\n'))
    {
        std::string_view login(line.begin(), line.end());
        while (!login.empty() && (login.back() == '\r' || login.back() == ' '))
            login.remove_suffix(1);
        while (!login.empty() && login.front() == ' ')
            login.remove_prefix(1);
        if (!login.empty())
            found.emplace_back(login);
    }
    return ordered(std::move(found));
}

auto AboutPage::ordered(std::vector<std::string> logins) -> std::vector<std::string>
{
    std::vector<std::string> sorted;
    for (const std::string_view leading : leadingContributors)
        if (std::ranges::contains(logins, leading))
            sorted.emplace_back(leading);
    std::erase_if(logins, [](const std::string& login) { return std::ranges::contains(leadingContributors, login); });
    std::ranges::shuffle(logins, std::mt19937{ std::random_device{}() });
    std::ranges::move(logins, std::back_inserter(sorted));
    return sorted;
}

auto AboutPage::figure(std::string label, std::string value) -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 2.0F } },
        components::Text(std::move(label)).font(theme::Font::caption).foreground_style(theme::ColourRole::secondary_label),
        components::Text(std::move(value)).font(theme::Font::callout.weight(theme::Font::Weight::semibold)).line_limit(1));
}

auto AboutPage::figures() const -> composition::Element
{
    const libchiaking::Version core = libchiaking::Version::current();
    std::string coreText = "chiaki-ng-core " + core.text();
    if (!core.commit().empty())
        coreText += " (" + std::string(core.commit()) + ")";
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                  .spacing = theme::Space{ 28.0F } },
        figure(strings->text("akira/about/version"), version::string()),
        figure(strings->text("akira/about/core"), std::move(coreText)),
        figure(strings->text("akira/about/license"), "AGPL-3.0"),
        components::Spacer())
        .id(composition::ElementKey{ "about.figures" });
}

auto AboutPage::mark(const Tile& shown) -> composition::Element
{
    composition::Element glyph = shown.mark.empty()
        ? composition::Element(components::Image(components::Image::SystemName{ std::string(shown.symbol) })
                                   .font(theme::Font::system(18.0F, theme::Font::Weight::medium)))
        : composition::Element(components::Image(std::string(shown.mark))
                                   .resizable()
                                   .interpolation(components::Image::Interpolation::high)
                                   .frame({ .width = 24.0F, .height = 24.0F }));
    const theme::ColourValue fill = shown.tint == 0 ? theme::ColourValue(theme::ColourRole::label).opacity(0.08F)
                                                    : theme::ColourValue(theme::Colour::hex(shown.tint));
    return std::move(glyph)
        .frame({ .width = markSide, .height = markSide })
        .background(fill, theme::Shape::rounded(12.0F))
        .accessibility_hidden(true);
}

auto AboutPage::tile(const Tile& shown, const components::OpenURLAction& open) const -> composition::Element
{
    std::vector<composition::Element> detail;
    detail.push_back(components::Text(strings->text(shown.detailKey)).font(theme::Font::callout));
    if (!shown.captionKey.empty())
        detail.push_back(components::Text(strings->text(shown.captionKey)).font(theme::Font::caption));
    return components::Button([open, url = std::string(shown.url)] { open(url); },
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 12.0F } },
            components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                                   .spacing = theme::Space{ 12.0F } },
                mark(shown), components::Text(strings->text(shown.titleKey)).font(theme::Font::headline)),
            components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                   .spacing = theme::Space{ 2.0F } },
                std::move(detail))
                .foreground_style(theme::ColourRole::secondary_label))
            .padding(theme::Space{ 17.0F })
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity,
                .alignment = composition::Alignment::top_leading })
            .glass_effect(theme::Glass::regular(), theme::Shape::rounded(tileRadius)))
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::rounded(tileRadius))
        .accessibility_label(strings->text(shown.titleKey))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity })
        .id(composition::ElementKey{ "about.tile." + std::string(shown.titleKey) });
}

auto AboutPage::project(const Project& shown, const components::OpenURLAction& open) const -> composition::Element
{
    composition::Element logo = composition::Element(components::Group());
    if (shown.logo.starts_with("https://"))
        logo = Artwork(std::string(shown.logo), std::string(shown.symbol), Artwork::Scaling::Fit);
    else if (!shown.logo.empty())
        logo = components::Image(std::string(shown.logo)).resizable().interpolation(components::Image::Interpolation::high);
    else
        logo = components::Image(components::Image::SystemName{ std::string(shown.symbol) })
                   .font(theme::Font::system(30.0F, theme::Font::Weight::medium))
                   .foreground_style(theme::ColourRole::secondary_label);
    std::vector<composition::Element> lines;
    lines.push_back(std::move(logo).frame({ .width = logoSide, .height = logoSide }).clip_shape(theme::Shape::rounded(14.0F)));
    lines.push_back(components::Text(std::string(shown.name))
                        .font(theme::Font::callout.weight(theme::Font::Weight::semibold))
                        .foreground_style(theme::ColourRole::accent));
    lines.push_back(components::Text(strings->format("akira/about/by", std::string(shown.author)))
                        .font(theme::Font::caption)
                        .foreground_style(theme::ColourRole::secondary_label)
                        .line_limit(2));
    if (!shown.noteKey.empty())
        lines.push_back(components::Text(strings->text(shown.noteKey))
                            .font(theme::Font::caption)
                            .foreground_style(theme::ColourRole::secondary_label));
    return components::Button([open, url = std::string(shown.url)] { open(url); },
        components::VStack(components::VStackOptions{ .spacing = theme::Space{ 8.0F } }, std::move(lines))
            .padding(theme::Insets::symmetric(theme::Space{ 8.0F }, theme::Space{ 14.0F }))
            .frame({ .max_width = composition::infinity, .min_height = 150.0F, .alignment = composition::Alignment::top })
            .background(theme::ColourValue(theme::ColourRole::label).opacity(0.05F), theme::Shape::rounded(12.0F)))
        .button_style(components::ButtonStyle::plain())
        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
            theme::Shape::rounded(12.0F))
        .id(composition::ElementKey{ "about.project." + std::string(shown.name) });
}

auto AboutPage::rows(std::vector<composition::Element> cells, std::size_t columns, float spacing) -> composition::Element
{
    return components::LazyVGrid(
        components::LazyVGridOptions{
            .columns = components::GridItem::repeated(
                columns, components::GridItem{ components::GridItem::Size::flexible(), theme::Space{ spacing } }),
            .spacing = theme::Space{ spacing } },
        std::move(cells));
}

auto AboutPage::people(const components::OpenURLAction& open, composition::StateHandle<bool> expanded) const
    -> composition::Element
{
    const bool showing = expanded.get();
    std::vector<composition::Element> parts;
    parts.push_back(components::Button([expanded, showing] { expanded.set(!showing); },
        components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::center,
                               .spacing = theme::Space{ 8.0F } },
            components::Image(components::Image::SystemName{ showing ? "expand_more" : "chevron_right" })
                .font(theme::Font::caption.weight(theme::Font::Weight::semibold)),
            components::Text(strings->text("akira/about/all_contributors")).font(theme::Font::callout),
            components::Text(std::to_string(logins->size()))
                .font(theme::Font::caption.weight(theme::Font::Weight::semibold).monospaced_digit())
                .foreground_style(theme::ColourRole::secondary_label)
                .padding(theme::Insets::symmetric(theme::Space{ 7.0F }, theme::Space{ 2.0F }))
                .background(theme::ColourValue(theme::ColourRole::label).opacity(0.08F), theme::Shape::capsule()),
            components::Spacer()))
                        .button_style(components::ButtonStyle::plain())
                        .content_shape(composition::ContentShapeKinds::interaction | composition::ContentShapeKinds::focus_effect,
                            theme::Shape::rounded(8.0F))
                        .accessibility_label(strings->text(showing ? "akira/about/hide_contributors"
                                                                   : "akira/about/show_contributors"))
                        .id(composition::ElementKey{ "about.contributors" }));
    if (showing)
    {
        std::vector<composition::Element> chips;
        for (const std::string& login : *logins)
            chips.push_back(components::Button([open, url = "https://github.com/" + login] { open(url); },
                components::Text(login)
                    .font(theme::Font::caption.weight(theme::Font::Weight::medium))
                    .foreground_style(theme::ColourRole::accent)
                    .line_limit(1)
                    .padding(theme::Insets::symmetric(theme::Space{ 9.0F }, theme::Space{ 4.0F }))
                    .frame({ .max_width = composition::infinity })
                    .background(theme::ColourValue(theme::ColourRole::accent).opacity(0.12F), theme::Shape::capsule()))
                                .button_style(components::ButtonStyle::plain())
                                .content_shape(composition::ContentShapeKinds::interaction
                                        | composition::ContentShapeKinds::focus_effect,
                                    theme::Shape::capsule())
                                .id(composition::ElementKey{ "about.contributor." + login }));
        parts.push_back(rows(std::move(chips), contributorColumns, 6.0F));
    }
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 10.0F } },
        std::move(parts));
}

auto AboutPage::thanks(const components::OpenURLAction& open, composition::StateHandle<bool> expanded) const
    -> composition::Element
{
    std::vector<composition::Element> cells;
    for (const Project& listed : projects)
        cells.push_back(project(listed, open));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 12.0F } },
        components::Text(strings->text("akira/about/thanks_detail"))
            .font(theme::Font::callout)
            .foreground_style(theme::ColourRole::secondary_label),
        rows(std::move(cells), projectColumns, 10.0F),
        people(open, expanded));
}

auto AboutPage::body(composition::Context& context) const -> composition::Element
{
    const components::OpenURLAction& open = context.environment(components::Environment::open_url);
    const auto expanded = context.state(contributorsShown);
    std::vector<composition::Element> notice;
    notice.push_back(components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                            .spacing = theme::space::sm },
        components::Image(components::Image::SystemName{ "warning" }).foreground_style(theme::ColourRole::orange),
        components::Text(strings->text("akira/about/notice")).font(theme::Font::callout)));
    std::vector<composition::Element> social;
    for (const Tile& listed : tiles)
        social.push_back(tile(listed, open));
    std::vector<composition::Element> thanked;
    thanked.push_back(thanks(open, expanded));
    std::vector<composition::Element> licence;
    licence.push_back(components::Text(strings->text("akira/about/license_text"))
                          .font(theme::Font::callout)
                          .foreground_style(theme::ColourRole::secondary_label));
    std::vector<composition::Element> privacy;
    privacy.push_back(components::Text(strings->text("akira/about/privacy_text"))
                          .font(theme::Font::callout)
                          .foreground_style(theme::ColourRole::secondary_label));
    std::vector<composition::Element> sections;
    sections.push_back(structure::Section(figures()));
    sections.push_back(structure::Section(structure::Section::Labels{}, std::move(notice)));
    sections.push_back(structure::Section(components::HStack(
        components::HStackOptions{ .alignment = components::VerticalAlignment::top, .spacing = theme::Space{ 16.0F } },
        std::move(social))));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/thanks")) }, std::move(thanked)));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/license")) }, std::move(licence)));
    sections.push_back(structure::Section(
        structure::Section::Labels{ .header = components::Text(strings->text("akira/about/privacy")) }, std::move(privacy)));
    return composition::Element(structure::Form(std::move(sections))).id(composition::ElementKey{ "about" });
}

}
