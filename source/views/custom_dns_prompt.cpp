#include "views/custom_dns_prompt.hpp"

#include "core/settings_manager.hpp"
#include "util/custom_dns.hpp"

#include <borealis.hpp>

using namespace brls::literals;

void CustomDnsPrompt::open(std::function<void()> onSaved)
{
    auto* settings = SettingsManager::getInstance();
    brls::Application::getImeManager()->openForText(
        [settings, onSaved = std::move(onSaved)](std::string text) {
            auto servers = akira::net::CustomDns::parseServers(text);
            if (!servers) {
                reject();
                return;
            }
            settings->setCustomDnsServers(std::move(*servers));
            settings->writeFile();
            if (onSaved)
                onSaved();
        },
        "akira/settings/custom_dns_prompt"_i18n,
        "akira/settings/custom_dns_placeholder"_i18n,
        31, akira::net::CustomDns::formatServers(settings->getCustomDnsServers()), 0);
}

void CustomDnsPrompt::reject()
{
    brls::sync([]() {
        auto* dialog = new brls::Dialog("akira/settings/custom_dns_invalid"_i18n);
        dialog->addButton("akira/common/ok"_i18n, [dialog]() { dialog->close(); });
        dialog->open();
    });
}
