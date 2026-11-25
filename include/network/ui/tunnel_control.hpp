#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <arisa/composition/observable.hpp>

namespace akira::app {
class UiDispatcher;
}

namespace akira::network {
class Tunnel;
}

namespace akira::network::ui {

class TunnelControl {
public:
    struct Status {
        bool configured = false;
        bool connecting = false;
        bool connected = false;
        std::string address;
        std::string error;

        friend bool operator==(const Status&, const Status&) = default;
    };

    TunnelControl(app::UiDispatcher& dispatcher, Tunnel& tunnel);
    ~TunnelControl();

    TunnelControl(const TunnelControl&) = delete;
    TunnelControl& operator=(const TunnelControl&) = delete;

    auto view() const -> const ::arisa::composition::Observable<Status>& { return state; }

    void refresh();
    void connect();
    void disconnect();

private:
    auto sampled(bool connecting) const -> Status;

    app::UiDispatcher& dispatcher;
    Tunnel& tunnel;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    ::arisa::composition::Observable<Status> state;
};

}
