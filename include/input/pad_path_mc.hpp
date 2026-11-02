#ifndef AKIRA_INPUT_PAD_PATH_MC_HPP
#define AKIRA_INPUT_PAD_PATH_MC_HPP

#include "input/pad_path_hos.hpp"
#include "input/ps_report.hpp"

#include <cstdint>

class ExtendedInputManager;

namespace akira::input {

class McGenericPath : public HosPadPath {
public:
    McGenericPath(HidNpadIdType npad, ExtendedInputManager& extended,
                  uint16_t vendor_id, uint16_t product_id);

    PadPathKind kind() const override { return PadPathKind::McGeneric; }
    const char* label() const override { return m_label; }

    PadCapabilities capabilities() const override;

    bool poll() override;
    void readTriggers(libchiaking::controllers::ControllerState& state) override;

    void sendRumble(float left, float right, float freqLow, float freqHigh) override;

    bool     switchNative() const override { return false; }
    uint16_t vendorId() const override { return m_vendor_id; }
    uint16_t productId() const override { return m_product_id; }

protected:
    ExtendedInputManager& m_extended;
    uint16_t              m_vendor_id;
    uint16_t              m_product_id;

    uint32_t              m_rumble_log_next = 0;
    const char*           m_label = "Controller";
};

class McPsNativePath : public McGenericPath {
public:
    McPsNativePath(HidNpadIdType npad, ExtendedInputManager& extended,
                   const PsModel& model, const uint8_t* bt_addr);

    PadPathKind kind() const override { return PadPathKind::McPsNative; }

    PadCapabilities capabilities() const override;

    bool poll() override;

    void readButtons(libchiaking::controllers::ControllerState& state) override;
    void readTriggers(libchiaking::controllers::ControllerState& state) override;
    bool readTouchpad(libchiaking::controllers::ControllerInput& input) override;

    bool usesButtonMapping() const override { return false; }

    void sendRumble(float left, float right, float freqLow, float freqHigh) override;

    void sendTriggerEffects(const TriggerEffect& left, const TriggerEffect& right) override;

    void sendEffectIntensity(uint8_t vibration, uint8_t trigger) override;

    void sendLightbar(uint8_t red, uint8_t green, uint8_t blue) override;

    bool nativeRumble() const override { return m_direct_output; }

    const uint8_t* address() const override { return m_have_address ? m_address : nullptr; }

    bool menuHeld() const override;

    ~McPsNativePath() override;

private:
    bool m_rumble_source_logged = false;
    struct TouchSlot {
        bool    active = false;
        uint8_t pad_id = 0;
        int8_t  touch_id = -1;
    };

    void releaseSlot(libchiaking::controllers::ControllerInput& input, TouchSlot& slot);

    const PsModel* m_model;
    bool           m_home_blocked      = false;
    bool           m_direct_output     = false;
    uint8_t        m_address[6]{};
    bool           m_have_address      = false;
    PsPadState     m_report;
    bool           m_report_valid = false;
    TouchSlot      m_slots[2];
};

} // namespace akira::input

#endif // AKIRA_INPUT_PAD_PATH_MC_HPP
