#include "platform/pads/hid/provider.hpp"

#include "settings/settings_manager.hpp"
#include "platform/pad_settings/hid/provider.hpp"

namespace akira::platform::hid
{

void HidPadProvider::open()
{
    m_pad.emplace();
}

void HidPadProvider::close()
{
    m_pad.reset();
}

input::Pad* HidPadProvider::acquire(Callbacks&)
{
    return current();
}

input::Pad* HidPadProvider::current()
{
    return m_pad ? &*m_pad : nullptr;
}

play::RumbleProfile HidPadProvider::primaryRumbleProfile() const
{
    auto* settings = settings::SettingsManager::getInstance();
    const std::vector<input::PadDescription> pads = scanPads();
    if (pads.empty())
        return settings->getRumbleProfile(play::kRumbleKeyDefault);
    return settings->getRumbleProfile(
        HidPadSettingsProvider::resolveKey(*settings, pads.front().kind == input::PadPathKind::JoyCon));
}

std::vector<input::PadDescription> HidPadProvider::scanPads() const
{
    static constexpr HidNpadIdType scanOrder[] = {
        HidNpadIdType_Handheld,
        HidNpadIdType_No1,
        HidNpadIdType_No2,
        HidNpadIdType_No3,
        HidNpadIdType_No4,
    };

    std::vector<input::PadDescription> out;

    for (HidNpadIdType npad : scanOrder) {
        PadState probe{};
        padInitialize(&probe, npad);
        padUpdate(&probe);

        if (!padIsConnected(&probe))
            continue;

        const uint64_t styleSet = padGetStyleSet(&probe);

        input::PadDescription desc;
        desc.npad = npad;

        if (styleSet & HidNpadStyleTag_NpadHandheld) {
            desc.kind  = input::PadPathKind::JoyCon;
            desc.style = HidNpadStyleTag_NpadHandheld;
            desc.label = "Handheld";
        } else if (styleSet & HidNpadStyleTag_NpadFullKey) {
            desc.kind  = input::PadPathKind::SwitchPro;
            desc.style = HidNpadStyleTag_NpadFullKey;
            desc.label = "Controller";
        } else if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            desc.kind  = input::PadPathKind::JoyCon;
            desc.style = HidNpadStyleTag_NpadJoyDual;
            desc.label = "Joy-Con (L/R)";
        } else {
            continue;
        }

        out.push_back(desc);
    }

    return out;
}

}
