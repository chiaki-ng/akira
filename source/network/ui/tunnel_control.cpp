#include "network/ui/tunnel_control.hpp"

#include "app/ui_dispatcher.hpp"
#include "network/tunnel.hpp"

namespace akira::network::ui {

TunnelControl::TunnelControl(app::UiDispatcher& dispatcher, Tunnel& tunnel)
    : dispatcher(dispatcher)
    , tunnel(tunnel)
    , state(sampled(false))
{
}

TunnelControl::~TunnelControl()
{
    alive->store(false);
}

auto TunnelControl::sampled(bool connecting) const -> Status
{
    const bool up = tunnel.connected();
    return Status{
        .configured = tunnel.configured(),
        .connecting = connecting,
        .connected = up,
        .address = up ? tunnel.address() : std::string(),
        .error = up ? std::string() : tunnel.lastError(),
    };
}

void TunnelControl::refresh()
{
    state.set(sampled(state.get().connecting));
}

void TunnelControl::connect()
{
    if (state.get().connecting)
        return;
    state.set(sampled(true));
    dispatcher.runOnWorker([this, &ui = dispatcher, &link = tunnel, guard = alive] {
        const bool up = link.connect();
        if (!guard->load())
            return;
        ui.post([this, guard, up, &link] {
            if (!guard->load())
                return;
            if (up)
                link.restartSearch();
            state.set(sampled(false));
        });
    });
}

void TunnelControl::disconnect()
{
    tunnel.disconnect();
    tunnel.restartSearch();
    state.set(sampled(false));
}

}
