#include "platform/ounce/hos_pad.hpp"

#include <algorithm>

#include <borealis.hpp>

#include "core/settings_manager.hpp"

namespace akira::platform
{

HosPad::HosPad()
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
            brls::Logger::warning("PadPath: six-axis start failed for sensor {}: 0x{:x}", i, rc);
    }
}

HosPad::~HosPad()
{
    sendRumble(0.0f, 0.0f, 0.0f, 0.0f);

    for (std::size_t i = 0; i < SensorCount; i++) {
        if (m_started[i])
            hidStopSixAxisSensor(m_sixaxis[i]);
    }
}

bool HosPad::poll()
{
    padUpdate(&m_pad);
    m_buttons = padGetButtons(&m_pad);
    return true;
}

HidAnalogStickState HosPad::stickPos(int index) const
{
    return padGetStickPos(&m_pad, index);
}

std::optional<HidSixAxisSensorState> HosPad::readGyro()
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

    switch (SettingsManager::getInstance()->getGyroSource()) {
        case GyroSource::Left:
            return leftConnected ? readSixAxis(m_sixaxis[JoyDualLeft]) : std::nullopt;
        case GyroSource::Right:
            return rightConnected ? readSixAxis(m_sixaxis[JoyDualRight]) : std::nullopt;
        case GyroSource::Auto:
        default:
            if (leftConnected)
                return readSixAxis(m_sixaxis[JoyDualLeft]);
            if (rightConnected)
                return readSixAxis(m_sixaxis[JoyDualRight]);
            return std::nullopt;
    }
}

void HosPad::resetMotion()
{
    for (const HidSixAxisSensorHandle handle : m_sixaxis)
        hidResetSixAxisSensorFusionParameters(handle);
}

void HosPad::sendRumble(float left, float right, float freqLow, float freqHigh)
{
    const float amp = std::clamp(std::max(left, right), 0.0f, 1.0f);
    brls::Application::getPlatform()->getInputManager()->sendRumbleRaw(0, freqLow, freqHigh, amp, amp);
}

}
