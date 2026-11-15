#include "input/pad_path_hos.hpp"

#include "core/settings_manager.hpp"

#include <borealis.hpp>

#include <algorithm>
#if AKIRA_PLATFORM_NX
#include <cstring>
#include <map>
#endif

namespace akira::input {

#if AKIRA_PLATFORM_NX
namespace {

int SixAxisSensorCount(HidNpadStyleTag style)
{
    return style == HidNpadStyleTag_NpadJoyDual ? 2 : 1;
}

uint32_t SensorKey(HidSixAxisSensorHandle handle)
{
    uint32_t key = 0;
    std::memcpy(&key, &handle, sizeof(key));
    return key;
}

std::map<uint32_t, int>& SensorRefs()
{
    static std::map<uint32_t, int> refs;
    return refs;
}

} // namespace

void StartSixAxisSensorShared(HidSixAxisSensorHandle handle)
{
    if (SensorRefs()[SensorKey(handle)]++ > 0)
        return;

    const Result rc = hidStartSixAxisSensor(handle);
    if (R_FAILED(rc)) {
        SensorRefs().erase(SensorKey(handle));
        brls::Logger::warning("PadPath: six-axis start failed for handle 0x{:x}: 0x{:x}",
            SensorKey(handle), rc);
    }
}

void StopSixAxisSensorShared(HidSixAxisSensorHandle handle)
{
    auto it = SensorRefs().find(SensorKey(handle));
    if (it == SensorRefs().end())
        return;

    if (--it->second > 0)
        return;

    hidStopSixAxisSensor(handle);
    SensorRefs().erase(it);
}

HosPadPath::HosPadPath(HidNpadIdType npad, HidNpadStyleTag style)
    : m_npad(npad)
    , m_style(style)
{
    padInitialize(&m_pad, npad);
    padUpdate(&m_pad);
    acquireHandles();
}

HosPadPath::~HosPadPath()
{
    releaseHandles();
}

void HosPadPath::acquireHandles()
{
    m_style_set_at_acquire = padGetStyleSet(&m_pad);
    m_sixaxis_count = SixAxisSensorCount(m_style);
    Result rc = hidGetSixAxisSensorHandles(&m_sixaxis[0], m_sixaxis_count, m_npad, m_style);
    if (R_FAILED(rc)) {
        brls::Logger::warning("PadPath: six-axis handles failed for npad {} style 0x{:x}: 0x{:x}",
            (int)m_npad, (uint32_t)m_style, rc);
        m_sixaxis_count = 0;
    } else {
        for (int i = 0; i < m_sixaxis_count; i++)
            StartSixAxisSensorShared(m_sixaxis[i]);
    }
}

void HosPadPath::releaseHandles()
{
    for (int i = 0; i < m_sixaxis_count; i++)
        StopSixAxisSensorShared(m_sixaxis[i]);
    m_sixaxis_count = 0;

    sendRumble(0.0f, 0.0f, 0.0f, 0.0f);
}

bool HosPadPath::poll()
{
    padUpdate(&m_pad);
    m_buttons = padGetButtons(&m_pad);

    uint64_t styleSet = padGetStyleSet(&m_pad);
    if (styleSet != m_style_set_at_acquire) {
        brls::Logger::info("PadPath: npad {} style set changed 0x{:x} -> 0x{:x}, rebuilding handles",
            (int)m_npad, m_style_set_at_acquire, styleSet);

        if (styleSet & m_style) {
        } else if (styleSet & HidNpadStyleTag_NpadFullKey) {
            m_style = HidNpadStyleTag_NpadFullKey;
        } else if (styleSet & HidNpadStyleTag_NpadHandheld) {
            m_style = HidNpadStyleTag_NpadHandheld;
        } else if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            m_style = HidNpadStyleTag_NpadJoyDual;
        } else if (styleSet & HidNpadStyleTag_NpadJoyLeft) {
            m_style = HidNpadStyleTag_NpadJoyLeft;
        } else if (styleSet & HidNpadStyleTag_NpadJoyRight) {
            m_style = HidNpadStyleTag_NpadJoyRight;
        }

        releaseHandles();
        acquireHandles();
    }

    return padIsConnected(&m_pad);
}
#else
HosPadPath::HosPadPath()
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

HosPadPath::~HosPadPath()
{
    sendRumble(0.0f, 0.0f, 0.0f, 0.0f);

    for (std::size_t i = 0; i < SensorCount; i++) {
        if (m_started[i])
            hidStopSixAxisSensor(m_sixaxis[i]);
    }
}

void HosPadPath::poll()
{
    padUpdate(&m_pad);
    m_buttons = padGetButtons(&m_pad);
}
#endif

HidAnalogStickState HosPadPath::stickPos(int index) const
{
    return padGetStickPos(&m_pad, index);
}

#if AKIRA_PLATFORM_NX
void HosPadPath::readTriggers(libchiaking::controllers::ControllerState& state)
{
    (void)state;
}

bool HosPadPath::readGyro(HidSixAxisSensorState* out)
{
    if (m_sixaxis_count <= 0)
        return false;

    int index = 0;

    if (m_style == HidNpadStyleTag_NpadJoyDual && m_sixaxis_count == 2) {
        uint64_t attrib = padGetAttributes(&m_pad);
        bool leftConnected  = attrib & HidNpadAttribute_IsLeftConnected;
        bool rightConnected = attrib & HidNpadAttribute_IsRightConnected;

        switch (SettingsManager::getInstance()->getGyroSource()) {
            case GyroSource::Left:
                if (!leftConnected) return false;
                index = 0;
                break;
            case GyroSource::Right:
                if (!rightConnected) return false;
                index = 1;
                break;
            case GyroSource::Auto:
            default:
                if (leftConnected)       index = 0;
                else if (rightConnected) index = 1;
                else                     return false;
                break;
        }
    }

    return hidGetSixAxisSensorStates(m_sixaxis[index], out, 1) != 0;
}

void HosPadPath::resetMotion()
{
    for (int i = 0; i < m_sixaxis_count; i++)
        hidResetSixAxisSensorFusionParameters(m_sixaxis[i]);
}
#else
std::optional<HidSixAxisSensorState> HosPadPath::readSensor(Sensor sensor) const
{
    HidSixAxisSensorState state{};
    if (hidGetSixAxisSensorStates(m_sixaxis[sensor], &state, 1) == 0)
        return std::nullopt;
    return state;
}

std::optional<HidSixAxisSensorState> HosPadPath::readGyro()
{
    const uint64_t styleSet = padGetStyleSet(&m_pad);

    if (styleSet & HidNpadStyleTag_NpadHandheld)
        return readSensor(Handheld);

    if (styleSet & HidNpadStyleTag_NpadFullKey)
        return readSensor(FullKey);

    if (!(styleSet & HidNpadStyleTag_NpadJoyDual))
        return std::nullopt;

    const uint64_t attrib = padGetAttributes(&m_pad);
    const bool leftConnected  = attrib & HidNpadAttribute_IsLeftConnected;
    const bool rightConnected = attrib & HidNpadAttribute_IsRightConnected;

    switch (SettingsManager::getInstance()->getGyroSource()) {
        case GyroSource::Left:
            return leftConnected ? readSensor(JoyDualLeft) : std::nullopt;
        case GyroSource::Right:
            return rightConnected ? readSensor(JoyDualRight) : std::nullopt;
        case GyroSource::Auto:
        default:
            if (leftConnected)
                return readSensor(JoyDualLeft);
            if (rightConnected)
                return readSensor(JoyDualRight);
            return std::nullopt;
    }
}

void HosPadPath::resetMotion()
{
    for (const HidSixAxisSensorHandle handle : m_sixaxis)
        hidResetSixAxisSensorFusionParameters(handle);
}
#endif

void HosPadPath::sendRumble(float left, float right, float freqLow, float freqHigh)
{
    const float amp = std::clamp(std::max(left, right), 0.0f, 1.0f);

    auto* inputMgr = brls::Application::getPlatform()->getInputManager();
#if AKIRA_PLATFORM_NX
    inputMgr->sendRumbleToNpad((unsigned int)m_npad, freqLow, freqHigh, amp, amp);
#else
    inputMgr->sendRumbleRaw(0, freqLow, freqHigh, amp, amp);
#endif
}

} // namespace akira::input
