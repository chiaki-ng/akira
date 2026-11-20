#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <arisa/composition/observable.hpp>

#include "settings/device_features.hpp"
#include "settings/maintenance.hpp"
#include "settings/pad_bench.hpp"

namespace akira::app {
class AppSettings;
class Notifier;
}

namespace akira::cloud {
class CloudSettings;
}

namespace akira::network {
class NetworkSettings;
}

namespace akira::play {
class ButtonCapture;
class ControllerSettings;
class StreamSettings;
}

namespace akira::settings {
class DeveloperSettings;
class SettingsStore;
}

namespace akira::settings::ui {

class SettingsModel {
public:
    enum class Detail { None, Mapping, Pad };
    enum class Entry { None, Subnet, Dns, UpdateServer };

    struct Parts {
        SettingsStore& store;
        DeveloperSettings& developer;
        play::StreamSettings& stream;
        play::ControllerSettings& controllers;
        network::NetworkSettings& network;
        app::AppSettings& app;
        cloud::CloudSettings& cloud;
        Maintenance& maintenance;
        PadBench& pads;
        play::ButtonCapture& capture;
        app::Notifier& notifier;
        DeviceFeatures features;
    };

    struct Status {
        int revision = 0;
        Detail detail = Detail::None;
        std::size_t pad = 0;
        std::vector<PadBench::Pad> pads;
        std::string tls;
        bool checking = false;
        std::optional<Maintenance::UpdateCheck> update;
        bool offered = false;
        std::optional<Maintenance::UpdateProgress> install;
        Entry entry = Entry::None;
        Entry rejected = Entry::None;

        friend bool operator==(const Status&, const Status&) = default;
    };

    static constexpr int unlockTaps = 7;
    static constexpr std::chrono::milliseconds tapWindow{ 3000 };

    explicit SettingsModel(Parts parts);
    ~SettingsModel();

    SettingsModel(const SettingsModel&) = delete;
    SettingsModel& operator=(const SettingsModel&) = delete;

    auto view() const -> const ::arisa::composition::Observable<Status>& { return state; }
    auto parts() const -> const Parts& { return held; }
    auto features() const -> const DeviceFeatures& { return held.features; }

    void commit();
    void open(Detail detail, std::size_t pad = 0);
    void close();
    void scanPads();

    auto subnets() const -> std::vector<std::string>;
    [[nodiscard]] auto addSubnet(const std::string& text) -> bool;
    void removeSubnet(const std::string& subnet);
    [[nodiscard]] auto setDns(const std::string& text) -> bool;
    auto tapVersion() -> bool;

    void ask(Entry entry);
    auto initial(Entry entry) const -> std::string;
    void submit(const std::string& text);
    void dismissEntry();
    void dismissRejection();

    void probeTls();
    void checkForUpdate();
    void simulateUpdate(const std::string& server);
    void installUpdate();
    void dismissOffer();
    void dismissInstall();

private:
    static auto split(const std::string& text) -> std::vector<std::string>;
    static auto join(const std::vector<std::string>& subnets) -> std::string;
    void saveSubnets(const std::vector<std::string>& subnets);
    void offer(Maintenance::UpdateCheck check);

    Parts held;
    ::arisa::composition::Observable<Status> state;
    int taps = 0;
    std::chrono::steady_clock::time_point lastTap{};
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}
