#include "app/arisa/upkeep.hpp"

#include <cstdio>
#include <ctime>
#include <format>
#include <iterator>
#include <utility>

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "app/ghash_benchmark.hpp"
#include "app/notifier.hpp"
#include "app/strings.hpp"
#include "app/ui_dispatcher.hpp"
#include "app/version.hpp"
#include "consoles/discovery_manager.hpp"
#include "log/log.hpp"
#include "platform/factory.hpp"
#include "play/chiaki_core.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"
#include "trophies/trophy_manager.hpp"
#include "util/http.hpp"
#include "util/http_pool.hpp"

namespace akira::app::arisa {

Upkeep::Upkeep(Services& services, std::function<void()> refreshConsoles, std::function<void()> stop)
    : services(services)
    , refreshConsoles(std::move(refreshConsoles))
    , stop(std::move(stop))
{
}

Upkeep::~Upkeep()
{
    alive->store(false);
    cancelled->store(true);
    stopBenchmark();
}

auto Upkeep::discoveryLog() const -> DiscoveryLog
{
    return DiscoveryLog{ .version = consoles::DiscoveryManager::getDiscoveryLogVersion(),
        .lines = consoles::DiscoveryManager::getDiscoveryLogSnapshot(),
        .enabled = services.developer.discoveryLog() };
}

auto Upkeep::discoveryLogVersion() const -> std::uint64_t
{
    return consoles::DiscoveryManager::getDiscoveryLogVersion();
}

void Upkeep::clearDiscoveryLog()
{
    consoles::DiscoveryManager::clearDiscoveryLog();
}

void Upkeep::runBenchmark(std::function<void(std::string)> line, std::function<void()> done)
{
    stopBenchmark();
    play::ChiakiCore& core = services.core;
    auto deliver = [this, line](std::string text) { post([line, text = std::move(text)] { line(text); }); };
    auto started = libchiaking::platform::Thread::start(core.threading(), "benchmark", libchiaking::platform::ThreadRole::crypto,
        [this, &core, deliver, done](libchiaking::platform::StopToken token) {
            GhashBenchmark(core.crypto(), core.clock(), services.strings).run(token, deliver);
            post(done);
        });
    if (!started)
    {
        line(services.strings.text("akira/benchmark/error_thread"));
        done();
        return;
    }
    benchmark = std::move(started).value();
}

void Upkeep::stopBenchmark()
{
    static_cast<void>(benchmark.request_stop());
    benchmark = libchiaking::platform::Thread{};
}

void Upkeep::post(std::function<void()> work)
{
    services.dispatcher.post([guard = alive, work = std::move(work)] {
        if (guard->load())
            work();
    });
}

auto Upkeep::legacyProfile() const -> bool
{
    return services.accounts.isLegacyProfileActive();
}

auto Upkeep::localSubnet() const -> std::string
{
    return services.discovery.getLocalSubnetCidr();
}

void Upkeep::restartDiscovery()
{
    if (!services.discovery.isServiceEnabled())
        return;
    services.discovery.setServiceEnabled(false);
    services.discovery.setServiceEnabled(true);
}

void Upkeep::flushTrophyCache()
{
    services.trophies.flushCache();
}

void Upkeep::accountNameDisplayChanged()
{
    if (refreshConsoles)
        refreshConsoles();
}

void Upkeep::consolesChanged()
{
    if (refreshConsoles)
        refreshConsoles();
}

void Upkeep::runTlsProbe(HttpSession& session, const std::function<void(std::string)>& show)
{
    struct Case {
        const char* url;
        bool expectOk;
        const char* what;
    };
    static constexpr Case cases[] = {
        { "https://badssl.com/", true, "control (valid chain)" },
        { "https://expired.badssl.com/", false, "expired certificate" },
        { "https://wrong.host.badssl.com/", false, "hostname mismatch" },
        { "https://self-signed.badssl.com/", false, "self-signed" },
        { "https://untrusted-root.badssl.com/", false, "untrusted root" },
    };
    log::info("tlsprobe: ===== TLS verification probe (certinfo path) =====");
    show("running...");
    int failures = 0;
    int done = 0;
    for (const Case& probe : cases)
    {
        HttpRequest request;
        request.url = probe.url;
        request.certInfo = true;
        request.timeoutSec = 15;
        request.followLocation = false;
        const HttpResponse response = session.perform(request);
        const bool connected = !response.transportFailed();
        const bool expected = connected == probe.expectOk;
        if (!expected)
            ++failures;
        log::info("tlsprobe: {} {} -> {} ({})", expected ? "PASS" : "FAIL", probe.what, connected ? "connected" : "rejected",
            response.error.empty() ? "no transport error" : response.error);
        ++done;
        show(std::format("{}/{} checked, {} unexpected - {} {}", done, std::size(cases), failures,
            expected ? "ok:" : "UNEXPECTED:", probe.what));
    }
    if (failures == 0)
    {
        log::info("tlsprobe: ===== all cases as expected: the service verifies this path =====");
        show("PASS - service verifies CA, hostname and expiry");
        return;
    }
    log::error("tlsprobe: ===== {} case(s) UNEXPECTED: do not rely on the service here =====", failures);
    show(std::format("FAIL - {} of 5 unexpected, see log", failures));
}

void Upkeep::probeTls(std::function<void(std::string)> report)
{
    services.http.submit([this, guard = alive, report = std::move(report)](HttpSession& session) {
        if (!guard->load())
            return;
        runTlsProbe(session, [this, guard, report](std::string text) {
            if (guard->load())
                post([report, text = std::move(text)] { report(text); });
        });
    });
}

void Upkeep::checkForUpdate(std::string channel, std::function<void(UpdateCheck)> done)
{
    platform::UpdateProvider& updates = services.platform.updates();
    services.http.submit([this, guard = alive, channel = std::move(channel), done = std::move(done), &updates](HttpSession&) {
        platform::UpdateInfo info = updates.checkForUpdate(channel);
        if (!guard->load())
            return;
        post([this, info = std::move(info), done] {
            if (!info.error.empty())
            {
                done(UpdateCheck{ .outcome = UpdateCheck::Outcome::Failed });
                return;
            }
            if (!info.available)
            {
                done(UpdateCheck{ .outcome = UpdateCheck::Outcome::UpToDate });
                return;
            }
            offered = info;
            done(UpdateCheck{ .outcome = UpdateCheck::Outcome::Available, .version = info.version });
        });
    });
}

void Upkeep::checkOnLaunch(Notifier& notifier)
{
    if (!services.appSettings.autoCheckUpdates() || app::version::isDev())
        return;
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    const std::int64_t last = services.appSettings.lastUpdateCheck();
    if (last != 0 && now - last < launchCheckInterval)
        return;
    services.appSettings.setLastUpdateCheck(now);
    services.store.save();
    checkForUpdate(services.appSettings.updateChannel(), [this, &notifier](UpdateCheck check) {
        if (check.outcome == UpdateCheck::Outcome::Available)
            notifier.notify(services.strings.format("akira/update/available", check.version));
    });
}

void Upkeep::simulateUpdate(std::string server, std::function<void(UpdateCheck)> done)
{
    std::string base = server;
    if (!base.starts_with("http://") && !base.starts_with("https://"))
        base = "http://" + base;
    platform::UpdateInfo info;
    info.available = true;
    info.channel = services.appSettings.updateChannel();
    info.version = "9.9.9";
    info.tag = "v9.9.9";
    info.notes = "Developer-simulated update from " + base;
    info.nroUrl = base + "/akira.nro";
    info.sha256Url = base + "/SHA256SUMS";
    offered = info;
    done(UpdateCheck{ .outcome = UpdateCheck::Outcome::Available, .version = info.version });
}

void Upkeep::install(const platform::UpdateInfo& info, const std::function<void(UpdateProgress)>& progress)
{
    platform::UpdateProvider& updates = services.platform.updates();
    auto report = [this, progress](UpdateProgress step) { post([progress, step = std::move(step)] { progress(step); }); };
    const std::string sha = info.sha256.empty() ? updates.fetchExpectedSha256(info) : info.sha256;
    int last = -1;
    const auto downloaded = updates.download(info, [&](std::int64_t received, std::int64_t total) {
        if (cancelled->load())
            return false;
        const int percent = total > 0 ? static_cast<int>((received * 100) / total) : 0;
        if (percent != last)
        {
            last = percent;
            report(UpdateProgress{ .stage = UpdateProgress::Stage::Downloading, .percent = percent });
        }
        return true;
    });
    if (!downloaded)
    {
        if (!cancelled->load())
            report(UpdateProgress{ .stage = UpdateProgress::Stage::Failed,
                .error = services.strings.text("akira/update/failed") + "\n" + downloaded.error() });
        return;
    }
    if (cancelled->load())
    {
        std::remove(downloaded->c_str());
        return;
    }
    report(UpdateProgress{ .stage = UpdateProgress::Stage::Verifying });
    if (auto verified = updates.verify(*downloaded, sha, info.size); !verified)
    {
        std::remove(downloaded->c_str());
        report(UpdateProgress{ .stage = UpdateProgress::Stage::Failed,
            .error = services.strings.text("akira/update/verify_failed") + "\n" + verified.error() });
        return;
    }
    if (auto applied = updates.applyDownloaded(*downloaded); !applied)
    {
        report(UpdateProgress{ .stage = UpdateProgress::Stage::Failed,
            .error = services.strings.text("akira/update/install_failed") + "\n" + applied.error() });
        return;
    }
    report(UpdateProgress{ .stage = UpdateProgress::Stage::Installed });
}

void Upkeep::installUpdate(std::function<void(UpdateProgress)> progress)
{
    if (!offered)
    {
        progress(UpdateProgress{ .stage = UpdateProgress::Stage::Failed, .error = services.strings.text("akira/update/failed") });
        return;
    }
    cancelled->store(false);
    services.http.submit([this, guard = alive, info = *offered, progress = std::move(progress)](HttpSession&) {
        if (guard->load())
            install(info, progress);
    });
}

void Upkeep::cancelUpdate()
{
    cancelled->store(true);
}

void Upkeep::quit()
{
    if (stop)
        stop();
}

}
