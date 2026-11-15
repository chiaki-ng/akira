#include "core/chiaki_core.hpp"
#if AKIRA_CLOUD
#include "cloud/service.hpp"
#endif
#include "core/settings_manager.hpp"
#include "util/crash_report.hpp"

#include <borealis.hpp>

#include <chrono>
#include <cstdint>
#include <exception>

namespace akira {

namespace platform = libchiaking::platform;

ChiakiCore* ChiakiCore::instance = nullptr;

auto ChiakiCore::threadPlacement() noexcept -> platform::horizon::ThreadPlacementPolicy
{
    platform::horizon::ThreadPlacementPolicy policy;
    const auto session = policy.placement(platform::ThreadRole::session);
    if (const auto background = platform::horizon::ThreadPlacement::make(
            session.core(), static_cast<std::uint8_t>(session.priority() + 1), session.stack_size()))
        policy.place(platform::ThreadRole::unspecified, *background);
    return policy;
}

ChiakiCore::ChiakiCore(SettingsManager& settings)
    : settings(settings),
      threadingProvider((CrashReport::stage("core threading"), threadPlacement())),
      httpProvider((CrashReport::stage("core http"), transportSettings()), threadingProvider, steadyClock, &coreLog),
      webSocketProvider((CrashReport::stage("core websocket"), threadingProvider), steadyClock),
      factory((CrashReport::stage("core factory"), platform::PlatformProviders{
          .threading = &threadingProvider,
          .clock = &steadyClock,
          .wall_clock = &systemClock,
          .random = &secureRandom,
          .crypto = &cryptoProvider,
          .http = &httpProvider,
          .websocket = &webSocketProvider,
          .diagnostics = &coreLog,
      })),
      recordStore((CrashReport::stage("core records"), recordRoot)),
      datacenters((CrashReport::stage("core datacenters"), settings)),
      chiakiNg((CrashReport::stage("core open"), libchiaking::ChiakiNg::open(
          factory, libchiaking::ChiakiNg::Stores{settings.signIns(), settings.pairings(), datacenters, nullptr,
                                                 recordStore})))
{
    CrashReport::stage("core ready");
    if (instance != nullptr)
        std::terminate();
    instance = this;
    settings.setCoreLog(&coreLog);
    CrashReport::stage("core duid");
    settings.ensureCloudDuid(random());
    CrashReport::stage("core remember saved");
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
        brls::Logger::error("[CORE] ChiakiNg did not open: {} provider missing",
            chiakiNg.error() == Missing::crypto ? "crypto" : "http");
        return;
    }

    if (auto returned = chiakiNg->users().welcome_back())
        brls::Logger::info("[CORE] remembered {} user(s), skipped {}", returned->users.size(), returned->rejected);
    else
        brls::Logger::error("[CORE] saved users could not be read");

    if (auto remembered = chiakiNg->consoles().remember_saved())
        brls::Logger::info("[CORE] remembered {} console(s), skipped {}", remembered->consoles.size(),
            remembered->rejected);
    else
        brls::Logger::error("[CORE] saved consoles could not be read");

    psnAccounts.emplace(*chiakiNg, settings, threadingProvider, steadyClock, random());
#if AKIRA_CLOUD
    psnAccounts->setRenewedObserver([] { cloud::Service::instance().refreshInBackground(); });
#endif
}

}
