#include "network/views/dns_warning_dialog.hpp"

#include <borealis/core/i18n.hpp>

#include "log/log.hpp"
#include "network/sony_dns_checker.hpp"
#include "settings/settings_manager.hpp"

namespace akira::network::views {

DnsWarningDialog::DnsWarningDialog(std::string message)
    : brls::Dialog(std::move(message))
{
    liveState->dialog = this;
    setCancelable(false);
    addButton(brls::getStr("akira/common/ok"), [] {});
    button1->setActionAvailable(brls::BUTTON_A, false);
    button1->setState(brls::ButtonState::DISABLED);
}

DnsWarningDialog::~DnsWarningDialog()
{
    liveState->dialog = nullptr;
}

int DnsWarningDialog::remainingSeconds() const
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - openedAt).count();
    return secondsUntilDnsWarningOk(elapsed);
}

void DnsWarningDialog::updateCountdown()
{
    const int remaining = remainingSeconds();
    if (remaining == lastRemaining)
        return;
    lastRemaining = remaining;
    button1->setText(remaining > 0
        ? brls::getStr("akira/network/dns_ok_in", remaining)
        : brls::getStr("akira/common/ok"));
    button1->setState(remaining > 0
        ? brls::ButtonState::DISABLED : brls::ButtonState::ENABLED);
    button1->setActionAvailable(brls::BUTTON_A, remaining == 0);
}

void DnsWarningDialog::show()
{
    openedAt = std::chrono::steady_clock::now();
    updateCountdown();
    const std::weak_ptr<LiveState> weakState = liveState;
    brls::Application::getRunLoopEvent()->subscribe([weakState] {
        if (const auto state = weakState.lock(); state && state->dialog)
            state->dialog->updateCountdown();
    });
    open();
}

void DnsWarningDialog::checkAtStartup()
{
    if (settings::SettingsManager::getInstance()->accounts().isLegacyProfileActive())
        return;

    brls::async([] {
        const auto report = checkSonyDnsAtStartup();
        if (!report.failed())
            return;

        std::string failedDomain;
        std::string failedServer;
        // Prefer a concrete poisoned DNS answer over an ambiguous timeout.
        for (const auto verdict : {DnsVerdict::Blocked, DnsVerdict::Inconclusive}) {
            for (const auto& server : report.servers) {
                for (size_t i = 0; i < server.domains.size(); ++i) {
                    if (server.domains[i] != verdict)
                        continue;
                    failedDomain = SonyDnsDomains[i];
                    failedServer = server.address;
                    break;
                }
                if (!failedDomain.empty())
                    break;
            }
            if (!failedDomain.empty())
                break;
        }
        log::warning("[NET] Sony DNS check failed domain={} server={}",
            failedDomain, failedServer);
        brls::sync([failedDomain = std::move(failedDomain),
                    failedServer = std::move(failedServer)] {
            (new DnsWarningDialog(
                brls::getStr("akira/network/dns_failed", failedDomain, failedServer)))->show();
        });
    }, true);
}

}
