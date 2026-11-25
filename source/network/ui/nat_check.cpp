#include "network/ui/nat_check.hpp"

#include "app/strings.hpp"
#include "app/ui_dispatcher.hpp"

#include <utility>

namespace akira::network::ui {

NatCheck::NatCheck(app::UiDispatcher& dispatcher, Probe probe, const app::Strings& strings)
    : dispatcher(dispatcher)
    , probe(std::move(probe))
    , strings(strings)
{
}

NatCheck::~NatCheck()
{
    alive->store(false);
}

void NatCheck::start()
{
    if (state.get().checking)
        return;
    state.set(Status{ .checking = true });
    dispatcher.runOnWorker([this, &ui = dispatcher, run = probe, guard = alive] {
        NatReport report = run();
        if (!guard->load())
            return;
        ui.post([this, guard, report = std::move(report)] {
            if (guard->load())
                finish(report);
        });
    });
}

void NatCheck::finish(const NatReport& report)
{
    state.set(Status{
        .checking = false,
        .finished = true,
        .type = report.type(),
        .detail = strings.text(report.detailKey()),
        .tone = report.tone(),
    });
}

}
