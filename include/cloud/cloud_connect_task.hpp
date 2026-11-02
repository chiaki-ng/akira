#ifndef AKIRA_CLOUD_CONNECT_TASK_HPP
#define AKIRA_CLOUD_CONNECT_TASK_HPP

#include <memory>
#include <mutex>
#include <string>

#include <libchiaking/platform/platform.hpp>

#include "cloud/cloud_launch.hpp"
#include "cloud/models.hpp"
#include "views/connect_task.hpp"

namespace cloud {

class CloudConnectTask : public akira::views::ConnectTask {
public:
    explicit CloudConnectTask(const Game& game);
    explicit CloudConnectTask(std::shared_ptr<CloudLaunch> cleared);

    static auto retrying(const CloudLaunch& previous) -> std::unique_ptr<CloudConnectTask>;
    ~CloudConnectTask() override;

    std::string title() const override;
    const char* logKind() const override { return "cloud"; }

    void start(akira::views::ConnectSink& sink) override;
    void cancel() override;

private:
    struct CallbackState {
        std::mutex mutex;
        akira::views::ConnectSink* sink = nullptr;
        libchiaking::platform::StopSource stop;
    };

    static void handOver(const std::shared_ptr<CallbackState>& state, std::shared_ptr<CloudLaunch> launch);

    Game game;
    std::shared_ptr<CloudLaunch> clearedLaunch;
    std::shared_ptr<CallbackState> callbackState;
};

} // namespace cloud

#endif // AKIRA_CLOUD_CONNECT_TASK_HPP
