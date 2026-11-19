#include "network/views/custom_dns_prompt.hpp"

#include "app/view_services.hpp"
#include "network/custom_dns.hpp"

#include <borealis.hpp>

namespace akira::network::views {

using namespace brls::literals;

void CustomDnsPrompt::open(std::function<void()> onSaved)
{
    auto* services = &app::ViewServices::current();
    brls::Application::getImeManager()->openForText(
        [services, onSaved = std::move(onSaved)](std::string text) {
            auto servers = CustomDns::parseServers(text);
            if (!servers) {
                reject();
                return;
            }
            services->networkSettings.setCustomDnsServers(std::move(*servers));
            services->store.save();
            if (onSaved)
                onSaved();
        },
        "akira/settings/custom_dns_prompt"_i18n,
        "akira/settings/custom_dns_placeholder"_i18n,
        31, CustomDns::formatServers(services->networkSettings.customDnsServers()), 0);
}

void CustomDnsPrompt::reject()
{
    brls::sync([]() {
        auto* dialog = new brls::Dialog("akira/settings/custom_dns_invalid"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
        dialog->open();
    });
}

}
