#include "input/pad_path.hpp"

#include <cstring>
#include "input/pad_names.hpp"
#include "input/pad_path_hos.hpp"
#include "input/pad_path_mc.hpp"

#include "core/settings_manager.hpp"
#include "input/extended_input_manager.hpp"

#include <borealis.hpp>

#include <algorithm>

namespace akira::input {

namespace {

class JoyConPath : public HosPadPath {
public:
    JoyConPath(HidNpadIdType npad, HidNpadStyleTag style)
        : HosPadPath(npad, style)
    {
    }

    PadPathKind kind() const override { return PadPathKind::JoyCon; }

    const char* label() const override
    {
        return m_style == HidNpadStyleTag_NpadHandheld ? "Handheld" : "Joy-Con (L/R)";
    }

    PadCapabilities capabilities() const override
    {
        PadCapabilities caps;
        caps.gyro   = true;
        caps.rumble = true;
        return caps;
    }
};

class SwitchProPath : public HosPadPath {
public:
    explicit SwitchProPath(HidNpadIdType npad)
        : HosPadPath(npad, HidNpadStyleTag_NpadFullKey)
    {
    }

    PadPathKind kind() const override { return PadPathKind::SwitchPro; }
    const char* label() const override { return "Pro Controller"; }

    PadCapabilities capabilities() const override
    {
        PadCapabilities caps;
        caps.gyro   = true;
        caps.rumble = true;
        return caps;
    }
};

class LegacyPadPath : public PadPath {
public:
    explicit LegacyPadPath(ExtendedInputManager& extended)
        : m_extended(extended)
    {
        padInitializeDefault(&m_pad);
        padUpdate(&m_pad);

        hidGetSixAxisSensorHandles(&m_sixaxis[0], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
        hidGetSixAxisSensorHandles(&m_sixaxis[1], 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey);
        hidGetSixAxisSensorHandles(&m_sixaxis[2], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
        for (int i = 0; i < 4; i++)
            StartSixAxisSensorShared(m_sixaxis[i]);
    }

    ~LegacyPadPath() override
    {
        for (int i = 0; i < 4; i++)
            StopSixAxisSensorShared(m_sixaxis[i]);
    }

    PadPathKind kind() const override { return PadPathKind::JoyCon; }
    const char* label() const override { return "Default"; }

    HidNpadIdType   npad()  const override { return HidNpadIdType_No1; }
    HidNpadStyleTag style() const override { return (HidNpadStyleTag)padGetStyleSet(&m_pad); }

    PadCapabilities capabilities() const override
    {
        PadCapabilities caps;
        caps.gyro   = true;
        caps.rumble = true;
        return caps;
    }

    bool poll() override
    {
        padUpdate(&m_pad);
        m_buttons = padGetButtons(&m_pad);
        return true;
    }

    HidAnalogStickState stickPos(int index) const override
    {
        return padGetStickPos(&m_pad, index);
    }

    void readTriggers(libchiaking::controllers::ControllerState& state) override
    {
        if (!m_extended.hasFreshAnalogState())
            return;

        state.l2 = m_extended.l2();
        state.r2 = m_extended.r2();
    }

    bool readGyro(HidSixAxisSensorState* out) override
    {
        uint64_t styleSet = padGetStyleSet(&m_pad);

        if (styleSet & HidNpadStyleTag_NpadHandheld) {
            return hidGetSixAxisSensorStates(m_sixaxis[0], out, 1) != 0;
        }

        if (styleSet & HidNpadStyleTag_NpadFullKey) {
            return hidGetSixAxisSensorStates(m_sixaxis[1], out, 1) != 0;
        }

        if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            uint64_t attrib = padGetAttributes(&m_pad);
            bool leftConnected  = attrib & HidNpadAttribute_IsLeftConnected;
            bool rightConnected = attrib & HidNpadAttribute_IsRightConnected;

            bool useLeft  = false;
            bool useRight = false;

            switch (SettingsManager::getInstance()->getGyroSource()) {
                case GyroSource::Left:  useLeft  = leftConnected;  break;
                case GyroSource::Right: useRight = rightConnected; break;
                case GyroSource::Auto:
                default:
                    if (leftConnected)       useLeft  = true;
                    else if (rightConnected) useRight = true;
                    break;
            }

            if (useLeft)
                return hidGetSixAxisSensorStates(m_sixaxis[2], out, 1) != 0;
            if (useRight)
                return hidGetSixAxisSensorStates(m_sixaxis[3], out, 1) != 0;
            return false;
        }

        return false;
    }

    void resetMotion() override
    {
        for (int i = 0; i < 4; i++)
            hidResetSixAxisSensorFusionParameters(m_sixaxis[i]);
    }

    void sendRumble(float left, float right, float freqLow, float freqHigh) override
    {
        float amp = std::clamp(std::max(left, right), 0.0f, 1.0f);

        auto* inputMgr = brls::Application::getPlatform()->getInputManager();
        inputMgr->sendRumbleRaw(0, freqLow, freqHigh, amp, amp);
    }

    uint64_t heldButtons() const override { return m_buttons; }

    void setExcludedNpads(uint64_t mask) override
    {
        if (mask == m_excluded)
            return;
        m_excluded = mask;

        constexpr uint64_t base = (1UL << HidNpadIdType_No1) | (1UL << HidNpadIdType_Handheld);
        uint64_t use = base & ~mask;
        if (!use)
            use = 1UL << HidNpadIdType_Handheld;

        padInitializeWithMask(&m_pad, use);
        padUpdate(&m_pad);
        m_buttons = padGetButtons(&m_pad);
    }

private:
    ExtendedInputManager&  m_extended;
    PadState               m_pad{};
    uint64_t               m_buttons = 0;
    uint64_t               m_excluded = 0;
    HidSixAxisSensorHandle m_sixaxis[4]{};
};

constexpr HidNpadIdType kScanOrder[] = {
    HidNpadIdType_Handheld,
    HidNpadIdType_No1,
    HidNpadIdType_No2,
    HidNpadIdType_No3,
    HidNpadIdType_No4,
};

} // namespace

namespace {

std::vector<PadDescription> DescribeWith(const AkiraInputDeviceList& devices, bool haveDevices);

} // namespace

std::vector<PadDescription> DescribePads(ExtendedInputManager& extended)
{
    AkiraInputDeviceList devices{};
    const bool haveDevices = extended.listDevices(&devices);

    return DescribeWith(devices, haveDevices);
}

std::vector<PadDescription> DescribePads()
{
    AkiraInputDeviceList devices{};
    const bool haveDevices = ExtendedInputManager::listDevicesOnce(&devices);

    return DescribeWith(devices, haveDevices);
}

namespace {

bool HidsysReady()
{
    static bool attempted = false;
    static bool ready     = false;

    if (!attempted) {
        attempted = true;
        const Result rc = hidsysInitialize();
        ready = R_SUCCEEDED(rc);
        if (!ready) {
            brls::Logger::warning("pad path: hidsysInitialize failed (0x{:x}),"
                                  " falling back to positional pad matching", rc);
        }
    }

    return ready;
}

bool NpadBluetoothAddress(HidNpadIdType npad, uint8_t* out)
{
    if (!HidsysReady())
        return false;

    HidsysUniquePadId ids[2]{};
    s32 total = 0;
    if (R_FAILED(hidsysGetUniquePadsFromNpad(npad, ids, 2, &total)) || total <= 0)
        return false;

    for (s32 i = 0; i < total && i < 2; i++) {
        BtdrvAddress addr{};
        if (R_FAILED(hidsysGetUniquePadBluetoothAddress(ids[i], &addr)))
            continue;

        bool any = false;
        for (size_t b = 0; b < sizeof(addr.address); b++) {
            if (addr.address[b] != 0) {
                any = true;
                break;
            }
        }
        if (!any)
            continue;

        std::memcpy(out, addr.address, 6);
        return true;
    }

    return false;
}

std::vector<PadDescription> DescribeWith(const AkiraInputDeviceList& devices, bool haveDevices)
{
    std::vector<PadDescription> out;

    bool taken[AKIRA_INPUT_MAX_LISTED_DEVICES]{};

    for (HidNpadIdType npad : kScanOrder) {
        PadState probe{};
        padInitialize(&probe, npad);
        padUpdate(&probe);

        if (!padIsConnected(&probe))
            continue;

        const uint64_t styleSet = padGetStyleSet(&probe);

        const AkiraInputDeviceInfo* mc = nullptr;
        if (haveDevices && (styleSet & HidNpadStyleTag_NpadFullKey) != 0) {
            uint8_t npad_addr[6]{};
            const bool haveNpadAddr = NpadBluetoothAddress(npad, npad_addr);

            if (haveNpadAddr) {
                for (uint8_t i = 0; i < devices.count && i < AKIRA_INPUT_MAX_LISTED_DEVICES; i++) {
                    const AkiraInputDeviceInfo& d = devices.devices[i];
                    if ((d.flags & AkiraInputDevice_Identified) == 0)
                        continue;
                    if (std::memcmp(d.bt_addr, npad_addr, sizeof(npad_addr)) != 0)
                        continue;
                    mc       = &d;
                    taken[i] = true;
                    break;
                }
            }

            if (mc == nullptr) {
                for (uint8_t i = 0; i < devices.count && i < AKIRA_INPUT_MAX_LISTED_DEVICES; i++) {
                    if (taken[i]) {
                        continue;
                    }
                    const AkiraInputDeviceInfo& d = devices.devices[i];
                    if ((d.flags & AkiraInputDevice_Identified) == 0) {
                        continue;
                    }
                    mc       = &d;
                    taken[i] = true;
                    break;
                }
            }
        }

        PadDescription desc;
        desc.npad = npad;

        if (mc != nullptr) {
            const PsModel* model = FindPsModel(mc->vendor_id, mc->product_id);

            desc.style       = HidNpadStyleTag_NpadFullKey;
            desc.vendor_id   = mc->vendor_id;
            desc.product_id  = mc->product_id;
            std::memcpy(desc.bt_addr, mc->bt_addr, sizeof(desc.bt_addr));
            desc.has_address = true;
            desc.caps.gyro   = true;
            desc.caps.rumble = true;
            desc.caps.analog_triggers = false;
            desc.caps.battery = PadReportsBattery(mc->vendor_id, mc->product_id);

            if (model != nullptr) {
                desc.kind          = PadPathKind::McPsNative;
                desc.label         = model->name;
                desc.caps.touchpad = model->has_touchpad;

                desc.caps.analog_triggers = true;
            } else {
                desc.kind = PadPathKind::McGeneric;

                const char* named = FindPadName(mc->vendor_id, mc->product_id);
                desc.label = named != nullptr ? named : "Controller";
            }

            out.push_back(desc);
            continue;
        }

        desc.caps.gyro   = true;
        desc.caps.rumble = true;

        desc.caps.battery = (styleSet & HidNpadStyleTag_NpadFullKey) == 0;

        if (styleSet & HidNpadStyleTag_NpadHandheld) {
            desc.kind  = PadPathKind::JoyCon;
            desc.style = HidNpadStyleTag_NpadHandheld;
            desc.label = "Handheld";
        } else if (styleSet & HidNpadStyleTag_NpadFullKey) {
            desc.kind  = PadPathKind::SwitchPro;
            desc.style = HidNpadStyleTag_NpadFullKey;
            const bool backendEnumerated = haveDevices && devices.count > 0;
            desc.label = backendEnumerated ? "Pro Controller" : "Controller";
        } else if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            desc.kind  = PadPathKind::JoyCon;
            desc.style = HidNpadStyleTag_NpadJoyDual;
            desc.label = "Joy-Con (L/R)";
        } else {
            continue;
        }

        out.push_back(desc);
    }

    return out;
}

} // namespace

PadDriverState ResolvePadDriverState(const PadDescription& desc,
                                     ExtendedInputManager& extended)
{
    PadOutputInputs in;
    in.supported_pad = desc.kind == PadPathKind::McPsNative
                    && FindPsModel(desc.vendor_id, desc.product_id) != nullptr;

    if (in.supported_pad) {
        const RumbleProfile profile =
            SettingsManager::getInstance()->resolveRumbleProfile(
                desc.vendor_id, desc.product_id,
                desc.has_address ? desc.bt_addr : nullptr, false, false);
        in.profile_native = profile.output_mode == PadOutputMode::Native;
    }

    in.backend_enabled   = !extended.backendReleased();
    in.ownership_refused = ExtendedInputManager::outputOwnershipRefused();

    in.wanted        = true;
    in.address_valid = true;
    in.owns_output   = !in.ownership_refused;

    const PadOutputState state = ResolvePadOutput(in);
    return { state.driver, state.reason };
}

PadDriver ResolvePadDriver(const PadDescription& desc, ExtendedInputManager& extended)
{
    return ResolvePadDriverState(desc, extended).driver;
}

std::unique_ptr<PadPath> MakePath(const PadDescription& desc, ExtendedInputManager& extended)
{
    switch (desc.kind) {
        case PadPathKind::McPsNative: {
            const PsModel* model = FindPsModel(desc.vendor_id, desc.product_id);
            if (model == nullptr)
                return nullptr;

            if (ResolvePadDriver(desc, extended) == PadDriver::MissionControl) {
                brls::Logger::info("pad {:04x}:{:04x} driven by MissionControl"
                                   " - using the generic path",
                                   desc.vendor_id, desc.product_id);
                return std::make_unique<McGenericPath>(desc.npad, extended, desc.vendor_id,
                                                       desc.product_id);
            }

            return std::make_unique<McPsNativePath>(desc.npad, extended, *model,
                                                   desc.has_address ? desc.bt_addr : nullptr);
        }
        case PadPathKind::McGeneric:
            return std::make_unique<McGenericPath>(desc.npad, extended, desc.vendor_id,
                                                   desc.product_id);
        case PadPathKind::SwitchPro:
            return std::make_unique<SwitchProPath>(desc.npad);
        case PadPathKind::JoyCon:
        default:
            return std::make_unique<JoyConPath>(desc.npad, desc.style);
    }
}

std::unique_ptr<PadPath> DefaultPadPath(ExtendedInputManager& extended)
{
    return std::make_unique<LegacyPadPath>(extended);
}

} // namespace akira::input
