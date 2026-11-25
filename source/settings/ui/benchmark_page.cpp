#include "settings/ui/benchmark_page.hpp"

#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/composition/scroll_anchor_role.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>
#include <vector>

namespace akira::settings::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace controls = ::arisa::controls;
namespace motion = ::arisa::motion;
namespace theme = ::arisa::theme;

BenchmarkPage::BenchmarkPage(SettingsModel& model, const app::Strings& strings)
    : model(&model)
    , strings(&strings)
{
}

auto BenchmarkPage::body(composition::Context& context) const -> composition::Element
{
    const SettingsModel::Status& status = context.read(model->view());
    std::vector<composition::Element> lines;
    for (const std::string& line : status.benchmark)
        lines.push_back(components::Text(line.empty() ? std::string(" ") : line)
                            .font(theme::Font::footnote.monospaced())
                            .foreground_style(theme::ColourRole::secondary_label));
    std::vector<composition::Element> state;
    if (status.benchmarking)
        state.push_back(controls::ProgressView());
    else
        state.push_back(components::Image(components::Image::SystemName{ "check_circle" }).foreground_style(theme::ColourRole::green));
    state.push_back(components::Text(strings->text(status.benchmarking ? "akira/benchmark/running" : "akira/benchmark/complete"))
                        .font(theme::Font::callout)
                        .foreground_style(theme::ColourRole::secondary_label));
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::Space{ 10.0F } },
        components::HStack(components::HStackOptions{ .spacing = theme::space::sm }, std::move(state))
            .id(composition::ElementKey{ "benchmark.state" }),
        components::ScrollView(components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                                      .spacing = theme::Space{ 2.0F } },
                                   std::move(lines))
                                   .padding(theme::Space{ 14.0F })
                                   .frame({ .max_width = composition::infinity, .alignment = composition::Alignment::leading })
                                   .background(theme::ColourValue(theme::ColourRole::label).opacity(0.05F), theme::Shape::rounded(12.0F)))
            .default_scroll_anchor(motion::UnitPoint::bottom, composition::ScrollAnchorRole::initial_offset)
            .default_scroll_anchor(motion::UnitPoint::bottom, composition::ScrollAnchorRole::size_changes)
            .default_scroll_anchor(motion::UnitPoint::top, composition::ScrollAnchorRole::alignment))
        .frame({ .max_width = composition::infinity, .max_height = composition::infinity, .alignment = composition::Alignment::top_leading })
        .id(composition::ElementKey{ "benchmark" });
}

}
