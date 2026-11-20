#include "settings/ui/discovery_log_page.hpp"

#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>
#include <vector>

namespace akira::settings::ui {

namespace async = ::arisa::async;
namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

DiscoveryLogPage::DiscoveryLogPage(SettingsModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto DiscoveryLogPage::follow(SettingsModel* model) -> async::Task<void>
{
    while (true)
    {
        model->refreshDiscoveryLog();
        co_await async::sleep(refreshEvery);
    }
}

auto DiscoveryLogPage::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(model->view());
    const Maintenance::DiscoveryLog& journal = status.discovery;
    SettingsModel* held = model;
    std::vector<composition::Element> lines;
    if (journal.lines.empty())
        lines.push_back(components::Text(strings->text("akira/settings/discovery_log_empty"))
                            .foreground_style(theme::ColourRole::secondary_label));
    for (const std::string& line : journal.lines)
        lines.push_back(components::Text(line).font(theme::Font::footnote.monospaced()).foreground_style(theme::ColourRole::secondary_label));
    const std::string count = journal.enabled ? std::format("{}", journal.lines.size()) : strings->text("akira/settings/discovery_log_off");
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 10.0F } },
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, components::Spacer(),
            components::Text(count)
                .font(theme::Font::footnote)
                .foreground_style(journal.enabled ? theme::ColourValue(theme::ColourRole::secondary_label)
                                                  : theme::ColourValue(theme::ColourRole::orange))
                .id(composition::ElementKey{ "discovery-log.count" }),
            components::Button(strings->text("akira/settings/discovery_log_clear"), components::Image::SystemName{ "delete" },
                [held] { held->clearDiscoveryLog(); })
                .button_style(components::ButtonStyle::glass(theme::Glass::clear()))
                .id(composition::ElementKey{ "discovery-log.clear" })),
        components::ScrollView(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                      .spacing = theme::Space{ 2.0F } },
                                   std::move(lines))
                                   .padding(theme::Space{ 14.0F })
                                   .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
                                   .background(theme::ColourValue(theme::ColourRole::label).opacity(0.05F), theme::Shape::rounded(12.0F)))
            .default_scroll_anchor(motion::UnitPoint::bottom))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity, .alignment = composition::Alignment::top_leading })
        .task([held] { return follow(held); })
        .id(composition::ElementKey{ "discovery-log" });
}

}
