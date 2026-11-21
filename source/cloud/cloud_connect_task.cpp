#include "cloud/cloud_connect_task.hpp"

#include "cloud/service.hpp"
#include "consoles/host.hpp"

#include <string>

namespace akira::cloud {

CloudConnectTask::CloudConnectTask(Service& service, const accounts::AccountSettings& accounts,
                                   const app::AppSettings& app, app::UiDispatcher& dispatcher,
                                   const app::Strings& strings, akira::play::ChiakiCore& core, const Game& game)
    : service(service), accounts(accounts), appSettings(app), dispatcher(dispatcher), strings(strings), core(core),
      game(game)
{
}

CloudConnectTask::CloudConnectTask(Service& service, const accounts::AccountSettings& accounts,
                                   const app::AppSettings& app, app::UiDispatcher& dispatcher,
                                   const app::Strings& strings, akira::play::ChiakiCore& core,
                                   std::shared_ptr<CloudLaunch> cleared)
    : service(service), accounts(accounts), appSettings(app), dispatcher(dispatcher), strings(strings), core(core),
      clearedLaunch(std::move(cleared))
{
    game.name = clearedLaunch->name();
}

auto CloudConnectTask::retrying(Service& service, const accounts::AccountSettings& accounts,
                                const app::AppSettings& app, app::UiDispatcher& dispatcher,
                                const app::Strings& strings, akira::play::ChiakiCore& core,
                                const CloudLaunch& previous) -> std::unique_ptr<CloudConnectTask>
{
    return std::make_unique<CloudConnectTask>(service, accounts, app, dispatcher, strings, core, previous.game());
}

CloudConnectTask::~CloudConnectTask()
{
    cancel();
}

std::string CloudConnectTask::title() const
{
    return strings.format("akira/connection/connecting_to", game.name);
}

void CloudConnectTask::start(play::ConnectSink& s)
{
    cancel();
    auto state = std::make_shared<CallbackState>();
    state->sink = &s;
    callbackState = state;

    s.progressStep(0, 0, "");

    if (clearedLaunch)
    {
        handOver(state, clearedLaunch, accounts, appSettings, dispatcher, core);
        return;
    }

    service.clearForLaunch(
        game, state->stop.token(),
        [state, &accounts = accounts, &app = appSettings, &dispatcher = dispatcher, &core = core](
            std::shared_ptr<CloudLaunch> launch) {
            handOver(state, std::move(launch), accounts, app, dispatcher, core);
        },
        [state](const std::string& error) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->stop.stop_requested() && state->sink)
                state->sink->failed(error);
        },
        [state, &strings = strings](libchiaking::catalog::LaunchCheck check) {
            constexpr int total = static_cast<int>(libchiaking::catalog::LaunchCheck::release) + 1;
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->stop.stop_requested() && state->sink)
                state->sink->progressStep(static_cast<int>(check) + 1, total,
                                          strings.format("akira/cloud/launch_starting"));
        });
}

void CloudConnectTask::handOver(const std::shared_ptr<CallbackState>& state, std::shared_ptr<CloudLaunch> launch,
                                const accounts::AccountSettings& accounts, const app::AppSettings& app,
                                app::UiDispatcher& dispatcher, akira::play::ChiakiCore& core)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->stop.stop_requested() || !state->sink)
        return;

    auto host = std::make_shared<consoles::Host>(consoles::HostRecord{.nickname = launch->name()}, accounts, app,
                                                  dispatcher, core);
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
