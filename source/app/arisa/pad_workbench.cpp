#include "app/arisa/pad_workbench.hpp"

#include "app/ui_dispatcher.hpp"
#include "platform/factory.hpp"
#include "platform/pads/provider.hpp"
#include "platform/pads/rumble.hpp"
#include "play/controller_settings.hpp"

namespace akira::app::arisa {

PadWorkbench::PadWorkbench(Services& services, platform::PadRumble& rumble)
    : services(services)
    , rumble(rumble)
{
}

auto PadWorkbench::summary(platform::PadSettingsProvider& tuning) const -> Summary
{
    if (const auto mode = tuning.outputMode(services.controllers))
        return *mode == play::PadOutputMode::Basic ? Summary::Basic : Summary::Native;
    const std::string key = tuning.currentKey(services.controllers);
    if (key == play::kRumbleKeySwitch)
        return Summary::SwitchProfile;
    if (key == play::kRumbleKeyDefault)
        return Summary::DefaultProfile;
    if (tuning.isUnitKey(key))
        return Summary::ThisController;
    return Summary::Other;
}

auto PadWorkbench::scan() -> std::vector<Pad>
{
    pads = services.platform.pads().scanPads();
    tunings.clear();
    std::vector<Pad> found;
    for (const platform::PadDescription& pad : pads)
    {
        std::unique_ptr<platform::PadSettingsProvider> tuning = services.platform.createPadSettings(pad);
        const std::string key = tuning->rumbleKey(services.controllers);
        services.controllers.seedRumbleProfile(key);
        found.push_back(Pad{
            .label = pad.label,
            .key = key,
            .summary = summary(*tuning),
            .direct = tuning->directOutput(),
            .tunesMotors = tuning->tunesMotors(),
            .outputReleased = tuning->outputReleased(),
            .drivesLightbar = tuning->drivesLightbar(),
            .joyCon = pad.kind == platform::PadPathKind::JoyCon,
        });
        tunings.push_back(std::move(tuning));
    }
    return found;
}

void PadWorkbench::use(std::size_t index, const play::RumbleProfile& profile)
{
    if (index < tunings.size())
        tunings[index]->useProfile(profile);
}

void PadWorkbench::refreshOutputGates(std::size_t index)
{
    if (index < tunings.size())
        tunings[index]->refreshOutputGates();
}

void PadWorkbench::buzz(std::size_t index, const play::RumbleProfile& profile)
{
    if (index >= tunings.size())
        return;
    if (auto preview = tunings[index]->previewDirect(profile))
    {
        if (preview->stop)
            services.dispatcher.postAfter(buzzLength, preview->stop);
        return;
    }
    const auto npad = static_cast<unsigned>(pads[index].npad);
    const float amplitude = profile.strength * profile.ceiling;
    rumble.npad(npad, platform::PadRumble::Wave{ profile.freq_low, profile.freq_high, amplitude, amplitude });
    services.dispatcher.postAfter(buzzLength, [this, npad] { rumble.npad(npad, platform::PadRumble::Wave{ 0.0F, 0.0F, 0.0F, 0.0F }); });
}

}
