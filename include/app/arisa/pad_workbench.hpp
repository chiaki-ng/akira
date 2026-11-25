#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "app/services.hpp"
#include "platform/pad_settings/provider.hpp"
#include "platform/pads/pad.hpp"
#include "settings/pad_bench.hpp"

namespace akira::platform {
class PadRumble;
}

namespace akira::app::arisa {

class PadWorkbench final : public settings::PadBench {
public:
    static constexpr std::chrono::milliseconds buzzLength{ 300 };

    PadWorkbench(Services& services, platform::PadRumble& rumble);

    auto scan() -> std::vector<Pad> override;
    void use(std::size_t index, const play::RumbleProfile& profile) override;
    void refreshOutputGates(std::size_t index) override;
    void buzz(std::size_t index, const play::RumbleProfile& profile) override;

private:
    auto summary(platform::PadSettingsProvider& tuning) const -> Summary;

    Services& services;
    platform::PadRumble& rumble;
    std::vector<platform::PadDescription> pads;
    std::vector<std::unique_ptr<platform::PadSettingsProvider>> tunings;
};

}
