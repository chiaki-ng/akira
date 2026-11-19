#include "cloud/cloud_connect_task.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include "cloud/service.hpp"
#include "consoles/host.hpp"

#include <string>

namespace akira::cloud {

CloudConnectTask::CloudConnectTask(Service& service, const accounts::AccountSettings& accounts,
                                   const app::AppSettings& app, const Game& game)
    : service(service), accounts(accounts), appSettings(app), game(game)
{
}

CloudConnectTask::CloudConnectTask(Service& service, const accounts::AccountSettings& accounts,
                                   const app::AppSettings& app, std::shared_ptr<CloudLaunch> cleared)
    : service(service), accounts(accounts), appSettings(app), clearedLaunch(std::move(cleared))
{
    game.name = clearedLaunch->name();
}

auto CloudConnectTask::retrying(Service& service, const accounts::AccountSettings& accounts,
                                const app::AppSettings& app, const CloudLaunch& previous)
    -> std::unique_ptr<CloudConnectTask>
{
    Game game;
    game.productId = previous.product().text();
    game.name = previous.name();
    game.serviceType = previous.ps5() ? "pscloud" : "psnow";
    game.streamServiceType = game.serviceType;
    return std::make_unique<CloudConnectTask>(service, accounts, app, game);
}

CloudConnectTask::~CloudConnectTask()
{
    cancel();
}

std::string CloudConnectTask::title() const
{
    return brls::getStr("akira/connection/connecting_to", game.name);
}

void CloudConnectTask::start(play::views::ConnectSink& s)
{
    cancel();
    auto state = std::make_shared<CallbackState>();
    state->sink = &s;
    callbackState = state;

    s.progressStep(0, 0, "");

    if (clearedLaunch)
    {
        handOver(state, clearedLaunch, accounts, appSettings);
        return;
    }

    service.clearForLaunch(
        game, state->stop.token(),
        [state, &accounts = accounts, &app = appSettings](std::shared_ptr<CloudLaunch> launch) {
            handOver(state, std::move(launch), accounts, app);
        },
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

void CloudConnectTask::handOver(const std::shared_ptr<CallbackState>& state, std::shared_ptr<CloudLaunch> launch,
                                const accounts::AccountSettings& accounts, const app::AppSettings& app)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->stop.stop_requested() || !state->sink)
        return;

    auto host = std::make_shared<consoles::Host>(consoles::HostRecord{.nickname = launch->name()}, accounts, app);
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

} // namespace akira::cloud
