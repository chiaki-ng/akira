#ifndef AKIRA_INPUT_PAD_PATH_HOS_HPP
#define AKIRA_INPUT_PAD_PATH_HOS_HPP

#if AKIRA_PLATFORM_NX
#include "input/pad_path.hpp"
#else
#include <switch.h>

#include <array>
#include <cstdint>
#include <optional>
#endif

namespace akira::input {

#if AKIRA_PLATFORM_NX
void StartSixAxisSensorShared(HidSixAxisSensorHandle handle);
void StopSixAxisSensorShared(HidSixAxisSensorHandle handle);

class HosPadPath : public PadPath {
public:
public:
    HosPadPath(HidNpadIdType npad, HidNpadStyleTag style);
    ~HosPadPath() override;

    HidNpadIdType   npad()  const override { return m_npad; }
    HidNpadStyleTag style() const override { return m_style; }

    bool poll() override;
    HidAnalogStickState stickPos(int index) const override;
    void readTriggers(libchiaking::controllers::ControllerState& state) override;
    bool readGyro(HidSixAxisSensorState* out) override;
    void resetMotion() override;
    void sendRumble(float left, float right, float freqLow, float freqHigh) override;

    uint64_t heldButtons() const override { return m_buttons; }

protected:
    void acquireHandles();
    void releaseHandles();

    PadState        m_pad{};
    HidNpadIdType   m_npad;
    HidNpadStyleTag m_style;
    uint64_t        m_buttons = 0;

    uint64_t m_style_set_at_acquire = 0;

    HidSixAxisSensorHandle m_sixaxis[2]{};
    int                    m_sixaxis_count = 0;
};
#else
class HosPadPath {
public:
    HosPadPath();
    ~HosPadPath();

    HosPadPath(const HosPadPath&) = delete;
    HosPadPath& operator=(const HosPadPath&) = delete;

    void poll();
    HidAnalogStickState stickPos(int index) const;
    std::optional<HidSixAxisSensorState> readGyro();
    void resetMotion();
    void sendRumble(float left, float right, float freqLow, float freqHigh);

    uint64_t heldButtons() const { return m_buttons; }
    bool menuHeld() const { return (m_buttons & HidNpadButton_Minus) != 0; }

private:
    enum Sensor : std::size_t {
        Handheld,
        FullKey,
        JoyDualLeft,
        JoyDualRight,
        SensorCount,
    };

    std::optional<HidSixAxisSensorState> readSensor(Sensor sensor) const;

    PadState m_pad{};
    uint64_t m_buttons = 0;
    std::array<HidSixAxisSensorHandle, SensorCount> m_sixaxis{};
    std::array<bool, SensorCount> m_started{};
};
#endif

} // namespace akira::input

#endif // AKIRA_INPUT_PAD_PATH_HOS_HPP
