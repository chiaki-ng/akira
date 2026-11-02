#pragma once

#include <memory>
#include <optional>

#include <libchiaking/chiaki_ng.hpp>
#include <libchiaking/platform/stock_providers.hpp>
#include <libchiaking/platform/defaults.hpp>

#include "core/cloud_datacenter_store.hpp"
#include "core/core_log.hpp"
#include "psn/accounts.hpp"

class SettingsManager;

namespace akira {

class ChiakiCore {
public:
    using Missing = libchiaking::ChiakiNg::Missing;

    static constexpr const char* recordRoot = "sdmc:/switch/akira";

    explicit ChiakiCore(SettingsManager& settings);
    ~ChiakiCore();

    ChiakiCore(const ChiakiCore&) = delete;
    ChiakiCore& operator=(const ChiakiCore&) = delete;

    static auto current() noexcept -> ChiakiCore&;

    auto threading() noexcept -> libchiaking::platform::ThreadingProvider& { return threadingProvider; }
    auto clock() noexcept -> libchiaking::platform::ClockProvider& { return factory.clock(); }
    auto wallClock() noexcept -> libchiaking::platform::ClockProvider& { return factory.wall_clock(); }
    auto random() noexcept -> libchiaking::platform::SecureRandomProvider& { return factory.random(); }
    auto crypto() noexcept -> libchiaking::platform::CryptoProvider& { return cryptoProvider; }
    auto network() noexcept -> libchiaking::platform::NetworkProvider& { return factory.network(); }
    auto http() noexcept -> libchiaking::platform::SelectedCurlHttpProvider& { return httpProvider; }
    auto log() noexcept -> CoreLog& { return coreLog; }
    auto records() noexcept -> libchiaking::platform::RecordStore& { return recordStore; }

    auto opened() const noexcept -> bool { return chiakiNg.has_value(); }
    auto missing() const noexcept -> std::optional<Missing>;
    auto chiaki() -> libchiaking::ChiakiNg&;
    auto accounts() noexcept -> psn::Accounts* { return psnAccounts ? &*psnAccounts : nullptr; }

private:
    static auto transportSettings() noexcept -> libchiaking::platform::HttpTransportSettings;
    static auto threadPlacement() noexcept -> libchiaking::platform::horizon::ThreadPlacementPolicy;

    void rememberSaved();

    static ChiakiCore* instance;

    SettingsManager& settings;
    libchiaking::platform::horizon::ThreadingProvider threadingProvider;
    libchiaking::platform::SteadyClock steadyClock;
    libchiaking::platform::SystemClock systemClock;
    CoreLog coreLog;
    libchiaking::platform::SelectedCurlHttpProvider httpProvider;
    libchiaking::platform::CurlWebSocketProvider webSocketProvider;
    libchiaking::platform::SystemSecureRandom secureRandom;
    libchiaking::platform::libnx::LibnxCryptoProvider cryptoProvider{secureRandom};
    libchiaking::platform::DefaultPlatformFactory factory;
    libchiaking::platform::DefaultStorageProvider recordStore;
    CloudDatacenterStore datacenters;
    libchiaking::Expected<libchiaking::ChiakiNg, Missing> chiakiNg;
    std::optional<psn::Accounts> psnAccounts;
};

}
