#ifndef AKIRA_CLOUD_CONNECT_TASK_HPP
#define AKIRA_CLOUD_CONNECT_TASK_HPP

#include <memory>
#include <mutex>
#include <string>

#include <libchiaking/platform/platform.hpp>

#include "cloud/cloud_launch.hpp"
#include "cloud/models.hpp"
#include "play/connect_task.hpp"

namespace akira::accounts {
class AccountSettings;
}

namespace akira::app {
class AppSettings;
}

namespace akira::cloud {

class Service;

class CloudConnectTask : public play::ConnectTask {
public:
    CloudConnectTask(Service& service, const accounts::AccountSettings& accounts, const app::AppSettings& app,
                     const Game& game);
    CloudConnectTask(Service& service, const accounts::AccountSettings& accounts, const app::AppSettings& app,
                     std::shared_ptr<CloudLaunch> cleared);

    static auto retrying(Service& service, const accounts::AccountSettings& accounts, const app::AppSettings& app,
                         const CloudLaunch& previous) -> std::unique_ptr<CloudConnectTask>;
    ~CloudConnectTask() override;

    std::string title() const override;
    const char* logKind() const override { return "cloud"; }

    void start(play::ConnectSink& sink) override;
    void cancel() override;

private:
    struct CallbackState {
        std::mutex mutex;
        play::ConnectSink* sink = nullptr;
        libchiaking::platform::StopSource stop;
    };

    static void handOver(const std::shared_ptr<CallbackState>& state, std::shared_ptr<CloudLaunch> launch,
                         const accounts::AccountSettings& accounts, const app::AppSettings& app);

    Service& service;
    const accounts::AccountSettings& accounts;
    const app::AppSettings& appSettings;
    Game game;
    std::shared_ptr<CloudLaunch> clearedLaunch;
    std::shared_ptr<CallbackState> callbackState;
};

} // namespace akira::cloud

#endif // AKIRA_CLOUD_CONNECT_TASK_HPP
