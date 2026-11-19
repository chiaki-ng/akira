#include "play/chiaki_core.hpp"
#include "log/log.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "settings/settings_manager.hpp"
#include "app/crash_report.hpp"

#include <chrono>
#include <cstdint>
#include <exception>

namespace akira::play {

namespace platform = libchiaking::platform;

ChiakiCore* ChiakiCore::instance = nullptr;

auto ChiakiCore::threadPlacement() noexcept -> platform::horizon::ThreadPlacementPolicy
{
    platform::horizon::ThreadPlacementPolicy policy;
    const auto base = policy.placement(platform::ThreadRole::unspecified);
    if (const auto background = platform::horizon::ThreadPlacement::make(
            base.core(), static_cast<std::uint8_t>(base.priority() + 1), base.stack_size()))
        policy.place(platform::ThreadRole::unspecified, *background);
    return policy;
}

ChiakiCore::ChiakiCore(SettingsManager& settings)
    : settings(settings),
      threadingProvider((app::CrashReport::stage("core threading"), threadPlacement())),
      httpProvider((app::CrashReport::stage("core http"), transportSettings()), threadingProvider, steadyClock, &coreLog),
      webSocketProvider((app::CrashReport::stage("core websocket"), threadingProvider), steadyClock),
      factory((app::CrashReport::stage("core factory"), platform::PlatformProviders{
          .threading = &threadingProvider,
          .clock = &steadyClock,
          .wall_clock = &systemClock,
          .random = &secureRandom,
          .crypto = &cryptoProvider,
          .http = &httpProvider,
          .websocket = &webSocketProvider,
          .diagnostics = &coreLog,
      })),
      recordStore((app::CrashReport::stage("core records"), recordRoot)),
      datacenters((app::CrashReport::stage("core datacenters"), settings)),
      chiakiNg((app::CrashReport::stage("core open"), libchiaking::ChiakiNg::open(
          factory, libchiaking::ChiakiNg::Stores{settings.signIns(), settings.pairings(), datacenters, nullptr,
                                                 recordStore})))
{
    app::CrashReport::stage("core ready");
    if (instance != nullptr)
        std::terminate();
    instance = this;
    settings.setCoreLog(&coreLog);
    app::CrashReport::stage("core duid");
    settings.ensureCloudDuid(random());
    app::CrashReport::stage("core remember saved");
    rememberSaved();
}

ChiakiCore::~ChiakiCore()
{
    settings.setCoreLog(nullptr);
    instance = nullptr;
}

auto ChiakiCore::current() noexcept -> ChiakiCore&
{
    if (instance == nullptr)
        std::terminate();
    return *instance;
}

auto ChiakiCore::missing() const noexcept -> std::optional<Missing>
{
    if (chiakiNg)
        return std::nullopt;
    return chiakiNg.error();
}

auto ChiakiCore::chiaki() -> libchiaking::ChiakiNg&
{
    return chiakiNg.value();
}

auto ChiakiCore::transportSettings() noexcept -> platform::HttpTransportSettings
{
    return platform::HttpTransportSettings{
        .transport = platform::HttpTransport::multi,
        .multi = {.max_host_connections = 2, .max_total_connections = 6, .dns_cache_lifetime = std::chrono::seconds{0}},
        .pool = {.idle_handles = 2, .connections_per_handle = 1},
        .tls_handshake_blocks = true,
    };
}

void ChiakiCore::rememberSaved()
{
    if (!chiakiNg) {
        log::error("[CORE] ChiakiNg did not open: {} provider missing",
            chiakiNg.error() == Missing::crypto ? "crypto" : "http");
        return;
    }

    if (auto returned = chiakiNg->users().welcome_back())
        log::info("[CORE] remembered {} user(s), skipped {}", returned->users.size(), returned->rejected);
    else
        log::error("[CORE] saved users could not be read");

    if (auto remembered = chiakiNg->consoles().remember_saved())
        log::info("[CORE] remembered {} console(s), skipped {}", remembered->consoles.size(),
            remembered->rejected);
    else
        log::error("[CORE] saved consoles could not be read");

    psnAccounts.emplace(*chiakiNg, settings, threadingProvider, steadyClock, random());
#if AKIRA_CLOUD
    psnAccounts->setRenewedObserver([] { cloud::Service::instance().refreshInBackground(); });
#endif
}

}
