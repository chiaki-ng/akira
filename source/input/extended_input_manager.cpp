#include "input/extended_input_manager.hpp"
#include "input/sample_cadence.hpp"

#include <borealis.hpp>

#include <cstring>
#include <format>
#include <string>

#include <mutex>

#include "core/settings_manager.hpp"
#include "core/chiaki_core.hpp"
#include "input/pad_path.hpp"
#include "input/ps_output.hpp"

namespace {

bool serviceIsRunning(const char* name)
{
    Handle handle;
    const SmServiceName encoded = smEncodeName(name);
    if (R_SUCCEEDED(smRegisterService(&handle, encoded, false, 1))) {
        svcCloseHandle(handle);
        smUnregisterService(encoded);
        return false;
    }
    return true;
}

} // namespace

ExtendedInputManager::Availability ExtendedInputManager::probe(uint32_t* out_mc_version)
{
    if (out_mc_version) {
        *out_mc_version = 0;
    }

    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME)) {
        return Availability::NotInstalled;
    }

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME))) {
        return Availability::NotInstalled;
    }

    uint32_t api = 0;
    Result rc = serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api);
    if (R_FAILED(rc) || api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return Availability::VersionMismatch;
    }

    AkiraInputStatus status{};
    rc = serviceDispatchOut(&srv, AkiraInputCmd_GetStatus, status);
    serviceClose(&srv);

    if (R_FAILED(rc)) {
        return Availability::Failed;
    }
    if (out_mc_version) {
        *out_mc_version = status.mc_version;
    }

    if (status.backend_state == AkiraInputBackend_Unavailable) {
        return Availability::Unsupported;
    }
    if (status.backend_state == AkiraInputBackend_Failed) {
        return Availability::Failed;
    }
    return Availability::Available;
}

bool ExtendedInputManager::listDevicesOnce(AkiraInputDeviceList* out)
{
    if (out == nullptr || !serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME))
        return false;

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME)))
        return false;

    uint32_t api = 0;
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api)) ||
        api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return false;
    }

    const Result rc = serviceDispatchOut(&srv, AkiraInputCmd_ListDevices, *out);
    serviceClose(&srv);

    return R_SUCCEEDED(rc);
}

bool ExtendedInputManager::writeOutputReportOnce(const uint8_t* address, const uint8_t* data, uint16_t length)
{
    if (address == nullptr || data == nullptr || length == 0 || length > AKIRA_INPUT_OUTPUT_MAX)
        return false;
    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME))
        return false;

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME)))
        return false;

    uint32_t api = 0;
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api)) ||
        api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return false;
    }

    AkiraInputPadRef which{};
    std::memcpy(which.bt_addr, address, sizeof(which.bt_addr));

    const Result rc = serviceDispatchIn(&srv, AkiraInputCmd_WriteOutputReport, which,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
        .buffers = { { data, length } },
    );
    serviceClose(&srv);

    return R_SUCCEEDED(rc);
}

bool ExtendedInputManager::probeAudio(const uint8_t* address, AkiraInputAudioProbe* out)
{
    if (address == nullptr || out == nullptr || !serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME))
        return false;

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME)))
        return false;

    uint32_t api = 0;
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api)) ||
        api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return false;
    }

    AkiraInputPadRef which{};
    std::memcpy(which.bt_addr, address, sizeof(which.bt_addr));

    const Result rc = serviceDispatchInOut(&srv, AkiraInputCmd_ProbeAudio, which, *out);
    serviceClose(&srv);

    return R_SUCCEEDED(rc);
}

bool ExtendedInputManager::probeDirectWrite(const uint8_t* address, uint8_t method,
                                           const uint8_t* frame, uint16_t length,
                                           uint32_t* out_rc, uint32_t* out_init_rc)
{
    if (address == nullptr || frame == nullptr || length == 0 || out_rc == nullptr)
        return false;
    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME))
        return false;

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME)))
        return false;

    uint32_t api = 0;
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api)) ||
        api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return false;
    }

    AkiraInputDirectProbe probe{};
    std::memcpy(probe.bt_addr, address, sizeof(probe.bt_addr));
    probe.method = method;

    AkiraInputDirectResult result{};
    const Result rc = serviceDispatchInOut(&srv, AkiraInputCmd_ProbeDirectWrite, probe, result,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
        .buffers = { { frame, length } },
    );
    serviceClose(&srv);

    if (R_FAILED(rc))
        return false;

    *out_rc = result.rc;
    if (out_init_rc != nullptr)
        *out_init_rc = result.init_rc;
    return true;
}

bool ExtendedInputManager::readRawReportOnce(AkiraInputRawReport* out)
{
    if (out == nullptr)
        return false;
    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME))
        return false;

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME)))
        return false;

    uint32_t api = 0;
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api)) ||
        api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return false;
    }

    AkiraInputDeviceList devices{};
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_ListDevices, devices)) ||
        devices.count == 0) {
        serviceClose(&srv);
        return false;
    }

    AkiraInputTriggerQuery query{};
    std::memcpy(query.bt_addr, devices.devices[0].bt_addr, sizeof(query.bt_addr));

    const Result rc = serviceDispatchInOut(&srv, AkiraInputCmd_GetRawReport, query, *out);
    serviceClose(&srv);

    return R_SUCCEEDED(rc);
}

std::string ExtendedInputManager::describeDirectState()
{
    return std::string(directStreamAllowed()  ? "stream toggle: ON"  : "stream toggle: OFF")
         + (directHapticsAllowed() ? ",  haptics: on" : ",  haptics: off");
}

bool ExtendedInputManager::backendDirectOutputEnabled()
{
    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME))
        return true;

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME)))
        return true;

    u8 on = 1;
    const Result rc = serviceDispatchOut(&srv, AkiraInputCmd_GetDirectOutput, on);
    serviceClose(&srv);

    return R_FAILED(rc) || on != 0;
}

std::string ExtendedInputManager::describeActiveDevice()
{
    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME)) {
        return "akira-input is not running";
    }

    Service srv{};
    if (R_FAILED(smGetService(&srv, AKIRA_INPUT_IPC_SERVICE_NAME))) {
        return "akira-input is not running";
    }

    uint32_t api = 0;
    if (R_FAILED(serviceDispatchOut(&srv, AkiraInputCmd_GetApiVersion, api)) ||
        api != AKIRA_INPUT_IPC_API_VERSION) {
        serviceClose(&srv);
        return "version mismatch - update akira-input";
    }

    AkiraInputStatus status{};
    const bool live = R_SUCCEEDED(serviceDispatchOut(&srv, AkiraInputCmd_GetStatus, status)) &&
                      status.backend_state == AkiraInputBackend_Active;

    AkiraInputDeviceList devices{};
    const Result rc = serviceDispatchOut(&srv, AkiraInputCmd_ListDevices, devices);
    serviceClose(&srv);

    if (R_FAILED(rc)) {
        return "could not read controllers";
    }
    if (devices.count == 0) {
        return live ? "no controller reporting"
                    : "no controllers seen yet - start a stream or open the overlay";
    }

    for (uint8_t i = 0; i < devices.count; i++) {
        const AkiraInputDeviceInfo& d = devices.devices[i];
        if (!(d.flags & AkiraInputDevice_Active)) {
            continue;
        }
        const char* liveness = "";
        if (live) {
            liveness = (d.flags & AkiraInputDevice_Reporting) ? ", reporting" : ", asleep";
        }
        return std::format("{:04x}:{:04x}{}", d.vendor_id, d.product_id, liveness);
    }

    return std::format("{} controller(s), none selected - automatic", devices.count);
}

const char* ExtendedInputManager::describe(Availability availability)
{
    switch (availability) {
        case Availability::NotInstalled:     return "akira/settings/analog_triggers_not_installed";
        case Availability::Unsupported:      return "akira/settings/analog_triggers_unsupported";
        case Availability::VersionMismatch:  return "akira/settings/analog_triggers_mismatch";
        case Availability::Available:        return "akira/settings/analog_triggers_available";
        case Availability::Failed:           return "akira/settings/analog_triggers_failed";
    }
    return "akira/settings/analog_triggers_failed";
}

ExtendedInputManager::~ExtendedInputManager()
{
    shutdown();
}

uint64_t ExtendedInputManager::pack(uint8_t l2, uint8_t r2, bool valid, uint32_t ms)
{
    return static_cast<uint64_t>(l2)
         | (static_cast<uint64_t>(r2) << 8)
         | (valid ? kValidBit : 0)
         | (static_cast<uint64_t>(ms) << 32);
}

uint32_t ExtendedInputManager::nowMs()
{
    return static_cast<uint32_t>(armTicksToNs(armGetSystemTick()) / 1'000'000ull);
}

bool ExtendedInputManager::initializeOptional()
{
    if (!serviceIsRunning(AKIRA_INPUT_IPC_SERVICE_NAME)) {
        m_availability = Availability::NotInstalled;
        brls::Logger::info("analog triggers: akira-input not installed");
        return false;
    }

    Result rc = smGetService(&m_srv, AKIRA_INPUT_IPC_SERVICE_NAME);
    if (R_FAILED(rc)) {
        m_availability = Availability::NotInstalled;
        brls::Logger::info("analog triggers: could not open akira-input (0x{:x})", rc);
        return false;
    }

    uint32_t api = 0;
    rc = serviceDispatchOut(&m_srv, AkiraInputCmd_GetApiVersion, api);
    if (R_FAILED(rc) || api != AKIRA_INPUT_IPC_API_VERSION) {
        brls::Logger::warning("analog triggers: api {} != {} (rc=0x{:x})",
                              api, AKIRA_INPUT_IPC_API_VERSION, rc);
        m_availability = Availability::VersionMismatch;
        serviceClose(&m_srv);
        return false;
    }

    AkiraInputStatus status{};
    rc = serviceDispatchOut(&m_srv, AkiraInputCmd_GetStatus, status);
    if (R_FAILED(rc)) {
        m_availability = Availability::Failed;
        serviceClose(&m_srv);
        return false;
    }

    m_mc_version = status.mc_version;

    if (status.backend_state == AkiraInputBackend_Unavailable
        || status.backend_state == AkiraInputBackend_Failed) {
        m_availability = Availability::Unsupported;
        brls::Logger::info("analog triggers: backend unavailable (state {}, last 0x{:x})",
                           status.backend_state, static_cast<uint32_t>(status.last_error));
        serviceClose(&m_srv);
        return false;
    }

    rc = serviceDispatch(&m_srv, AkiraInputCmd_Subscribe);
    if (R_FAILED(rc)) {
        m_availability = Availability::Unsupported;
        brls::Logger::info("analog triggers: subscribe refused (0x{:x})", rc);
        serviceClose(&m_srv);
        return false;
    }
    m_subscribed = true;

    m_running.store(true, std::memory_order_release);
    auto started = libchiaking::platform::Thread::start(akira::ChiakiCore::current().threading(),
        "extended-input", libchiaking::platform::ThreadRole::feedback,
        [this](libchiaking::platform::StopToken) { poll(); });
    if (!started) {
        brls::Logger::warning("analog triggers: poll thread failed");
        m_running.store(false, std::memory_order_release);
        m_availability = Availability::Failed;
        shutdown();
        return false;
    }
    m_thread = std::move(started).value();

    m_availability = Availability::Available;
    brls::Logger::info("analog triggers: available (mc 0x{:06x})", m_mc_version);
    return true;
}

void ExtendedInputManager::shutdown()
{
    m_running.store(false, std::memory_order_release);

    m_thread = libchiaking::platform::Thread{};

    if (serviceIsActive(&m_srv)) {
        if (m_direct_addr_valid) {
            uint8_t frame[akira::input::kDs5BluetoothFrameBytes];

            if (m_direct_sent_valid && m_direct_sent != 0) {
                const size_t len = akira::input::BuildDs5RumbleFrame(0, 0, 0, frame, sizeof(frame));
                if (len > 0)
                    (void)writeDirectFrame(frame, (uint16_t)len);
            }

            {
                const size_t len = akira::input::BuildDs5HapticsRestoreFrame(
                    0, frame, sizeof(frame));
                if (len > 0)
                    (void)writeDirectFrame(frame, (uint16_t)len);
            }

            static const uint8_t kNull[sizeof(m_direct_trigger_last)] = {};
            if (m_direct_trigger_sent &&
                std::memcmp(m_direct_trigger_last, kNull, sizeof(kNull)) != 0) {
                const size_t len = akira::input::BuildDs5TriggerFrame(0, 0, kNull, 0, kNull,
                                                                      frame, sizeof(frame));
                if (len > 0)
                    (void)writeDirectFrame(frame, (uint16_t)len);
            }
        }

        ensureOutputOwnership(false);

        mutexLock(&m_couch_lock);
        for (auto& t : m_couch) {
            if (t.in_use && t.owns_output)
                ensureCouchOwnership(t, false, false);
            t.in_use = false;
        }
        mutexUnlock(&m_couch_lock);

        if (m_subscribed) {
            logStatus("shutdown");
        }
        if (m_subscribed) {
            (void)serviceDispatch(&m_srv, AkiraInputCmd_Unsubscribe);
            m_subscribed = false;
        }
        serviceClose(&m_srv);
    }

    m_snapshot.store(0, std::memory_order_relaxed);

    m_direct_wanted.store(false, std::memory_order_release);
    m_direct_rumble.store(0, std::memory_order_relaxed);
    m_direct_addr_valid = false;
    m_direct_discovery_state = DirectDiscoveryState::Unknown;
    m_direct_sent_valid = false;
    m_direct_logged     = false;
    m_direct_trigger_sent   = false;
    m_direct_trigger_logged = false;
}

bool ExtendedInputManager::listDevices(AkiraInputDeviceList* out) const
{
    if (out == nullptr || m_availability != Availability::Available)
        return false;

    mutexLock(&m_srv_lock);
    const Result rc = serviceDispatchOut(const_cast<Service*>(&m_srv),
                                         AkiraInputCmd_ListDevices, *out);
    mutexUnlock(&m_srv_lock);

    return R_SUCCEEDED(rc);
}

void ExtendedInputManager::setRawWanted(bool wanted)
{
    m_raw_wanted.store(wanted, std::memory_order_relaxed);
}

std::atomic<bool> ExtendedInputManager::s_direct_stream_allowed{false};
std::atomic<bool> ExtendedInputManager::s_output_refused{false};
std::atomic<uint32_t> ExtendedInputManager::s_direct_vid_pid{0};
std::atomic<bool> ExtendedInputManager::s_backend_released{false};

std::atomic<bool> ExtendedInputManager::s_direct_haptics_allowed{false};

void ExtendedInputManager::setDirectHapticsAllowed(bool allowed)
{
    s_direct_haptics_allowed.store(allowed, std::memory_order_release);
}

bool ExtendedInputManager::directHapticsAllowed()
{
    loadDirectGatesOnce();
    return s_direct_haptics_allowed.load(std::memory_order_acquire);
}

void ExtendedInputManager::loadDirectGatesOnce()
{
    static std::once_flag once;
    std::call_once(once, []() {
        auto* settings = SettingsManager::getInstance();
        if (settings == nullptr)
            return;

        const akira::input::RumbleProfile ds =
            settings->getRumbleProfile(akira::input::kRumbleKeyDualSense);

        s_direct_stream_allowed.store(
            ds.output_mode == akira::input::PadOutputMode::Native, std::memory_order_release);
        s_direct_haptics_allowed.store(
            ds.rumble_source != akira::input::RumbleSource::Off,
            std::memory_order_release);
    });
}

void ExtendedInputManager::setDirectStreamAllowed(bool allowed)
{
    s_direct_stream_allowed.store(allowed, std::memory_order_release);
}

bool ExtendedInputManager::directStreamAllowed()
{
    loadDirectGatesOnce();
    return s_direct_stream_allowed.load(std::memory_order_acquire);
}

void ExtendedInputManager::refreshDirectGates(uint16_t vendor_id, uint16_t product_id,
                                             const uint8_t* address)
{
    auto* settings = SettingsManager::getInstance();
    if (settings == nullptr)
        return;

    const akira::input::RumbleProfile profile =
        settings->resolveRumbleProfile(vendor_id, product_id, address, false, false);

    s_direct_vid_pid.store(((uint32_t)vendor_id << 16) | product_id,
                           std::memory_order_release);

    akira::input::PadOutputInputs in;
    in.supported_pad     = akira::input::PadTakesDirectOutput(vendor_id, product_id);
    in.profile_native    = profile.output_mode == akira::input::PadOutputMode::Native;
    in.backend_enabled   = !s_backend_released.load(std::memory_order_acquire);
    in.ownership_refused = outputOwnershipRefused();

    in.wanted        = true;
    in.address_valid = true;
    in.owns_output   = !in.ownership_refused;

    const bool native =
        akira::input::ResolvePadOutput(in).driver == akira::input::PadDriver::Akira;

    s_direct_stream_allowed.store(native, std::memory_order_release);
    const bool haptics = profile.rumble_source != akira::input::RumbleSource::Off;
    s_direct_haptics_allowed.store(haptics, std::memory_order_release);

    brls::Logger::info("direct gates for {:04x}:{:04x}: driver={} haptics={}",
                       vendor_id, product_id,
                       akira::input::PadDriverName(
                           native ? akira::input::PadDriver::Akira
                                  : akira::input::PadDriver::MissionControl),
                       haptics);
}

void ExtendedInputManager::setDirectOutput(bool enabled)
{
    if (!enabled) {
        m_direct_rumble.store(0, std::memory_order_relaxed);

        clearGameLightbar();
    }

    m_direct_wanted.store(enabled, std::memory_order_release);
}

void ExtendedInputManager::setDirectRumble(uint8_t left, uint8_t right)
{
    m_direct_rumble.store((uint16_t)(((uint16_t)left << 8) | right),
                          std::memory_order_relaxed);
}

void ExtendedInputManager::setConsoleIntensity(akira::input::Ds5EffectIntensity vibration,
                                              akira::input::Ds5EffectIntensity trigger)
{
    const uint8_t was_v = m_console_vibration.exchange((uint8_t)vibration,
                                                       std::memory_order_relaxed);
    const uint8_t was_t = m_console_trigger.exchange((uint8_t)trigger,
                                                     std::memory_order_relaxed);

    if (was_v == (uint8_t)vibration && was_t == (uint8_t)trigger)
        return;

    brls::Logger::info("console intensity: vibration={} triggers={} (byte 0x{:02x})",
                       (int)vibration, (int)trigger,
                       akira::input::Ds5IntensityByte(vibration, trigger));
}

akira::input::Ds5EffectIntensity ExtendedInputManager::consoleVibrationIntensity() const
{
    return (akira::input::Ds5EffectIntensity)m_console_vibration.load(std::memory_order_relaxed);
}

akira::input::Ds5EffectIntensity ExtendedInputManager::consoleTriggerIntensity() const
{
    return (akira::input::Ds5EffectIntensity)m_console_trigger.load(std::memory_order_relaxed);
}

void ExtendedInputManager::setGameLightbar(uint8_t red, uint8_t green, uint8_t blue)
{
    m_game_lightbar.store(kGameLightbarSet
                          | ((uint32_t)red << 16)
                          | ((uint32_t)green << 8)
                          | (uint32_t)blue,
                          std::memory_order_release);
}

void ExtendedInputManager::clearGameLightbar()
{
    m_game_lightbar.store(0, std::memory_order_release);
}

void ExtendedInputManager::setDirectTriggerEffects(uint8_t left_type, const uint8_t* left_params,
                                                   uint8_t right_type, const uint8_t* right_params)
{
    if (left_params == nullptr || right_params == nullptr)
        return;

    const uint32_t seq = m_direct_trigger_seq.load(std::memory_order_relaxed);
    m_direct_trigger_seq.store(seq + 1, std::memory_order_release);

    m_direct_trigger_left[0] = left_type;
    std::memcpy(m_direct_trigger_left + 1, left_params, akira::input::kDs5TriggerParamBytes);
    m_direct_trigger_right[0] = right_type;
    std::memcpy(m_direct_trigger_right + 1, right_params, akira::input::kDs5TriggerParamBytes);

    m_direct_trigger_seq.store(seq + 2, std::memory_order_release);
}

bool ExtendedInputManager::writeOutputReport(const uint8_t* address, const uint8_t* data, uint16_t length)
{
    if (length == 0 || length > AKIRA_INPUT_OUTPUT_MAX)
        return false;

    AkiraInputPadRef which{};
    std::memcpy(which.bt_addr, address, sizeof(which.bt_addr));

    mutexLock(&m_srv_lock);
    const Result rc = serviceDispatchIn(&m_srv, AkiraInputCmd_WriteOutputReport, which,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
        .buffers = { { data, length } });
    mutexUnlock(&m_srv_lock);

    return R_SUCCEEDED(rc);
}

bool ExtendedInputManager::writeDirectFrame(uint8_t* frame, uint16_t length)
{
    if (frame == nullptr || length < 5 || length > AKIRA_INPUT_OUTPUT_MAX)
        return false;

    AkiraInputPadRef which{};
    std::memcpy(which.bt_addr, m_direct_addr, sizeof(which.bt_addr));

    mutexLock(&m_srv_lock);
    frame[1] = (uint8_t)((m_direct_seq.fetch_add(1, std::memory_order_relaxed) & 0x0f) << 4);

    akira::input::StampDs5Crc(frame, length);

    const Result rc = serviceDispatchIn(&m_srv, AkiraInputCmd_WriteOutputReport, which,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
        .buffers = { { frame, length } });
    mutexUnlock(&m_srv_lock);

    return R_SUCCEEDED(rc);
}

akira::input::PadOutputState ExtendedInputManager::outputState() const
{
    akira::input::PadOutputInputs in;

    const uint32_t ids = s_direct_vid_pid.load(std::memory_order_acquire);
    in.supported_pad = ids != 0
        && akira::input::PadTakesDirectOutput((uint16_t)(ids >> 16),
                                              (uint16_t)(ids & 0xffff));

    in.profile_native    = s_direct_stream_allowed.load(std::memory_order_acquire);
    in.haptics_wanted    = s_direct_haptics_allowed.load(std::memory_order_acquire);
    in.backend_enabled   = !s_backend_released.load(std::memory_order_acquire);
    in.ownership_refused = s_output_refused.load(std::memory_order_acquire);
    in.owns_output       = m_owns_output;
    in.wanted            = m_direct_wanted.load(std::memory_order_acquire);
    in.address_valid     = m_direct_addr_valid;
    in.haptics_landing   = m_haptics_landing.load(std::memory_order_acquire);

    return akira::input::ResolvePadOutput(in);
}

bool ExtendedInputManager::directHapticsReady() const
{
    return outputState().stream_haptics;
}

bool ExtendedInputManager::sendDirectHaptics(uint8_t* frame, uint16_t length)
{
    if (!directHapticsReady() || frame == nullptr)
        return false;

    return writeDirectFrame(frame, length);
}

void ExtendedInputManager::ensureOutputOwnership(bool want)
{
    if (!serviceIsActive(&m_srv))
        return;

    const uint32_t now = nowMs();
    if (want) {
        if (now < m_owns_recheck)
            return;
        m_owns_recheck = now + 1000;
    } else if (!m_owns_output) {
        return;
    }

    if (want && !m_direct_addr_valid)
        return;

    AkiraInputExternalControl in{};
    std::memcpy(in.bt_addr, m_direct_addr, sizeof(in.bt_addr));
    in.acquire = want ? 1 : 0;

    uint32_t out_rc = 0;
    mutexLock(&m_srv_lock);
    const Result rc = serviceDispatchInOut(&m_srv, AkiraInputCmd_ExternalControl,
                                           in, out_rc);
    mutexUnlock(&m_srv_lock);

    const bool refused = R_FAILED(rc) || R_FAILED((Result)out_rc);
    if (want)
        s_output_refused.store(refused, std::memory_order_release);

    if (R_FAILED(rc)) {
        brls::Logger::warning("output ownership: {} failed ipc=0x{:x}",
                              want ? "acquire" : "release", rc);
        return;
    }

    if (want && refused) {
        const bool had = m_owns_output;
        m_owns_output = false;
        if (had) {
            brls::Logger::warning("output ownership: lost - the backend released it");
        }
        brls::Logger::info("output ownership: refused rc=0x{:x} - leaving the pad "
                           "to MissionControl", out_rc);
        return;
    }

    const bool took_it = want && !m_owns_output;
    const bool gave_it = !want && m_owns_output;
    m_owns_output = want;

    if (!want)
        m_lightbar_painted_valid = false;

    if (took_it || gave_it)
        brls::Logger::info("output ownership: {} rc=0x{:x} - link {}",
                           want ? "acquired" : "released", out_rc,
                           want ? "widened for haptics" : "back to its default width");

    if (took_it)
        paintLightbar();
}

bool ExtendedInputManager::resolveLightbar(uint8_t* rgb) const
{
    if (!m_direct_addr_valid || rgb == nullptr)
        return false;

    auto* settings = SettingsManager::getInstance();
    if (settings == nullptr)
        return false;

    const uint32_t ids = s_direct_vid_pid.load(std::memory_order_acquire);
    if (ids == 0)
        return false;

    const akira::input::RumbleProfile profile =
        settings->resolveRumbleProfile((uint16_t)(ids >> 16), (uint16_t)(ids & 0xffff),
                                       m_direct_addr, false, false);
    if (!profile.lightbar_enabled)
        return false;

    const uint32_t game = m_game_lightbar.load(std::memory_order_acquire);
    if (game & kGameLightbarSet) {
        rgb[0] = (uint8_t)(game >> 16);
        rgb[1] = (uint8_t)(game >> 8);
        rgb[2] = (uint8_t)game;
        return true;
    }

    rgb[0] = profile.lightbar_r;
    rgb[1] = profile.lightbar_g;
    rgb[2] = profile.lightbar_b;
    return true;
}

void ExtendedInputManager::paintLightbar()
{
    uint8_t rgb[3];
    if (!resolveLightbar(rgb))
        return;

    const uint32_t want = ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2];
    if (m_lightbar_painted_valid && m_lightbar_painted == want)
        return;

    const uint32_t now = nowMs();
    if (m_lightbar_painted_valid && (now - m_lightbar_painted_ms) < kLightbarIntervalMs)
        return;

    uint8_t frame[akira::input::kDs5BluetoothFrameBytes];
    const size_t len = akira::input::BuildDs5LightbarFrame(
        0, rgb[0], rgb[1], rgb[2], frame, sizeof(frame));
    if (len == 0)
        return;

    const bool ok = writeDirectFrame(frame, (uint16_t)len);
    if (ok) {
        m_lightbar_painted       = want;
        m_lightbar_painted_valid = true;
        m_lightbar_painted_ms    = now;
    }

    brls::Logger::info("lightbar: {:02x}{:02x}{:02x} {}",
                       rgb[0], rgb[1], rgb[2], ok ? "sent" : "refused");
}

void ExtendedInputManager::reconsiderBackendRefusal()
{
    if (!s_output_refused.load(std::memory_order_acquire))
        return;

    const uint32_t now = nowMs();
    if (now < m_refusal_recheck)
        return;
    m_refusal_recheck = now + 1000;

    const bool enabled = backendDirectOutputEnabled();
    s_backend_released.store(!enabled, std::memory_order_release);
    if (!enabled)
        return;

    s_output_refused.store(false, std::memory_order_release);
    brls::Logger::info("output ownership: the backend has native output on again");
}

void ExtendedInputManager::pumpDirectOutput()
{
    reconsiderBackendRefusal();

    pumpCouchOutputs(nowMs());

    const akira::input::PadOutputState state = outputState();
    const bool allowed = state.want_claim || state.write_state;

    ensureOutputOwnership(state.want_claim);

    if (!allowed || !m_direct_addr_valid) {
        if (!m_direct_gate_logged) {
            m_direct_gate_logged = true;
            brls::Logger::info("direct output blocked: stream_toggle={} addr_valid={} wanted={}",
                               allowed, m_direct_addr_valid,
                               m_direct_wanted.load(std::memory_order_acquire));
        }
        return;
    }

    if (!m_direct_gate_logged) {
        m_direct_gate_logged = true;
        brls::Logger::info("direct output live: addr valid, wanted={}",
                           m_direct_wanted.load(std::memory_order_acquire));
    }

    paintLightbar();

    const uint32_t now = nowMs();
    pumpDirectState(now);
}

bool ExtendedInputManager::directWriteFailed()
{
    if (++m_direct_failures < kDirectFailureLimit)
        return false;

    ensureOutputOwnership(false);
    m_direct_addr_valid = false;
    return true;
}

void ExtendedInputManager::pumpDirectState(uint32_t now)
{
    const bool wanted = m_direct_wanted.load(std::memory_order_acquire);

    const uint16_t want_rumble = wanted
        ? m_direct_rumble.load(std::memory_order_relaxed)
        : 0;

    uint8_t want_triggers[sizeof(m_direct_trigger_last)] = {};
    if (wanted) {
        const uint32_t seq = m_direct_trigger_seq.load(std::memory_order_acquire);
        if (seq != 0 && (seq & 1u) == 0) {
            std::memcpy(want_triggers, m_direct_trigger_left, sizeof(m_direct_trigger_left));
            std::memcpy(want_triggers + sizeof(m_direct_trigger_left),
                        m_direct_trigger_right, sizeof(m_direct_trigger_right));

            if (m_direct_trigger_seq.load(std::memory_order_acquire) != seq)
                return;
        }
    }

    if (!wanted && (!m_direct_sent_valid || (m_direct_sent == 0 && !m_direct_trigger_sent)))
        return;

    const uint8_t want_intensity = akira::input::Ds5IntensityByte(consoleVibrationIntensity(),
                                                                  consoleTriggerIntensity());

    const bool same = m_direct_sent_valid
                   && want_rumble == m_direct_sent
                   && want_intensity == m_direct_intensity_last
                   && std::memcmp(want_triggers, m_direct_trigger_last,
                                  sizeof(want_triggers)) == 0;
    if (same)
        return;

    if (m_direct_sent_valid && (now - m_direct_sent_ms) < kDirectWriteIntervalMs)
        return;

    const uint8_t* left  = want_triggers;
    const uint8_t* right = want_triggers + sizeof(m_direct_trigger_left);

    uint8_t frame[akira::input::kDs5BluetoothFrameBytes];
    const size_t len = akira::input::BuildDs5StateFrame(
        0,
        (uint8_t)(want_rumble >> 8), (uint8_t)(want_rumble & 0xff),
        left[0], left + 1, right[0], right + 1,
        frame, sizeof(frame), false, akira::input::Ds5FrameLayout::Tagged, want_intensity);
    if (len == 0)
        return;

    if (!writeDirectFrame(frame, (uint16_t)len)) {
        m_direct_sent_ms = now;
        (void)directWriteFailed();
        return;
    }

    m_direct_sent            = want_rumble;
    m_direct_sent_valid      = true;
    m_direct_intensity_last  = want_intensity;
    m_direct_sent_ms         = now;
    m_direct_trigger_sent    = true;
    m_direct_trigger_sent_ms = now;
    m_direct_failures        = 0;
    std::memcpy(m_direct_trigger_last, want_triggers, sizeof(want_triggers));

    if (!m_direct_trigger_logged) {
        m_direct_trigger_logged = true;
        brls::Logger::info("direct state: rumble={:04x} triggers left=0x{:02x} right=0x{:02x} "
                           "intensity=0x{:02x}",
                           want_rumble, left[0], right[0], want_intensity);
    }
}


ExtendedInputManager::CouchOutputTarget* ExtendedInputManager::findCouch(const uint8_t* bt_addr)
{
    for (auto& t : m_couch) {
        if (t.in_use && std::memcmp(t.bt_addr, bt_addr, sizeof(t.bt_addr)) == 0)
            return &t;
    }
    return nullptr;
}

void ExtendedInputManager::registerCouchOutput(const uint8_t* bt_addr, uint16_t vendor_id,
                                               uint16_t product_id)
{
    if (bt_addr == nullptr)
        return;

    mutexLock(&m_couch_lock);
    CouchOutputTarget* t = findCouch(bt_addr);
    if (t == nullptr) {
        for (auto& c : m_couch) {
            if (!c.in_use) {
                t = &c;
                break;
            }
        }
    }
    if (t != nullptr) {
        const bool fresh = !t->in_use;
        t->in_use = true;
        std::memcpy(t->bt_addr, bt_addr, sizeof(t->bt_addr));
        t->vendor_id  = vendor_id;
        t->product_id = product_id;
        if (fresh) {
            t->rumble        = 0;
            t->have_triggers = false;
            t->have_lightbar = false;
            t->sent_valid    = false;
            t->trigger_sent  = false;
            t->lightbar_painted_valid = false;
            t->owns_output   = false;
            t->owns_recheck  = 0;
            t->failures      = 0;
        }
    }
    mutexUnlock(&m_couch_lock);

    if (t == nullptr)
        brls::Logger::warning("couch output: no free slot for pad {:04x}:{:04x}",
                              vendor_id, product_id);
    else
        brls::Logger::info("couch output: registered {:04x}:{:04x}", vendor_id, product_id);
}

void ExtendedInputManager::unregisterCouchOutput(const uint8_t* bt_addr)
{
    if (bt_addr == nullptr)
        return;

    mutexLock(&m_couch_lock);
    if (CouchOutputTarget* t = findCouch(bt_addr)) {
        if (t->owns_output)
            ensureCouchOwnership(*t, false, false);
        t->in_use = false;
    }
    mutexUnlock(&m_couch_lock);
}

void ExtendedInputManager::setCouchRumble(const uint8_t* bt_addr, uint8_t left, uint8_t right)
{
    if (bt_addr == nullptr)
        return;
    mutexLock(&m_couch_lock);
    if (CouchOutputTarget* t = findCouch(bt_addr))
        t->rumble = (uint16_t)(((uint16_t)left << 8) | right);
    mutexUnlock(&m_couch_lock);
}

void ExtendedInputManager::setCouchTriggerEffects(const uint8_t* bt_addr,
                                                  uint8_t left_type, const uint8_t* left_params,
                                                  uint8_t right_type, const uint8_t* right_params)
{
    if (bt_addr == nullptr || left_params == nullptr || right_params == nullptr)
        return;
    mutexLock(&m_couch_lock);
    if (CouchOutputTarget* t = findCouch(bt_addr)) {
        t->trigger_left[0] = left_type;
        std::memcpy(t->trigger_left + 1, left_params, akira::input::kDs5TriggerParamBytes);
        t->trigger_right[0] = right_type;
        std::memcpy(t->trigger_right + 1, right_params, akira::input::kDs5TriggerParamBytes);
        t->have_triggers = true;
    }
    mutexUnlock(&m_couch_lock);
}

void ExtendedInputManager::setCouchLightbar(const uint8_t* bt_addr, uint8_t red, uint8_t green,
                                            uint8_t blue)
{
    if (bt_addr == nullptr)
        return;
    mutexLock(&m_couch_lock);
    if (CouchOutputTarget* t = findCouch(bt_addr)) {
        t->lightbar[0] = red;
        t->lightbar[1] = green;
        t->lightbar[2] = blue;
        t->have_lightbar = true;
    }
    mutexUnlock(&m_couch_lock);
}

bool ExtendedInputManager::writeCouchFrame(CouchOutputTarget& t, uint8_t* frame, uint16_t length)
{
    if (frame == nullptr || length < 5 || length > AKIRA_INPUT_OUTPUT_MAX)
        return false;

    frame[1] = (uint8_t)((t.seq++ & 0x0f) << 4);
    akira::input::StampDs5Crc(frame, length);
    return writeOutputReport(t.bt_addr, frame, length);
}

void ExtendedInputManager::ensureCouchOwnership(CouchOutputTarget& t, bool want, bool regime_on)
{
    if (!serviceIsActive(&m_srv))
        return;

    const uint32_t now = nowMs();
    if (want) {
        if (!regime_on)
            return;
        if (now < t.owns_recheck)
            return;
        t.owns_recheck = now + 1000;
    } else if (!t.owns_output) {
        return;
    }

    AkiraInputExternalControl in{};
    std::memcpy(in.bt_addr, t.bt_addr, sizeof(in.bt_addr));
    in.acquire = want ? 1 : 0;

    uint32_t out_rc = 0;
    mutexLock(&m_srv_lock);
    const Result rc = serviceDispatchInOut(&m_srv, AkiraInputCmd_ExternalControl, in, out_rc);
    mutexUnlock(&m_srv_lock);

    if (R_FAILED(rc))
        return;

    const bool refused = R_FAILED((Result)out_rc);
    if (want && refused) {
        t.owns_output = false;
        return;
    }

    if (!want)
        t.lightbar_painted_valid = false;

    t.owns_output = want;
}

void ExtendedInputManager::paintCouchLightbar(CouchOutputTarget& t, uint32_t now)
{
    if (!t.have_lightbar)
        return;

    if (t.lightbar_painted_valid && std::memcmp(t.lightbar_painted, t.lightbar, 3) == 0)
        return;
    if (t.lightbar_painted_valid && (now - t.lightbar_painted_ms) < kLightbarIntervalMs)
        return;

    uint8_t frame[akira::input::kDs5BluetoothFrameBytes];
    const size_t len = akira::input::BuildDs5LightbarFrame(
        0, t.lightbar[0], t.lightbar[1], t.lightbar[2], frame, sizeof(frame));
    if (len == 0)
        return;

    if (writeCouchFrame(t, frame, (uint16_t)len)) {
        std::memcpy(t.lightbar_painted, t.lightbar, 3);
        t.lightbar_painted_valid = true;
        t.lightbar_painted_ms    = now;
    }
}

void ExtendedInputManager::pumpCouchOutputs(uint32_t now)
{
    const bool regime = s_direct_stream_allowed.load(std::memory_order_acquire)
                     && !s_backend_released.load(std::memory_order_acquire)
                     && !s_output_refused.load(std::memory_order_acquire)
                     && m_direct_wanted.load(std::memory_order_acquire);

    const uint8_t intensity = akira::input::Ds5IntensityByte(consoleVibrationIntensity(),
                                                             consoleTriggerIntensity());

    mutexLock(&m_couch_lock);
    for (auto& t : m_couch) {
        if (!t.in_use)
            continue;

        const bool supported = akira::input::PadTakesDirectOutput(t.vendor_id, t.product_id);
        const bool want = regime && supported;

        ensureCouchOwnership(t, want, regime);
        if (!want || !t.owns_output)
            continue;

        paintCouchLightbar(t, now);

        const uint16_t want_rumble = t.rumble;
        uint8_t want_triggers[sizeof(t.trigger_last)] = {};
        if (t.have_triggers) {
            std::memcpy(want_triggers, t.trigger_left, sizeof(t.trigger_left));
            std::memcpy(want_triggers + sizeof(t.trigger_left), t.trigger_right,
                        sizeof(t.trigger_right));
        }

        const bool same = t.sent_valid
                       && want_rumble == t.sent
                       && intensity == t.intensity_last
                       && std::memcmp(want_triggers, t.trigger_last, sizeof(want_triggers)) == 0;
        if (same)
            continue;
        if (t.sent_valid && (now - t.sent_ms) < kDirectWriteIntervalMs)
            continue;

        const uint8_t* left  = want_triggers;
        const uint8_t* right = want_triggers + sizeof(t.trigger_left);

        uint8_t frame[akira::input::kDs5BluetoothFrameBytes];
        const size_t len = akira::input::BuildDs5StateFrame(
            0,
            (uint8_t)(want_rumble >> 8), (uint8_t)(want_rumble & 0xff),
            left[0], left + 1, right[0], right + 1,
            frame, sizeof(frame), false, akira::input::Ds5FrameLayout::Tagged, intensity);
        if (len == 0)
            continue;

        if (!writeCouchFrame(t, frame, (uint16_t)len)) {
            t.sent_ms = now;
            if (++t.failures >= kDirectFailureLimit)
                ensureCouchOwnership(t, false, false);
            continue;
        }

        t.sent           = want_rumble;
        t.sent_valid     = true;
        t.intensity_last = intensity;
        t.sent_ms        = now;
        t.trigger_sent   = true;
        t.failures       = 0;
        std::memcpy(t.trigger_last, want_triggers, sizeof(want_triggers));
    }
    mutexUnlock(&m_couch_lock);
}

bool ExtendedInputManager::copyRawSlot(const RawSlot& slot, AkiraInputRawReport* out) const
{
    for (int attempt = 0; attempt < 3; attempt++) {
        const uint32_t before = slot.seq.load(std::memory_order_acquire);
        if (before & 1u)
            continue;

        AkiraInputRawReport copy = slot.report;

        const uint32_t after = slot.seq.load(std::memory_order_acquire);
        if (before != after)
            continue;

        if (before == 0 || copy.length == 0)
            return false;

        const uint32_t published = slot.published_ms.load(std::memory_order_acquire);
        if (nowMs() - published > kMaxAgeMs)
            return false;

        *out = copy;
        return true;
    }

    return false;
}

bool ExtendedInputManager::readRawReportFor(const uint8_t* bt_addr, AkiraInputRawReport* out) const
{
    if (out == nullptr || bt_addr == nullptr)
        return false;

    for (const RawSlot& slot : m_raw_slots) {
        AkiraInputRawReport copy{};
        if (!copyRawSlot(slot, &copy))
            continue;
        if (std::memcmp(copy.bt_addr, bt_addr, sizeof(copy.bt_addr)) != 0)
            continue;
        *out = copy;
        return true;
    }

    return false;
}

bool ExtendedInputManager::readRawReport(AkiraInputRawReport* out) const
{
    if (out == nullptr)
        return false;

    if (m_direct_addr_valid && readRawReportFor(m_direct_addr, out))
        return true;

    for (const RawSlot& slot : m_raw_slots) {
        if (copyRawSlot(slot, out))
            return true;
    }

    return false;
}

int ExtendedInputManager::claimRawSlot(const uint8_t* bt_addr)
{
    for (int i = 0; i < kMaxRawSlots; i++) {
        if (m_raw_slot_used[i] &&
            std::memcmp(m_raw_slot_addr[i], bt_addr, sizeof(m_raw_slot_addr[i])) == 0) {
            return i;
        }
    }

    for (int i = 0; i < kMaxRawSlots; i++) {
        if (!m_raw_slot_used[i]) {
            m_raw_slot_used[i] = true;
            std::memcpy(m_raw_slot_addr[i], bt_addr, sizeof(m_raw_slot_addr[i]));
            return i;
        }
    }

    int oldest = 0;
    uint32_t oldest_ms = m_raw_slots[0].published_ms.load(std::memory_order_acquire);
    for (int i = 1; i < kMaxRawSlots; i++) {
        const uint32_t ms = m_raw_slots[i].published_ms.load(std::memory_order_acquire);
        if (ms < oldest_ms) {
            oldest_ms = ms;
            oldest    = i;
        }
    }
    std::memcpy(m_raw_slot_addr[oldest], bt_addr, sizeof(m_raw_slot_addr[oldest]));
    return oldest;
}

void ExtendedInputManager::publishRawReport(const AkiraInputRawReport& raw)
{
    const int index = claimRawSlot(raw.bt_addr);
    RawSlot& slot = m_raw_slots[index];

    const uint32_t seq = slot.seq.load(std::memory_order_relaxed);
    slot.seq.store(seq + 1, std::memory_order_release);
    slot.report = raw;
    slot.seq.store(seq + 2, std::memory_order_release);

    slot.published_ms.store(nowMs(), std::memory_order_release);
}

void ExtendedInputManager::refreshTrackedDevices()
{
    AkiraInputDeviceList devices{};
    if (!listDevices(&devices)) {
        return;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < devices.count && count < kMaxRawSlots; i++) {
        const AkiraInputDeviceInfo& dev = devices.devices[i];
        if ((dev.flags & AkiraInputDevice_Identified) == 0)
            continue;
        std::memcpy(m_tracked_addr[count], dev.bt_addr, sizeof(m_tracked_addr[count]));
        count++;
    }

    m_tracked_count = count;
}

void ExtendedInputManager::logStatus(const char* when)
{
    AkiraInputStatus status{};
    const Result rc = serviceDispatchOut(&m_srv, AkiraInputCmd_GetStatus, status);
    if (R_FAILED(rc)) {
        brls::Logger::warning("analog triggers [{}]: status read failed (0x{:x})", when, rc);
        return;
    }

    const uint64_t wrote  = status.output_written - m_haptics_written_seen;
    const uint64_t missed = status.output_failed  - m_haptics_failed_seen;
    m_haptics_written_seen = status.output_written;
    m_haptics_failed_seen  = status.output_failed;

    if (missed > 0 && wrote == 0) {
        if (m_haptics_landing.exchange(false, std::memory_order_acq_rel)) {
            brls::Logger::warning(
                "haptic stream: {} report(s) refused and none accepted - falling back to motors",
                missed);
        }
    } else if (wrote > 0) {
        m_haptics_landing.store(true, std::memory_order_release);
    }

    const uint64_t reports = status.reports_received - m_reports_seen;
    m_reports_seen = status.reports_received;

    brls::Logger::info(
        "analog triggers [{}]: state={} subs={} claims={} reports=+{} last=0x{:x}",
        when, status.backend_state, status.subscriber_count,
        status.claims_held, reports,
        static_cast<uint32_t>(status.last_error));

    if (status.claims_held > 0 && reports == 0) {
        brls::Logger::warning(
            "analog triggers [{}]: {} claim(s) held and no reports arriving",
            when, status.claims_held);
    }

    logRawReport(when);

    if (status.last_unknown_vid_pid != 0) {
        brls::Logger::warning(
            "analog triggers [{}]: unrecognised pad vid={:04x} pid={:04x} - no analog for it",
            when,
            status.last_unknown_vid_pid >> 16,
            status.last_unknown_vid_pid & 0xffff);
    }

}

void ExtendedInputManager::logRawReport(const char* when)
{
    if (!m_direct_addr_valid) {
        return;
    }

    AkiraInputTriggerQuery query{};
    std::memcpy(query.bt_addr, m_direct_addr, sizeof(query.bt_addr));

    AkiraInputRawReport raw{};
    const Result rc = serviceDispatchInOut(&m_srv, AkiraInputCmd_GetRawReport, query, raw);
    if (R_FAILED(rc)) {
        return;
    }

    const size_t len = raw.length > AKIRA_INPUT_RAW_MAX ? AKIRA_INPUT_RAW_MAX : raw.length;

    std::string hex;
    hex.reserve(len * 3);
    for (size_t i = 0; i < len; i++) {
        hex += std::format("{:02x}", raw.data[i]);
        if (i + 1 < len) {
            hex += ' ';
        }
    }

    brls::Logger::info(
        "analog triggers [{}]: raw vid={:04x} pid={:04x} report=0x{:02x} len={} desc_len={}",
        when, raw.vendor_id, raw.product_id, raw.report_id, raw.length,
        raw.descriptor_length);
    brls::Logger::info("analog triggers [{}]: raw bytes {}", when, hex);
}

void ExtendedInputManager::resolveDirectAddress()
{
    AkiraInputDeviceList devices{};
    if (!listDevices(&devices)) {
        if (m_direct_discovery_state != DirectDiscoveryState::Failed)
            brls::Logger::warning("direct output: listDevices failed, no raw reports will be polled");
        m_direct_discovery_state = DirectDiscoveryState::Failed;
        return;
    }

    if (devices.count == 0) {
        if (m_direct_discovery_state != DirectDiscoveryState::Empty)
            brls::Logger::info("direct output: device list empty, waiting for a controller");
        m_direct_discovery_state = DirectDiscoveryState::Empty;
        return;
    }

    if (m_direct_discovery_state != DirectDiscoveryState::Candidates) {
        for (uint8_t i = 0; i < devices.count; i++) {
            const AkiraInputDeviceInfo& d = devices.devices[i];
            brls::Logger::info("direct output: candidate {:04x}:{:04x} flags=0x{:02x} takes_direct={}",
                               d.vendor_id, d.product_id, d.flags,
                               akira::input::PadTakesDirectOutput(d.vendor_id, d.product_id));
        }
    }
    m_direct_discovery_state = DirectDiscoveryState::Candidates;

    for (uint8_t i = 0; i < devices.count; i++) {
        const AkiraInputDeviceInfo& dev = devices.devices[i];
        if (!akira::input::PadTakesDirectOutput(dev.vendor_id, dev.product_id)) {
            continue;
        }

        if (!m_direct_addr_valid ||
            std::memcmp(m_direct_addr, dev.bt_addr, sizeof(m_direct_addr)) != 0) {
            std::memcpy(m_direct_addr, dev.bt_addr, sizeof(m_direct_addr));
            m_direct_addr_valid = true;
            brls::Logger::info(
                "direct output: pad {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x} "
                "({:04x}:{:04x}) from the device list",
                dev.bt_addr[0], dev.bt_addr[1], dev.bt_addr[2],
                dev.bt_addr[3], dev.bt_addr[4], dev.bt_addr[5],
                dev.vendor_id, dev.product_id);
        }

        refreshDirectGates(dev.vendor_id, dev.product_id, dev.bt_addr);
        return;
    }
}

void ExtendedInputManager::poll()
{
    uint32_t ticks = 0;
    akira::input::SampleCadence discovery_cadence(akira::input::DirectDiscoveryPeriod);

    while (m_running.load(std::memory_order_acquire)) {
        if (!m_direct_addr_valid &&
            discovery_cadence.due(akira::input::SampleCadence::Clock::now())) {
            resolveDirectAddress();
        }

        if (!m_direct_addr_valid) {
            m_snapshot.store(0, std::memory_order_relaxed);
            svcSleepThread(kPollIntervalNs);
            continue;
        }

        AkiraInputTriggerQuery query{};
        std::memcpy(query.bt_addr, m_direct_addr, sizeof(query.bt_addr));

        AkiraInputTriggerState state{};
        mutexLock(&m_srv_lock);
        const Result rc = serviceDispatchInOut(&m_srv, AkiraInputCmd_GetTriggerState, query, state);
        mutexUnlock(&m_srv_lock);

        if (R_SUCCEEDED(rc) && state.capability == AkiraInputTrigger_Analog && state.raw_max > 0) {
            const uint32_t l2 = (static_cast<uint32_t>(state.l2_raw) * 0xff) / state.raw_max;
            const uint32_t r2 = (static_cast<uint32_t>(state.r2_raw) * 0xff) / state.raw_max;
            m_snapshot.store(pack(static_cast<uint8_t>(l2 > 0xff ? 0xff : l2),
                                  static_cast<uint8_t>(r2 > 0xff ? 0xff : r2),
                                  true, nowMs()),
                             std::memory_order_relaxed);
        } else if (R_FAILED(rc) && rc != AKIRA_INPUT_ERR_NO_STATE) {
            brls::Logger::warning("analog triggers: backend lost (0x{:x})", rc);
            m_snapshot.store(0, std::memory_order_relaxed);
            m_degraded_notice.store(true, std::memory_order_release);
            m_running.store(false, std::memory_order_release);
            return;
        } else {
            m_snapshot.store(0, std::memory_order_relaxed);
        }

        if (m_raw_wanted.load(std::memory_order_relaxed)) {
            if (m_tracked_count == 0 || (m_tracked_refresh++ % 120) == 0)
                refreshTrackedDevices();

            for (uint8_t i = 0; i < m_tracked_count; i++) {
                AkiraInputTriggerQuery raw_query{};
                std::memcpy(raw_query.bt_addr, m_tracked_addr[i], sizeof(raw_query.bt_addr));

                AkiraInputRawReport raw{};
                mutexLock(&m_srv_lock);
                const Result raw_rc =
                    serviceDispatchInOut(&m_srv, AkiraInputCmd_GetRawReport, raw_query, raw);
                mutexUnlock(&m_srv_lock);

                if (R_FAILED(raw_rc) || raw.length == 0)
                    continue;

                publishRawReport(raw);

                if (!akira::input::PadTakesDirectOutput(raw.vendor_id, raw.product_id))
                    continue;

                if (!m_direct_addr_valid ||
                    std::memcmp(m_direct_addr, raw.bt_addr, sizeof(m_direct_addr)) != 0) {
                    if (m_direct_addr_valid)
                        continue;
                    std::memcpy(m_direct_addr, raw.bt_addr, sizeof(m_direct_addr));
                    m_direct_addr_valid = true;
                    refreshDirectGates(raw.vendor_id, raw.product_id, raw.bt_addr);
                    m_direct_sent_valid = false;
                    m_direct_failures   = 0;
                    m_haptics_landing.store(true, std::memory_order_release);
                }
            }
        }

        pumpDirectOutput();

        if (++ticks >= kStatusLogInterval) {
            ticks = 0;
            logStatus("stream");
        }

        svcSleepThread(kPollIntervalNs);
    }
}


bool ExtendedInputManager::consumeDegradedNotice()
{
    return m_degraded_notice.exchange(false, std::memory_order_acq_rel);
}

bool ExtendedInputManager::hasFreshAnalogState() const
{
    const uint64_t snap = m_snapshot.load(std::memory_order_relaxed);
    if ((snap & kValidBit) == 0) {
        return false;
    }
    return (nowMs() - static_cast<uint32_t>(snap >> 32)) < kMaxAgeMs;
}

uint8_t ExtendedInputManager::l2() const
{
    return static_cast<uint8_t>(m_snapshot.load(std::memory_order_relaxed) & 0xff);
}

uint8_t ExtendedInputManager::r2() const
{
    return static_cast<uint8_t>((m_snapshot.load(std::memory_order_relaxed) >> 8) & 0xff);
}
