#include "network/views/custom_dns_prompt.hpp"

#include "settings/settings_manager.hpp"
#include "network/custom_dns.hpp"

#include <borealis.hpp>

namespace akira::network::views {

using namespace brls::literals;

void CustomDnsPrompt::open(std::function<void()> onSaved)
{
    auto* settings = settings::SettingsManager::getInstance();
    brls::Application::getImeManager()->openForText(
        [settings, onSaved = std::move(onSaved)](std::string text) {
            auto servers = CustomDns::parseServers(text);
            if (!servers) {
                reject();
                return;
            }
            settings->network().setCustomDnsServers(std::move(*servers));
            settings->store().save();
            if (onSaved)
                onSaved();
        },
        "akira/settings/custom_dns_prompt"_i18n,
        "akira/settings/custom_dns_placeholder"_i18n,
        31, CustomDns::formatServers(settings->network().customDnsServers()), 0);
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
