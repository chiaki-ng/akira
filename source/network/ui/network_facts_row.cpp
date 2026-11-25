#include "network/ui/network_facts_row.hpp"

#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <utility>

namespace akira::network::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

NetworkFactsRow::NetworkFactsRow(NetworkFacts facts, const app::Strings& strings)
    : facts(std::move(facts))
    , strings(&strings)
{
}

auto NetworkFactsRow::body(composition::Context&) const -> composition::Element
{
    return components::HStack(components::HStackOptions{ .alignment = components::VerticalAlignment::top,
                                  .spacing = theme::Space{ factSpacing } },
        fact(strings->text("akira/network/fact_ip"), components::Text(orMissing(facts.address))),
        fact(strings->text("akira/network/fact_system_dns"), components::Text(orMissing(joined(facts.systemDns)))),
        fact(strings->text("akira/network/fact_akira_dns"), components::Text(orMissing(joined(facts.akiraDns)))),
        fact(strings->text("akira/network/fact_psn"), psn()))
        .line_limit(1)
        .id(composition::ElementKey{ "network.facts" });
}

auto NetworkFactsRow::fact(std::string label, composition::Element value) -> composition::Element
{
    return components::VStack(components::VStackOptions{ .alignment = components::HorizontalAlignment::leading,
                                  .spacing = theme::space::xxs },
        components::Text(std::move(label)).font(theme::Font::caption).foreground_style(theme::ColourRole::tertiary_label),
        std::move(value).font(theme::Font::footnote.monospaced_digit()).foreground_style(theme::ColourRole::secondary_label));
}

auto NetworkFactsRow::joined(const std::vector<std::string>& servers) -> std::string
{
    std::string text;
    for (const std::string& server : servers)
        text += (text.empty() ? "" : ", ") + server;
    return text;
}

auto NetworkFactsRow::orMissing(std::string text) const -> std::string
{
    return text.empty() ? strings->text("akira/network/fact_none") : text;
}

auto NetworkFactsRow::psn() const -> composition::Element
{
    switch (facts.psn)
    {
        case NetworkFacts::Psn::Reachable:
        case NetworkFacts::Psn::Unreachable:
        {
            const bool up = facts.psn == NetworkFacts::Psn::Reachable;
            const theme::ColourRole tone = up ? theme::ColourRole::green : theme::ColourRole::red;
            return components::HStack(components::HStackOptions{ .spacing = theme::space::xs },
                components::ShapeView(theme::Shape::circle()).foreground_style(tone).frame({ .width = dotSize, .height = dotSize }),
                components::Text(strings->text(up ? "akira/network/psn_reachable" : "akira/network/psn_unreachable"))
                    .foreground_style(tone));
        }
        case NetworkFacts::Psn::Skipped: return components::Text(strings->text("akira/network/psn_skipped"));
        case NetworkFacts::Psn::Checking: break;
    }
    return components::Text(strings->text("akira/network/psn_checking"));
}

}
