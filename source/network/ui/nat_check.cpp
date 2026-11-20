#include "network/ui/nat_check.hpp"

#include "app/strings.hpp"
#include "app/ui_dispatcher.hpp"
#include "network/nat_probe.hpp"

namespace akira::network::ui {

NatCheck::NatCheck(app::UiDispatcher& dispatcher, play::ChiakiCore& core, const app::Strings& strings)
    : dispatcher(dispatcher)
    , core(core)
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
    dispatcher.runOnWorker([this, &ui = dispatcher, &core = core, guard = alive] {
        NatReport report(NatProbe(core).check());
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
