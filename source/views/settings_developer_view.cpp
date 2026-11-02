#include "views/settings_developer_view.hpp"
#include "views/host_list_tab.hpp"
#include "views/log_browser_view.hpp"
#include "views/update_flow.hpp"

#include <borealis/core/i18n.hpp>

#include <libchiaking/play_types.hpp>

#include "util/http.hpp"
#include "util/http_pool.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>

using namespace brls::literals;

namespace {

void runTlsVerifyProbe(HttpSession& session, brls::DetailCell* cell, std::shared_ptr<std::atomic<bool>> alive) {
    auto show = [cell, alive](const std::string& text) {
        brls::sync([cell, alive, text]() {
            if (alive->load())
                cell->setDetailText(text);
        });
    };

    struct Case { const char* url; bool expectOk; const char* what; };
    static const Case cases[] = {
        { "https://badssl.com/",                true,  "control (valid chain)" },
        { "https://expired.badssl.com/",        false, "expired certificate" },
        { "https://wrong.host.badssl.com/",     false, "hostname mismatch" },
        { "https://self-signed.badssl.com/",    false, "self-signed" },
        { "https://untrusted-root.badssl.com/", false, "untrusted root" },
    };

    brls::Logger::info("tlsprobe: ===== TLS verification probe (certinfo path) =====");
    show("running...");
    int failures = 0;
    int done = 0;
    for (const auto& c : cases) {
        HttpRequest req;
        req.url = c.url;
        req.certInfo = true;
        req.timeoutSec = 15;
        req.followLocation = false;

        HttpResponse res = session.perform(req);
        bool connected = !res.transportFailed();
        bool asExpected = (connected == c.expectOk);
        if (!asExpected) failures++;

        brls::Logger::info("tlsprobe: {} {} -> {} ({})",
            asExpected ? "PASS" : "FAIL", c.what,
            connected ? "connected" : "rejected",
            res.error.empty() ? "no transport error" : res.error);

        done++;
        show(fmt::format("{}/{} checked, {} unexpected - {} {}",
            done, (int)(sizeof(cases) / sizeof(cases[0])), failures,
            asExpected ? "ok:" : "UNEXPECTED:", c.what));
    }

    if (failures == 0)
    {
        brls::Logger::info("tlsprobe: ===== all cases as expected: the service verifies this path =====");
        show("PASS - service verifies CA, hostname and expiry");
    }
    else
    {
        brls::Logger::error("tlsprobe: ===== {} case(s) UNEXPECTED: do not rely on the service here =====", failures);
        show(fmt::format("FAIL - {} of 5 unexpected, see log", failures));
    }
}

} // namespace

SettingsDeveloperView::~SettingsDeveloperView() {
    alive->store(false);
}

void SettingsDeveloperView::initTakionSelector(brls::SelectorCell* cell, TakionTarget target, std::string title,
    std::vector<std::uint32_t> implemented) {
    const int current = settings->getTakionVersion(target);
    std::vector<int> versions{0};
    for (const std::uint32_t version : implemented)
        versions.push_back(static_cast<int>(version));
    if (current != 0 && std::ranges::find(versions, current) == versions.end())
        versions.push_back(current);

    std::vector<std::string> labels{"akira/settings/dev_takion_version_auto"_i18n};
    for (size_t i = 1; i < versions.size(); i++)
        labels.push_back("v" + std::to_string(versions[i]));

    const auto selected = std::ranges::find(versions, current);
    cell->init(std::move(title), labels, static_cast<int>(selected - versions.begin()),
        [this, target, versions](int index) {
            settings->setTakionVersion(target, versions[static_cast<size_t>(index)]);
            settings->writeFile();
        });
}

SettingsDeveloperView::SettingsDeveloperView() {
    this->inflateFromXMLRes("xml/settings/developer.xml");

    settings = SettingsManager::getInstance();

    fakeHostsToggle->init(
        "akira/settings/dev_fake_hosts"_i18n,
        settings->getDevFakeHosts(),
        [this](bool isOn) {
            settings->setDevFakeHosts(isOn);
            settings->writeFile();
            HostListTab::notifyActiveProfileChanged();
        });

    using libchiaking::play::DeveloperTuning;
    initTakionSelector(takionPs4Selector, TakionTarget::PS4, "akira/settings/dev_takion_version_ps4"_i18n,
        DeveloperTuning::implemented_protocol_versions(libchiaking::consoles::Generation::ps4));
    initTakionSelector(takionPs5Selector, TakionTarget::PS5, "akira/settings/dev_takion_version_ps5"_i18n,
        DeveloperTuning::implemented_protocol_versions(libchiaking::consoles::Generation::ps5));
    initTakionSelector(takionPs5CloudSelector, TakionTarget::PS5Cloud,
        "akira/settings/dev_takion_version_ps5_cloud"_i18n,
        DeveloperTuning::implemented_protocol_versions(libchiaking::catalog::ServiceGeneration::ps5_cloud)
            | std::views::filter([](std::uint32_t version) { return version <= highestCloudTakion; })
            | std::ranges::to<std::vector>());
    initTakionSelector(takionPsNowSelector, TakionTarget::PSNow, "akira/settings/dev_takion_version_ps_now"_i18n,
        DeveloperTuning::implemented_protocol_versions(libchiaking::catalog::ServiceGeneration::old_service));

    simulateUpdateCell->setText("akira/settings/simulate_update"_i18n);
    tlsVerifyProbeCell->setText("akira/settings/dev_tls_verify_probe"_i18n);
    logBrowserCell->setText("akira/settings/dev_logs"_i18n);
    logBrowserCell->setDetailText(SettingsManager::LOG_DIR);
    logBrowserCell->registerClickAction([](brls::View*) {
        brls::Application::pushActivity(
            new brls::Activity(new LogBrowserView()),
            brls::TransitionAnimation::NONE);
        return true;
    });
    tlsVerifyProbeCell->registerClickAction([this](brls::View*) {
        brls::DetailCell* cell = this->tlsVerifyProbeCell;
        HttpPool::instance().submit([cell, guard = alive](HttpSession& session) {
            runTlsVerifyProbe(session, cell, guard);
        });
        return true;
    });

    simulateUpdateCell->registerClickAction([](brls::View*) {
        akira::UpdateFlow::simulate();
        return true;
    });
}
