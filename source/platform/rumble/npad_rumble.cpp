#include "platform/rumble/npad_rumble.hpp"

#include "log/log.hpp"

namespace akira::platform {

NpadRumble::NpadRumble()
{
    prepare(handheld, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
}

void NpadRumble::prepare(Device& device, HidNpadIdType id, u32 styles)
{
    HidNpadStyleTag style = HidNpadStyleTag_NpadFullKey;
    s32 count = 2;
    if (styles & HidNpadStyleTag_NpadHandheld)
        style = HidNpadStyleTag_NpadHandheld;
    else if (styles & HidNpadStyleTag_NpadFullKey)
        style = HidNpadStyleTag_NpadFullKey;
    else if (styles & HidNpadStyleTag_NpadJoyDual)
        style = HidNpadStyleTag_NpadJoyDual;
    else if (styles & HidNpadStyleTag_NpadJoyLeft)
    {
        style = HidNpadStyleTag_NpadJoyLeft;
        count = 1;
    }
    else if (styles & HidNpadStyleTag_NpadJoyRight)
    {
        style = HidNpadStyleTag_NpadJoyRight;
        count = 1;
    }
    const Result result = hidInitializeVibrationDevices(device.handles.data(), count, id, style);
    if (R_FAILED(result))
    {
        log::warning("Vibration init failed for npad {} (style {:#x}): {:#x}", static_cast<int>(id),
            static_cast<u32>(style), result);
        device.count = 0;
        return;
    }
    device.count = count;
    device.style = styles;
}

void NpadRumble::send(Device& device, Wave wave)
{
    if (device.count <= 0)
        return;
    std::array<HidVibrationValue, 2> values{};
    for (HidVibrationValue& value : values)
    {
        value.amp_low = wave.ampLow;
        value.freq_low = wave.freqLow;
        value.amp_high = wave.ampHigh;
        value.freq_high = wave.freqHigh;
    }
    hidSendVibrationValues(device.handles.data(), values.data(), device.count);
}

auto NpadRumble::deviceFor(unsigned npad) -> Device&
{
    Device& device = devices[npad];
    const u32 styles = hidGetNpadStyleSet(static_cast<HidNpadIdType>(npad));
    if (styles != 0 && styles != device.style)
        prepare(device, static_cast<HidNpadIdType>(npad), styles);
    return device;
}

void NpadRumble::playerOne(Wave wave)
{
    std::lock_guard lock(mutex);
    if (hidGetNpadStyleSet(HidNpadIdType_Handheld) != 0)
    {
        send(handheld, wave);
        return;
    }
    send(deviceFor(HidNpadIdType_No1), wave);
}

void NpadRumble::npad(unsigned npad, Wave wave)
{
    std::lock_guard lock(mutex);
    if (npad == HidNpadIdType_Handheld)
    {
        send(handheld, wave);
        return;
    }
    if (npad >= players)
        return;
    send(deviceFor(npad), wave);
}

void NpadRumble::refresh(unsigned npad)
{
    std::lock_guard lock(mutex);
    if (npad >= players)
        return;
    devices[npad].style = 0;
    static_cast<void>(deviceFor(npad));
}

}
