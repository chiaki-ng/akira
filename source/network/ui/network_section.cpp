#include "network/ui/network_section.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>

#include <string>
#include <vector>

#include "network/custom_dns.hpp"
#include "network/network_settings.hpp"
#include "settings/ui/setting_rows.hpp"

namespace akira::network::ui {

namespace components = ::arisa::components;
namespace composition = ::arisa::composition;
namespace theme = ::arisa::theme;

NetworkSection::NetworkSection(settings::ui::SettingsModel& model, const settings::HelpCatalog& help, const app::Strings& strings)
    : model(&model)
    , help(&help)
    , strings(&strings)
{
}

auto NetworkSection::body(composition::Context& context) const -> composition::Element
{
    static_cast<void>(context.read(model->view()));
    const settings::ui::SettingRows rows(*model, *help, *strings);
    settings::ui::SettingsModel* edit = model;
    std::vector<composition::Element> shown;
    const std::string local = model->parts().maintenance.localSubnet();
    if (!local.empty())
        shown.push_back(rows.value(local, rows.text("akira/settings/this_network")));
    for (const std::string& subnet : model->subnets())
        shown.push_back(components::Button([edit, subnet] { edit->removeSubnet(subnet); },
            components::HStack(components::Text(subnet), components::Spacer(),
                components::Text(rows.text("akira/settings/remove")).foreground_style(theme::ColourRole::red)))
                            .id(composition::ElementKey{ "network.subnet." + subnet }));
    shown.push_back(rows.action(rows.text("akira/settings/add_subnet_label"), "add", "",
        [edit] { edit->ask(settings::ui::SettingsModel::Entry::Subnet); }));
    const auto& servers = model->parts().network.customDnsServers();
    shown.push_back(rows.link(rows.text("akira/settings/custom_dns"),
        servers.empty() ? rows.text("akira/settings/custom_dns_system") : CustomDns::formatServers(servers), "",
        [edit] { edit->ask(settings::ui::SettingsModel::Entry::Dns); }));
    return rows.section(rows.text("akira/settings/network"), std::move(shown));
}

}
