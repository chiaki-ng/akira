#include "platform/pads/extended/pad_path.hpp"
#include "platform/pad_settings/extended/provider.hpp"
#include "log/log.hpp"

#include <cstring>
#include "platform/pads/extended/pad_names.hpp"
#include "platform/pads/extended/pad_path_hos.hpp"
#include "platform/pads/extended/pad_path_mc.hpp"

#include "play/controller_settings.hpp"
#include "platform/pads/extended/extended_input_manager.hpp"

#include <algorithm>

namespace akira::platform::extended {

namespace {

class JoyConPath : public HosPadPath {
public:
    JoyConPath(HidNpadIdType npad, HidNpadStyleTag style, const play::ControllerSettings& controllers,
               platform::PadRumble& rumble)
        : HosPadPath(npad, style, controllers, rumble)
    {
    }

    platform::PadPathKind kind() const override { return platform::PadPathKind::JoyCon; }

    const char* label() const override
    {
        return m_style == HidNpadStyleTag_NpadHandheld ? "Handheld" : "Joy-Con (L/R)";
    }

    platform::PadCapabilities capabilities() const override
    {
        platform::PadCapabilities caps;
        caps.gyro   = true;
        caps.rumble = true;
        return caps;
    }
};

class SwitchProPath : public HosPadPath {
public:
    SwitchProPath(HidNpadIdType npad, const play::ControllerSettings& controllers, platform::PadRumble& rumble)
        : HosPadPath(npad, HidNpadStyleTag_NpadFullKey, controllers, rumble)
    {
    }

    platform::PadPathKind kind() const override { return platform::PadPathKind::SwitchPro; }
    const char* label() const override { return "Pro Controller"; }

    platform::PadCapabilities capabilities() const override
    {
        platform::PadCapabilities caps;
        caps.gyro   = true;
        caps.rumble = true;
        return caps;
    }
};

class LegacyPadPath : public PadPath {
public:
    LegacyPadPath(ExtendedInputManager& extended, const play::ControllerSettings& controllers)
        : m_extended(extended)
        , m_controllers(controllers)
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

    platform::PadPathKind kind() const override { return platform::PadPathKind::JoyCon; }
    const char* label() const override { return "Default"; }

    HidNpadIdType   npad()  const override { return HidNpadIdType_No1; }
    HidNpadStyleTag style() const override { return (HidNpadStyleTag)padGetStyleSet(&m_pad); }

    platform::PadCapabilities capabilities() const override
    {
        platform::PadCapabilities caps;
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

    std::optional<HidSixAxisSensorState> readGyro() override
    {
        uint64_t styleSet = padGetStyleSet(&m_pad);

        if (styleSet & HidNpadStyleTag_NpadHandheld) {
            return readSixAxis(m_sixaxis[0]);
        }

        if (styleSet & HidNpadStyleTag_NpadFullKey) {
            return readSixAxis(m_sixaxis[1]);
        }

        if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            uint64_t attrib = padGetAttributes(&m_pad);
            bool leftConnected  = attrib & HidNpadAttribute_IsLeftConnected;
            bool rightConnected = attrib & HidNpadAttribute_IsRightConnected;

            bool useLeft  = false;
            bool useRight = false;

            switch (m_controllers.gyroSource()) {
                case play::GyroSource::Left:  useLeft  = leftConnected;  break;
                case play::GyroSource::Right: useRight = rightConnected; break;
                case play::GyroSource::Auto:
                default:
                    if (leftConnected)       useLeft  = true;
                    else if (rightConnected) useRight = true;
                    break;
            }

            if (useLeft)
                return readSixAxis(m_sixaxis[2]);
            if (useRight)
                return readSixAxis(m_sixaxis[3]);
            return std::nullopt;
        }

        return std::nullopt;
    }

    void resetMotion() override
    {
        for (int i = 0; i < 4; i++)
            hidResetSixAxisSensorFusionParameters(m_sixaxis[i]);
    }

    void sendRumble(float left, float right, float freqLow, float freqHigh) override
    {
        float amp = std::clamp(std::max(left, right), 0.0f, 1.0f);

        m_extended.rumble().playerOne({ .freqLow = freqLow, .freqHigh = freqHigh, .ampLow = amp, .ampHigh = amp });
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
    const play::ControllerSettings& m_controllers;
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

std::vector<platform::PadDescription> DescribeWith(const AkiraInputDeviceList& devices, bool haveDevices);

} // namespace

std::vector<platform::PadDescription> DescribePads(ExtendedInputManager& extended)
{
    AkiraInputDeviceList devices{};
    const bool haveDevices = extended.listDevices(&devices);

    return DescribeWith(devices, haveDevices);
}

std::vector<platform::PadDescription> DescribePads()
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
            log::warning("pad path: hidsysInitialize failed (0x{:x}),"
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

std::vector<platform::PadDescription> DescribeWith(const AkiraInputDeviceList& devices, bool haveDevices)
{
    std::vector<platform::PadDescription> out;

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

        platform::PadDescription desc;
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
                desc.kind          = platform::PadPathKind::McPsNative;
                desc.label         = model->name;
                desc.caps.touchpad = model->has_touchpad;

                desc.caps.analog_triggers = true;
            } else {
                desc.kind = platform::PadPathKind::McGeneric;

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
            desc.kind  = platform::PadPathKind::JoyCon;
            desc.style = HidNpadStyleTag_NpadHandheld;
            desc.label = "Handheld";
        } else if (styleSet & HidNpadStyleTag_NpadFullKey) {
            desc.kind  = platform::PadPathKind::SwitchPro;
            desc.style = HidNpadStyleTag_NpadFullKey;
            const bool backendEnumerated = haveDevices && devices.count > 0;
            desc.label = backendEnumerated ? "Pro Controller" : "Controller";
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

} // namespace

PadDriverState ResolvePadDriverState(const platform::PadDescription& desc,
                                     ExtendedInputManager& extended, const play::ControllerSettings& controllers)
{
    PadOutputInputs in;
    in.supported_pad = desc.kind == platform::PadPathKind::McPsNative
                    && FindPsModel(desc.vendor_id, desc.product_id) != nullptr;

    if (in.supported_pad) {
        const play::RumbleProfile profile =
            akira::platform::extended::ExtendedPadSettingsProvider::resolveProfile(controllers, 
                desc.vendor_id, desc.product_id,
                desc.has_address ? desc.bt_addr : nullptr, false, false);
        in.profile_native = profile.output_mode == play::PadOutputMode::Native;
    }

    in.backend_enabled   = !extended.backendReleased();
    in.ownership_refused = ExtendedInputManager::outputOwnershipRefused();

    in.wanted        = true;
    in.address_valid = true;
    in.owns_output   = !in.ownership_refused;

    const PadOutputState state = ResolvePadOutput(in);
    return { state.driver, state.reason };
}

PadDriver ResolvePadDriver(const platform::PadDescription& desc, ExtendedInputManager& extended,
                           const play::ControllerSettings& controllers)
{
    return ResolvePadDriverState(desc, extended, controllers).driver;
}

std::unique_ptr<PadPath> MakePath(const platform::PadDescription& desc, ExtendedInputManager& extended,
                                  const play::ControllerSettings& controllers)
{
    switch (desc.kind) {
        case platform::PadPathKind::McPsNative: {
            const PsModel* model = FindPsModel(desc.vendor_id, desc.product_id);
            if (model == nullptr)
                return nullptr;

            if (ResolvePadDriver(desc, extended, controllers) == PadDriver::MissionControl) {
                log::info("pad {:04x}:{:04x} driven by MissionControl"
                                   " - using the generic path",
                                   desc.vendor_id, desc.product_id);
                return std::make_unique<McGenericPath>(desc.npad, extended, controllers, desc.vendor_id,
                                                       desc.product_id);
            }

            return std::make_unique<McPsNativePath>(desc.npad, extended, controllers, *model,
                                                   desc.has_address ? desc.bt_addr : nullptr);
        }
        case platform::PadPathKind::McGeneric:
            return std::make_unique<McGenericPath>(desc.npad, extended, controllers, desc.vendor_id,
                                                   desc.product_id);
        case platform::PadPathKind::SwitchPro:
            return std::make_unique<SwitchProPath>(desc.npad, controllers, extended.rumble());
        case platform::PadPathKind::JoyCon:
        default:
            return std::make_unique<JoyConPath>(desc.npad, desc.style, controllers, extended.rumble());
    }
}

std::unique_ptr<PadPath> DefaultPadPath(ExtendedInputManager& extended, const play::ControllerSettings& controllers)
{
    return std::make_unique<LegacyPadPath>(extended, controllers);
}

} // namespace akira::platform::extended
