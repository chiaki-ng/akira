#include "play/chiaki_core.hpp"
#include "log/log.hpp"
#include "accounts/account_settings.hpp"
#include "cloud/cloud_settings.hpp"
#include "consoles/console_settings.hpp"
#include "settings/developer_settings.hpp"
#include "settings/settings_store.hpp"
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

ChiakiCore::ChiakiCore(Settings settings, app::UiDispatcher& dispatcher, std::function<void()> signInRenewed)
    : settings(settings),
      dispatcher(dispatcher),
      signInRenewed(std::move(signInRenewed)),
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
      datacenters((app::CrashReport::stage("core datacenters"), settings.cloud.datacenters()), settings.store),
      chiakiNg((app::CrashReport::stage("core open"), libchiaking::ChiakiNg::open(
          factory, libchiaking::ChiakiNg::Stores{settings.accounts.signIns(), settings.consoles.pairings(), datacenters, nullptr,
                                                 recordStore})))
{
    app::CrashReport::stage("core ready");
    if (instance != nullptr)
        std::terminate();
    instance = this;
    settings.developer.setCoreLog(&coreLog);
    app::CrashReport::stage("core duid");
    settings.cloud.ensureDuid(random());
    app::CrashReport::stage("core remember saved");
    rememberSaved();
}

ChiakiCore::~ChiakiCore()
{
    settings.developer.setCoreLog(nullptr);
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

    psnAccounts.emplace(*chiakiNg, settings.accounts, threadingProvider, steadyClock, random(), dispatcher);
    if (signInRenewed)
        psnAccounts->setRenewedObserver(signInRenewed);
}

}
