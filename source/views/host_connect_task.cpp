#include "views/host_connect_task.hpp"

#include <memory>

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

#include "core/host.hpp"
#include "core/chiaki_core.hpp"

using namespace brls::literals;

namespace akira::views {

HostConnectTask::HostConnectTask(Host* host)
    : host(host)
{
}

HostConnectTask::~HostConnectTask()
{
    /* The view hands the task back before it tears itself down, so joining here
     * is what guarantees the worker is not still holding a sink that is about
     * to stop existing. */
    worker = libchiaking::platform::Thread{};
    alive.reset();
}

std::string HostConnectTask::title() const
{
    return brls::getStr("akira/connection/connecting_to", host->getHostName());
}

const char* HostConnectTask::logKind() const
{
    return host->isRemote() ? "remote" : "local";
}

void HostConnectTask::cancel()
{
    worker.request_stop();
    running.store(false);
}

void HostConnectTask::start(ConnectSink& s)
{
    sink = &s;
    running.store(true);

    auto started = libchiaking::platform::Thread::start(akira::ChiakiCore::current().threading(), "host-connect",
        libchiaking::platform::ThreadRole::unspecified,
        [this](libchiaking::platform::StopToken stop) { run(stop); });
    if (!started) {
        running.store(false);
        sink->failed("akira/connection/failed_create_thread"_i18n);
        return;
    }
    worker = std::move(started).value();
}

void HostConnectTask::run(const libchiaking::platform::StopToken& stop)
{
    brls::Logger::info("Connection thread started");

    if (!host->isRemote()) {
        brls::Logger::info("Local connection - ready to stream");
        running.store(false);
        post([target = host](ConnectSink& to) { to.succeeded(target, {}); });
        return;
    }

    sink->progress(ConnectionStage::Finding);
    auto* accounts = akira::ChiakiCore::current().accounts();
    if (!accounts || !accounts->staySignedIn(stop)) {
        running.store(false);
        if (!stop.stop_requested())
            post([](ConnectSink& to) { to.failed("akira/connection/psn_token_expired"_i18n); });
        return;
    }

    brls::Logger::info("PSN sign-in is current, handing over to the stream");
    running.store(false);
    if (!stop.stop_requested() && !sink->cancelled())
        post([target = host](ConnectSink& to) { to.succeeded(target, {}); });
}

void HostConnectTask::post(std::function<void(ConnectSink&)> work)
{
    brls::sync([token = aliveToken, to = sink, work = std::move(work)]() {
        if (!token.expired())
            work(*to);
    });
}

} // namespace akira::views
