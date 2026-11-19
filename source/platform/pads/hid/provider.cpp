#include "platform/pads/hid/provider.hpp"

#include "play/controller_settings.hpp"
#include "platform/pad_settings/hid/provider.hpp"

namespace akira::platform::hid
{

HidPadProvider::HidPadProvider(const play::ControllerSettings& controllers)
    : m_controllers(controllers)
{
}

void HidPadProvider::open()
{
    m_pad.emplace(m_controllers);
}

void HidPadProvider::close()
{
    m_pad.reset();
}

platform::Pad* HidPadProvider::acquire(Callbacks&)
{
    return current();
}

platform::Pad* HidPadProvider::current()
{
    return m_pad ? &*m_pad : nullptr;
}

play::RumbleProfile HidPadProvider::primaryRumbleProfile() const
{
    const std::vector<platform::PadDescription> pads = scanPads();
    if (pads.empty())
        return m_controllers.rumbleProfile(play::kRumbleKeyDefault);
    return m_controllers.rumbleProfile(
        HidPadSettingsProvider::resolveKey(m_controllers, pads.front().kind == platform::PadPathKind::JoyCon));
}

std::vector<platform::PadDescription> HidPadProvider::scanPads() const
{
    static constexpr HidNpadIdType scanOrder[] = {
        HidNpadIdType_Handheld,
        HidNpadIdType_No1,
        HidNpadIdType_No2,
        HidNpadIdType_No3,
        HidNpadIdType_No4,
    };

    std::vector<platform::PadDescription> out;

    for (HidNpadIdType npad : scanOrder) {
        PadState probe{};
        padInitialize(&probe, npad);
        padUpdate(&probe);

        if (!padIsConnected(&probe))
            continue;

        const uint64_t styleSet = padGetStyleSet(&probe);

        platform::PadDescription desc;
        desc.npad = npad;

        if (styleSet & HidNpadStyleTag_NpadHandheld) {
            desc.kind  = platform::PadPathKind::JoyCon;
            desc.style = HidNpadStyleTag_NpadHandheld;
            desc.label = "Handheld";
        } else if (styleSet & HidNpadStyleTag_NpadFullKey) {
            desc.kind  = platform::PadPathKind::SwitchPro;
            desc.style = HidNpadStyleTag_NpadFullKey;
            desc.label = "Controller";
        } else if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            desc.kind  = platform::PadPathKind::JoyCon;
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
