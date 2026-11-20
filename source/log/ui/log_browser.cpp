#include "log/ui/log_browser.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>
#include <vector>

namespace akira::log::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace motion = ::arisa::motion;
namespace provider = ::arisa::provider;
namespace theme = ::arisa::theme;

LogBrowser::LogBrowser(LogShelf& shelf, const app::Strings& strings)
    : shelf(&shelf)
    , strings(&strings)
{
}

auto LogBrowser::tone(LogExcerpt::Level level) -> theme::ColourValue
{
    switch (level)
    {
        case LogExcerpt::Level::Error: return theme::ColourRole::red;
        case LogExcerpt::Level::Warning: return theme::ColourRole::orange;
        case LogExcerpt::Level::Debug: return theme::ColourRole::green;
        case LogExcerpt::Level::Verbose: return theme::ColourRole::tertiary_label;
        case LogExcerpt::Level::Info: break;
    }
    return theme::ColourRole::label;
}

auto LogBrowser::status(const LogShelf::View& view) const -> std::string
{
    if (view.file)
    {
        if (!view.excerpt)
            return strings->text("akira/settings/log_read_error");
        std::string shown = std::format("{}  ·  {} {}", LogFolder::sizeText(view.excerpt->totalBytes()), view.excerpt->lines(),
            strings->text("akira/settings/log_lines"));
        if (view.excerpt->truncated())
            shown += "  ·  " + strings->text("akira/settings/log_tail_notice");
        return shown;
    }
    if (!view.entries)
        return strings->text("akira/settings/log_folder_unavailable");
    return std::format("{} {}", view.entries->size(), strings->text("akira/settings/log_items"));
}

auto LogBrowser::row(const LogFolder::Entry& entry) const -> composition::Element
{
    LogShelf* held = shelf;
    std::string detail = strings->text("akira/settings/log_folder");
    if (!entry.directory)
    {
        detail = LogFolder::sizeText(entry.size);
        if (const std::string modified = LogFolder::modifiedText(entry.modified); !modified.empty())
            detail += "  ·  " + modified;
    }
    return components::Button([held, entry] { held->enter(entry); },
        components::HStack(components::HStackOptions{ .spacing = theme::Space{ 10.0F } },
            components::Image(components::Image::SystemName{ entry.directory ? "folder" : "description" })
                .foreground_style(theme::ColourRole::accent),
            components::Text(entry.name).line_limit(1),
            components::Spacer(),
            components::Text(detail).font(theme::Font::footnote).foreground_style(theme::ColourRole::secondary_label),
            components::Image(components::Image::SystemName{ "chevron_right" }).foreground_style(theme::ColourRole::tertiary_label)))
        .button_sizing(components::ButtonSizing::flexible)
        .id(composition::ElementKey{ "logs.entry." + entry.name });
}

auto LogBrowser::folder(const LogShelf::View& view) const -> composition::Element
{
    if (!view.entries)
        return components::ContentUnavailableView(strings->text("akira/settings/log_folder_unavailable"), "folder",
            components::Text(strings->text("akira/settings/log_folder_unavailable_hint")))
            .id(composition::ElementKey{ "logs.unavailable" });
    if (view.entries->empty())
        return components::ContentUnavailableView(strings->text("akira/settings/log_browser_title"), "article",
            components::Text(strings->text("akira/settings/log_folder_empty")))
            .id(composition::ElementKey{ "logs.empty" });
    std::vector<composition::Element> rows;
    for (const LogFolder::Entry& entry : *view.entries)
        rows.push_back(row(entry));
    return components::ScrollView(components::VStack(components::VStackOptions{ .spacing = theme::Space{ 6.0F } }, std::move(rows)))
        .id(composition::ElementKey{ "logs.list" });
}

auto LogBrowser::file(const LogShelf::View& view) const -> composition::Element
{
    if (!view.excerpt)
        return components::Text(strings->text("akira/settings/log_read_error")).foreground_style(theme::ColourRole::red);
    if (view.excerpt->empty())
        return components::Text(strings->text("akira/settings/log_file_empty")).foreground_style(theme::ColourRole::secondary_label);
    std::vector<composition::Element> blocks;
    for (const LogExcerpt::Block& block : view.excerpt->blocks())
        blocks.push_back(components::Text(block.text).font(theme::Font::footnote.monospaced()).foreground_style(tone(block.level)));
    return components::ScrollView(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                     .spacing = theme::Space{ 2.0F } },
                                      std::move(blocks))
                                      .padding(theme::Space{ 14.0F })
                                      .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
                                      .background(theme::ColourValue(theme::ColourRole::label).opacity(0.05F), theme::Shape::rounded(12.0F)))
        .default_scroll_anchor(motion::UnitPoint::bottom)
        .id(composition::ElementKey{ "logs.file" });
}

auto LogBrowser::body(composition::Context& context) const -> composition::Element
{
    const LogShelf::View& view = context.read(shelf->view());
    LogShelf* held = shelf;
    std::string title = strings->text("akira/settings/log_browser_title");
    if (view.file)
        title = view.file->filename().string();
    else if (view.current != view.root)
        title = view.current.filename().string();
    const std::string path = view.file ? view.file->string() : view.current.string();
    composition::Element screen =
        components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                               .spacing = theme::Space{ 10.0F } },
            components::HStack(components::HStackOptions{ .spacing = theme::space::sm },
                components::Text(title).font(theme::Font::headline).line_limit(1),
                components::Spacer(),
                components::Text(status(view)).font(theme::Font::footnote).foreground_style(theme::ColourRole::secondary_label),
                components::Button(strings->text("akira/settings/log_refresh"), components::Image::SystemName{ "refresh" },
                    [held] { held->refresh(); })
                    .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                    .id(composition::ElementKey{ "logs.refresh" })),
            components::Text(path).font(theme::Font::caption).foreground_style(theme::ColourRole::tertiary_label).line_limit(1),
            view.file ? file(view) : folder(view))
            .frame({ .max_width = composition::infinity, .max_height = composition::infinity, .alignment = composition::Alignment::top_leading })
            .on_command(provider::Button::x, strings->text("akira/settings/log_refresh"), [held] { held->refresh(); })
            .id(composition::ElementKey{ "logs" });
    if (!view.file && view.current == view.root)
        return screen;
    return std::move(screen).on_exit_command(strings->text("akira/common/back"), [held] { static_cast<void>(held->back()); });
}

}
