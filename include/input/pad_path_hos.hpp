#ifndef AKIRA_INPUT_PAD_PATH_HOS_HPP
#define AKIRA_INPUT_PAD_PATH_HOS_HPP

#include "input/pad_path.hpp"

namespace akira::input {

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

} // namespace akira::input

#endif // AKIRA_INPUT_PAD_PATH_HOS_HPP
