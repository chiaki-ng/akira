#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <arisa/composition/observable.hpp>

#include "network/nat_report.hpp"

namespace akira::app {
class Strings;
class UiDispatcher;
}

namespace akira::network::ui {

class NatCheck {
public:
    struct Status {
        bool checking = false;
        bool finished = false;
        std::optional<int> type;
        std::string detail;
        NatReport::Tone tone = NatReport::Tone::Good;

        friend bool operator==(const Status&, const Status&) = default;
    };

    using Probe = std::function<NatReport()>;

    NatCheck(app::UiDispatcher& dispatcher, Probe probe, const app::Strings& strings);
    ~NatCheck();

    NatCheck(const NatCheck&) = delete;
    NatCheck& operator=(const NatCheck&) = delete;

    auto view() const -> const ::arisa::composition::Observable<Status>& { return state; }

    void start();

private:
    void finish(const NatReport& report);

    app::UiDispatcher& dispatcher;
    Probe probe;
    const app::Strings& strings;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    ::arisa::composition::Observable<Status> state;
};

}
