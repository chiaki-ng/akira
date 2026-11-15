#include "platform/ounce/ounce_pad_source.hpp"

namespace akira::platform
{

void OuncePadSource::open()
{
    m_pad.emplace();
}

void OuncePadSource::close()
{
    m_pad.reset();
}

input::Pad* OuncePadSource::acquire(Callbacks&)
{
    return current();
}

input::Pad* OuncePadSource::current()
{
    return m_pad ? &*m_pad : nullptr;
}

std::vector<input::PadDescription> OuncePadSource::scanPads() const
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
