#include "platform/pads/hid/hid_pad.hpp"
#include "log/log.hpp"

#include <algorithm>

#include <borealis.hpp>

#include "settings/settings_manager.hpp"

namespace akira::platform::hid
{

HidPad::HidPad()
{
    padInitializeDefault(&m_pad);
    padUpdate(&m_pad);

    hidGetSixAxisSensorHandles(&m_sixaxis[Handheld], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    hidGetSixAxisSensorHandles(&m_sixaxis[FullKey], 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey);
    hidGetSixAxisSensorHandles(&m_sixaxis[JoyDualLeft], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);

    for (std::size_t i = 0; i < SensorCount; i++) {
        const Result rc = hidStartSixAxisSensor(m_sixaxis[i]);
        m_started[i] = R_SUCCEEDED(rc);
        if (!m_started[i])
            log::warning("PadPath: six-axis start failed for sensor {}: 0x{:x}", i, rc);
    }
}

HidPad::~HidPad()
{
    sendRumble(0.0f, 0.0f, 0.0f, 0.0f);

    for (std::size_t i = 0; i < SensorCount; i++) {
        if (m_started[i])
            hidStopSixAxisSensor(m_sixaxis[i]);
    }
}

bool HidPad::poll()
{
    padUpdate(&m_pad);
    m_buttons = padGetButtons(&m_pad);
    return true;
}

HidAnalogStickState HidPad::stickPos(int index) const
{
    return padGetStickPos(&m_pad, index);
}

std::optional<HidSixAxisSensorState> HidPad::readGyro()
{
    const uint64_t styleSet = padGetStyleSet(&m_pad);

    if (styleSet & HidNpadStyleTag_NpadHandheld)
        return readSixAxis(m_sixaxis[Handheld]);

    if (styleSet & HidNpadStyleTag_NpadFullKey)
        return readSixAxis(m_sixaxis[FullKey]);

    if (!(styleSet & HidNpadStyleTag_NpadJoyDual))
        return std::nullopt;

    const uint64_t attrib = padGetAttributes(&m_pad);
    const bool leftConnected  = attrib & HidNpadAttribute_IsLeftConnected;
    const bool rightConnected = attrib & HidNpadAttribute_IsRightConnected;

    switch (settings::SettingsManager::getInstance()->controllers().gyroSource()) {
        case play::GyroSource::Left:
            return leftConnected ? readSixAxis(m_sixaxis[JoyDualLeft]) : std::nullopt;
        case play::GyroSource::Right:
            return rightConnected ? readSixAxis(m_sixaxis[JoyDualRight]) : std::nullopt;
        case play::GyroSource::Auto:
        default:
            if (leftConnected)
                return readSixAxis(m_sixaxis[JoyDualLeft]);
            if (rightConnected)
                return readSixAxis(m_sixaxis[JoyDualRight]);
            return std::nullopt;
    }
}

void HidPad::resetMotion()
{
    for (const HidSixAxisSensorHandle handle : m_sixaxis)
        hidResetSixAxisSensorFusionParameters(handle);
}

void HidPad::sendRumble(float left, float right, float freqLow, float freqHigh)
{
    const float amp = std::clamp(std::max(left, right), 0.0f, 1.0f);
    brls::Application::getPlatform()->getInputManager()->sendRumbleRaw(0, freqLow, freqHigh, amp, amp);
}

}
