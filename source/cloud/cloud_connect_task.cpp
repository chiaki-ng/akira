#include "cloud/cloud_connect_task.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include "cloud/service.hpp"
#include "core/host.hpp"

#include <string>

namespace cloud {

CloudConnectTask::CloudConnectTask(const Game& game)
    : game(game)
{
}

CloudConnectTask::CloudConnectTask(std::shared_ptr<CloudLaunch> cleared)
    : clearedLaunch(std::move(cleared))
{
    game.name = clearedLaunch->name();
}

auto CloudConnectTask::retrying(const CloudLaunch& previous) -> std::unique_ptr<CloudConnectTask>
{
    Game game;
    game.productId = previous.product().text();
    game.name = previous.name();
    game.serviceType = previous.ps5() ? "pscloud" : "psnow";
    game.streamServiceType = game.serviceType;
    return std::make_unique<CloudConnectTask>(game);
}

CloudConnectTask::~CloudConnectTask()
{
    cancel();
}

std::string CloudConnectTask::title() const
{
    return brls::getStr("akira/connection/connecting_to", game.name);
}

void CloudConnectTask::start(akira::views::ConnectSink& s)
{
    cancel();
    auto state = std::make_shared<CallbackState>();
    state->sink = &s;
    callbackState = state;

    s.progressStep(0, 0, "");

    if (clearedLaunch)
    {
        handOver(state, clearedLaunch);
        return;
    }

    Service::instance().clearForLaunch(
        game, state->stop.token(),
        [state](std::shared_ptr<CloudLaunch> launch) { handOver(state, std::move(launch)); },
        [state](const std::string& error) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->stop.stop_requested() && state->sink)
                state->sink->failed(error);
        },
        [state](libchiaking::catalog::LaunchCheck check) {
            constexpr int total = static_cast<int>(libchiaking::catalog::LaunchCheck::release) + 1;
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->stop.stop_requested() && state->sink)
                state->sink->progressStep(static_cast<int>(check) + 1, total,
                                          brls::getStr("akira/cloud/launch_starting"));
        });
}

void CloudConnectTask::handOver(const std::shared_ptr<CallbackState>& state, std::shared_ptr<CloudLaunch> launch)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->stop.stop_requested() || !state->sink)
        return;

    auto host = std::make_shared<Host>(launch->name());
    host->setCloudLaunch(*launch);
    state->sink->succeeded(host.get(), host);
}

void CloudConnectTask::cancel()
{
    if (!callbackState)
        return;
    static_cast<void>(callbackState->stop.request_stop());
    std::lock_guard<std::mutex> lock(callbackState->mutex);
    callbackState->sink = nullptr;
}

} // namespace cloud
