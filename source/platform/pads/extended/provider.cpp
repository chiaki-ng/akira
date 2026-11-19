#include "platform/pads/extended/provider.hpp"
#include "platform/pad_settings/extended/provider.hpp"
#include "log/log.hpp"

#include <algorithm>

#include "play/controller_settings.hpp"
#include "platform/pads/extended/ps_output.hpp"
#include "play/input_manager.hpp"
#include "play/stream_stats.hpp"

namespace akira::platform::extended
{

ExtendedPadProvider::ExtendedPadProvider(const play::ControllerSettings& controllers)
    : m_controllers(controllers)
    , m_extended(controllers)
{
}

void ExtendedPadProvider::open()
{
    setPathLocked(akira::platform::extended::DefaultPadPath(m_extended, m_controllers));

    m_extended.initializeOptional();

    if (m_bound_npad != kNoNpad)
    {
        HidNpadIdType wanted = m_bound_npad;
        m_bound_npad = kNoNpad;
        for (const auto& desc : describePadsLocked()) {
            if (desc.npad != wanted)
                continue;
            auto path = akira::platform::extended::MakePath(desc, m_extended, m_controllers);
            if (path) {
                m_bound_npad = wanted;
                setPathLocked(std::move(path));
            }
            break;
        }
    }
}

void ExtendedPadProvider::close()
{
    m_extended.shutdown();
    if (m_path) {
        m_path.reset();
        ++m_path_generation;
    }
}

input::Pad* ExtendedPadProvider::acquire(Callbacks& callbacks)
{
    pollPadArrivals(callbacks);
    pollCouchClaim(callbacks);

    if (!m_path)
        return nullptr;

    retryPathIdentification();
    reconcilePathDriver();

    m_path->setExcludedNpads(couchNpadMask());
    return m_path.get();
}

input::Pad* ExtendedPadProvider::current()
{
    return m_path.get();
}

std::vector<input::PadDescription> ExtendedPadProvider::scanPads() const
{
    return akira::platform::extended::DescribePads();
}

play::RumbleProfile ExtendedPadProvider::primaryRumbleProfile() const
{
    const std::vector<input::PadDescription> pads = scanPads();
    if (pads.empty())
        return m_controllers.rumbleProfile(play::kRumbleKeyDefault);

    const input::PadDescription& pad = pads.front();
    const bool switchNative = pad.kind == input::PadPathKind::JoyCon
                           || pad.kind == input::PadPathKind::SwitchPro;

    return akira::platform::extended::ExtendedPadSettingsProvider::resolveProfile(m_controllers, pad.vendor_id, pad.product_id,
                                          pad.has_address ? pad.bt_addr : nullptr,
                                          switchNative,
                                          pad.kind == input::PadPathKind::JoyCon);
}

void ExtendedPadProvider::fillStreamStats(play::StreamStats& stats)
{
    stats.analog_triggers_active = m_extended.hasFreshAnalogState();
    stats.analog_l2 = m_extended.l2();
    stats.analog_r2 = m_extended.r2();
}

void ExtendedPadProvider::routeSeatFeedback(uint8_t slot, const libchiaking::controllers::ControllerFeedback& feedback)
{
    uint8_t addr[6];
    uint16_t vid = 0;
    uint16_t pid = 0;
    if (!couchSlotOutputTarget(slot, addr, &vid, &pid))
        return;

    if (const auto* rumble = feedback.get<libchiaking::controllers::Rumble>())
    {
        m_extended.registerCouchOutput(addr, vid, pid);
        m_extended.setCouchRumble(addr, rumble->left, rumble->right);
    }
    else if (const auto* effects = feedback.get<libchiaking::controllers::TriggerEffects>())
    {
        m_extended.registerCouchOutput(addr, vid, pid);
        m_extended.setCouchTriggerEffects(addr,
            effects->left.mode, effects->left.parameters.data(),
            effects->right.mode, effects->right.parameters.data());
    }
    else if (const auto* confirmed = feedback.get<libchiaking::controllers::SeatConfirmed>())
    {
        const auto& colour = confirmed->colour;
        if ((colour.red | colour.green | colour.blue) == 0)
            return;
        m_extended.registerCouchOutput(addr, vid, pid);
        m_extended.setCouchLightbar(addr, colour.red, colour.green, colour.blue);
    }
}

void ExtendedPadProvider::setPath(std::unique_ptr<akira::platform::extended::PadPath> path)
{
    std::lock_guard<std::mutex> lock(mutex());
    setPathLocked(std::move(path));
}

void ExtendedPadProvider::setPathLocked(std::unique_ptr<akira::platform::extended::PadPath> path)
{
    if (!path)
        return;

    if (m_path)
        m_path->sendRumble(0.0f, 0.0f, 0.0f, 0.0f);
    m_path = std::move(path);
    ++m_path_generation;

    log::info("InputManager: input path is now {}", m_path->label());
}

PadPathInfo ExtendedPadProvider::pathInfo() const
{
    std::lock_guard<std::mutex> lock(mutex());
    return pathInfoLocked();
}

PadPathInfo ExtendedPadProvider::pathInfoLocked() const
{
    PadPathInfo info;
    info.generation = m_path_generation;
    if (!m_path)
        return info;

    info.available = true;
    info.kind = m_path->kind();
    info.npad = m_path->npad();
    info.style = m_path->style();
    info.vendorId = m_path->vendorId();
    info.productId = m_path->productId();
    info.nativeRumble = m_path->nativeRumble();
    info.switchNative = m_path->switchNative();
    if (const uint8_t* address = m_path->address()) {
        std::copy_n(address, info.address.size(), info.address.begin());
        info.hasAddress = true;
    }
    return info;
}

void ExtendedPadProvider::sendTriggerEffects(const akira::platform::extended::PadPath::TriggerEffect& left,
    const akira::platform::extended::PadPath::TriggerEffect& right)
{
    std::lock_guard<std::mutex> lock(mutex());
    if (m_path)
        m_path->sendTriggerEffects(left, right);
}

void ExtendedPadProvider::sendEffectIntensity(uint8_t vibration, uint8_t trigger)
{
    std::lock_guard<std::mutex> lock(mutex());
    if (m_path)
        m_path->sendEffectIntensity(vibration, trigger);
}

void ExtendedPadProvider::sendLightbar(uint8_t red, uint8_t green, uint8_t blue)
{
    std::lock_guard<std::mutex> lock(mutex());
    if (m_path)
        m_path->sendLightbar(red, green, blue);
}

std::vector<input::PadDescription> ExtendedPadProvider::describePads()
{
    std::lock_guard<std::mutex> lock(mutex());
    return describePadsLocked();
}

std::vector<input::PadDescription> ExtendedPadProvider::describePadsLocked()
{
    return akira::platform::extended::DescribePads(m_extended);
}

void ExtendedPadProvider::selectNpad(HidNpadIdType npad)
{
    std::lock_guard<std::mutex> lock(mutex());

    for (const auto& desc : describePadsLocked()) {
        if (desc.npad != npad)
            continue;

        auto path = akira::platform::extended::MakePath(desc, m_extended, m_controllers);
        if (!path)
            break;

        m_bound_npad = npad;

        log::info("InputManager: using {} on npad {}", path->label(), (int)npad);
        setPathLocked(std::move(path));
        return;
    }

    log::warning("InputManager: npad {} no longer present, falling back to default", (int)npad);
    setPathLocked(akira::platform::extended::DefaultPadPath(m_extended, m_controllers));
}

void ExtendedPadProvider::retryPathIdentification()
{
    if (m_identify_done)
        return;

    const uint32_t now = (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000000ull);

    if (m_identify_until == 0) {
        m_identify_until = now + 10000;
        m_identify_next  = now;
    }

    if (akira::platform::extended::PadTakesDirectOutput(m_path->vendorId(), m_path->productId())) {
        m_identify_done = true;
        return;
    }

    if (now > m_identify_until) {
        m_identify_done = true;
        return;
    }

    if (m_arrival_pending || m_couch_claim_active) {
        m_identify_until = now + 10000;
        m_identify_next  = now;
        return;
    }

    if (now < m_identify_next)
        return;
    m_identify_next = now + 1000;

    for (const auto& desc : describePadsLocked()) {
        if (desc.kind != akira::input::PadPathKind::McPsNative)
            continue;
        if (m_couch_roster.find(desc.npad) != m_couch_roster.end())
            continue;
        if (m_bound_npad != kNoNpad && desc.npad != m_bound_npad)
            continue;

        auto path = akira::platform::extended::MakePath(desc, m_extended, m_controllers);
        if (!path)
            continue;

        log::info("InputManager: {} identified on npad {} after the path was chosen"
                           " - upgrading from {}",
                           path->label(), (int)desc.npad, m_path->label());

        m_bound_npad    = desc.npad;
        m_identify_done = true;
        setPathLocked(std::move(path));
        return;
    }
}

void ExtendedPadProvider::reconcilePathDriver()
{
    if (m_bound_npad == kNoNpad)
        return;

    const uint32_t now = (uint32_t)(armTicksToNs(armGetSystemTick()) / 1000000ull);
    if (now < m_driver_next)
        return;
    m_driver_next = now + 1000;

    for (const auto& desc : describePadsLocked()) {
        if (desc.npad != m_bound_npad)
            continue;

        const akira::platform::extended::PadDriver driver =
            akira::platform::extended::ResolvePadDriver(desc, m_extended, m_controllers);
        const bool native_now = m_path->nativeRumble();

        if ((driver == akira::platform::extended::PadDriver::Akira) == native_now)
            return;

        auto path = akira::platform::extended::MakePath(desc, m_extended, m_controllers);
        if (!path)
            return;

        log::info("InputManager: pad on npad {} is now driven by {}"
                           " - {} to {}",
                           (int)desc.npad, akira::platform::extended::PadDriverName(driver),
                           m_path->label(), path->label());
        setPathLocked(std::move(path));
        return;
    }
}

uint8_t ExtendedPadProvider::couchJoin(HidNpadIdType npad)
{
    std::lock_guard<std::mutex> lock(mutex());
    return couchJoinLocked(npad);
}

uint8_t ExtendedPadProvider::couchJoinLocked(HidNpadIdType npad)
{
    if (npad == m_bound_npad)
        return kNoCouchSlot;
    auto existing = m_couch_roster.find(npad);
    if (existing != m_couch_roster.end())
        return existing->second;
    for (uint8_t slot = 1; slot < libchiaking::controllers::SeatNumber::count; slot++)
    {
        bool taken = false;
        for (const auto& entry : m_couch_roster)
        {
            if (entry.second == slot)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            m_couch_roster[npad] = slot;
            return slot;
        }
    }
    return kNoCouchSlot;
}

uint64_t ExtendedPadProvider::couchNpadMask() const
{
    uint64_t mask = 0;
    for (const auto& entry : m_couch_roster)
        mask |= 1UL << (uint32_t)entry.first;
    return mask;
}

void ExtendedPadProvider::couchLeave(HidNpadIdType npad)
{
    std::lock_guard<std::mutex> lock(mutex());
    couchLeaveLocked(npad);
}

void ExtendedPadProvider::couchLeaveLocked(HidNpadIdType npad)
{
    m_couch_roster.erase(npad);
    m_secondary_paths.erase(npad);
    m_secondary_missing_since.erase(npad);
}

uint8_t ExtendedPadProvider::couchPadCount() const
{
    std::lock_guard<std::mutex> lock(mutex());
    return couchPadCountLocked();
}

uint8_t ExtendedPadProvider::couchPadCountLocked() const
{
    uint8_t count = 1;
    for (const auto& entry : m_couch_roster)
    {
        if (entry.second + 1 > count)
            count = (uint8_t)(entry.second + 1);
    }
    return count;
}

uint8_t ExtendedPadProvider::CouchPadIndexForController(HidNpadIdType npad)
{
    std::lock_guard<std::mutex> lock(mutex());
    if (npad == m_bound_npad)
        return 0;
    auto it = m_couch_roster.find(npad);
    if (it != m_couch_roster.end())
        return it->second;
    return kNoCouchSlot;
}

bool ExtendedPadProvider::couchSlotOutputTarget(uint8_t slot, uint8_t out_addr[6], uint16_t* vendor_id,
                                         uint16_t* product_id)
{
    std::lock_guard<std::mutex> lock(mutex());

    if (slot == 0 || slot == kNoCouchSlot)
        return false;

    HidNpadIdType npad = kNoNpad;
    for (const auto& entry : m_couch_roster) {
        if (entry.second == slot) {
            npad = entry.first;
            break;
        }
    }
    if (npad == kNoNpad)
        return false;

    for (const auto& desc : describePadsLocked()) {
        if (desc.npad != npad)
            continue;
        if (!desc.has_address)
            return false;
        if (out_addr)
            std::memcpy(out_addr, desc.bt_addr, 6);
        if (vendor_id)
            *vendor_id = desc.vendor_id;
        if (product_id)
            *product_id = desc.product_id;
        return true;
    }
    return false;
}

void ExtendedPadProvider::readSecondaryPads(play::InputManager& input, std::span<libchiaking::controllers::ControllerInput> states, uint8_t& count)
{
    Callbacks callbacks;
    std::unique_lock<std::mutex> lock(mutex());
    if (m_couch_roster.empty())
    {
        m_secondary_paths.clear();
        m_couch_pads.clear();
        count = couchPadCountLocked();
        return;
    }
    const auto now = akira::platform::extended::timing::Clock::now();
    if (m_couch_pads.empty() || now - m_couch_pads_read >= play::DirectDiscoveryPeriod)
    {
        m_couch_pads = describePadsLocked();
        m_couch_pads_read = now;
    }
    const auto& pads = m_couch_pads;
    std::vector<std::pair<HidNpadIdType, uint8_t>> disconnected;

    for (const auto& entry : m_couch_roster)
    {
        HidNpadIdType npad = entry.first;
        uint8_t slot = entry.second;
        if (slot == 0 || slot >= states.size())
            continue;

        states[slot].set_idle();
        if (slot + 1 > count)
            count = (uint8_t)(slot + 1);

        const akira::input::PadDescription* found = nullptr;
        for (const auto& desc : pads)
        {
            if (desc.npad == npad)
            {
                found = &desc;
                break;
            }
        }
        if (!found)
        {
            auto [missing, firstMiss] = m_secondary_missing_since.try_emplace(npad, now);
            if (firstMiss)
                log::warning("Couch: slot {} npad {} disappeared; sending idle input",
                                      (int)slot, (int)npad);
            if (now - missing->second >= akira::platform::extended::timing::DisconnectGrace)
                disconnected.emplace_back(npad, slot);
            continue;
        }

        m_secondary_missing_since.erase(npad);

        auto& cached = m_secondary_paths[npad];
        if (!cached)
            cached = akira::platform::extended::MakePath(*found, m_extended, m_controllers);
        if (!cached)
            continue;
        cached->poll();
        input.applyPadInput(*cached, states[slot].state());
    }

    for (const auto& entry : disconnected)
    {
        log::warning("Couch: removing disconnected slot {} npad {}",
                              (int)entry.second, (int)entry.first);
        couchLeaveLocked(entry.first);
        if (m_on_couch_disconnected) {
            auto callback = m_on_couch_disconnected;
            callbacks.emplace_back([callback, entry]() {
                callback(entry.first, entry.second);
            });
        }
    }

    count = couchPadCountLocked();

    for (auto it = m_secondary_paths.begin(); it != m_secondary_paths.end(); )
    {
        if (m_couch_roster.find(it->first) == m_couch_roster.end())
            it = m_secondary_paths.erase(it);
        else
            ++it;
    }

    lock.unlock();
    for (auto& callback : callbacks)
        callback();
}

std::map<HidNpadIdType, uint8_t> ExtendedPadProvider::couchRoster() const
{
    std::lock_guard<std::mutex> lock(mutex());
    return m_couch_roster;
}

float ExtendedPadProvider::couchClaimProgress() const
{
    std::lock_guard<std::mutex> lock(mutex());

    if (!m_couch_claim_active)
        return 0.0f;

    const auto now = akira::platform::extended::timing::Clock::now();
    float bestProgress = 0.0f;
    for (const auto& entry : m_claim_started_at)
        bestProgress = std::max(bestProgress, akira::platform::extended::timing::progress(
            entry.second, now, akira::platform::extended::timing::CouchClaimHold));
    return bestProgress;
}

bool ExtendedPadProvider::couchClaimActive() const
{
    std::lock_guard<std::mutex> lock(mutex());
    return m_couch_claim_active;
}

void ExtendedPadProvider::setOnCouchJoined(std::function<void(HidNpadIdType, uint8_t)> callback)
{
    std::lock_guard<std::mutex> lock(mutex());
    m_on_couch_joined = std::move(callback);
}

void ExtendedPadProvider::setOnCouchDisconnected(std::function<void(HidNpadIdType, uint8_t)> callback)
{
    std::lock_guard<std::mutex> lock(mutex());
    m_on_couch_disconnected = std::move(callback);
}

void ExtendedPadProvider::setOnPadArrived(std::function<void(HidNpadIdType)> callback)
{
    std::lock_guard<std::mutex> lock(mutex());
    m_on_pad_arrived = std::move(callback);
}

HidNpadIdType ExtendedPadProvider::boundNpad() const
{
    std::lock_guard<std::mutex> lock(mutex());
    return m_bound_npad;
}

void ExtendedPadProvider::resolvePadArrival()
{
    std::lock_guard<std::mutex> lock(mutex());
    m_arrival_pending = false;
}

void ExtendedPadProvider::beginCouchJoin()
{
    std::lock_guard<std::mutex> lock(mutex());
    m_couch_claim_active = true;
    m_claim_started_at.clear();
}

void ExtendedPadProvider::cancelCouchJoin()
{
    std::lock_guard<std::mutex> lock(mutex());
    m_couch_claim_active = false;
    m_arrival_pending = false;
    m_claim_started_at.clear();
}

void ExtendedPadProvider::tickCouchClaim()
{
    Callbacks callbacks;
    std::unique_lock<std::mutex> lock(mutex());
    pollCouchClaim(callbacks);
    lock.unlock();
    for (auto& callback : callbacks)
        callback();
}

void ExtendedPadProvider::promptControllerSetup()
{
    HidLaControllerSupportArg arg;
    hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_max = libchiaking::controllers::SeatNumber::count;
    arg.hdr.enable_single_mode = false;
    HidLaControllerSupportResultInfo result;
    hidLaShowControllerSupport(&result, &arg);
}

bool ExtendedPadProvider::isClaimableNpad(HidNpadIdType npad) const
{
    std::lock_guard<std::mutex> lock(mutex());
    return isClaimableNpadLocked(npad);
}

bool ExtendedPadProvider::isClaimableNpadLocked(HidNpadIdType npad) const
{
    if (npad == m_bound_npad)
        return false;
    if ((uint32_t)npad >= 8)
        return false;
    return true;
}

void ExtendedPadProvider::pollPadArrivals(Callbacks& callbacks)
{
    const auto now = akira::platform::extended::timing::Clock::now();
    if (!m_arrival_cadence.due(now))
        return;

    static constexpr HidNpadIdType kArrivalScan[] = {
        HidNpadIdType_No1,
        HidNpadIdType_No2,
        HidNpadIdType_No3,
        HidNpadIdType_No4,
    };

    uint32_t mask = 0;
    for (HidNpadIdType npad : kArrivalScan)
    {
        PadState probe{};
        padInitializeWithMask(&probe, 1UL << (uint32_t)npad);
        padUpdate(&probe);
        if (padIsConnected(&probe))
            mask |= 1UL << (uint32_t)npad;
    }

    const uint32_t previous = m_connected_mask;
    m_connected_mask = mask;
    m_announced_mask &= mask;

    if (!m_arrival_primed)
    {
        m_arrival_primed = true;
        m_announced_mask = mask;
        return;
    }

    if (!m_on_pad_arrived)
        return;

    const uint32_t arrived = mask & ~previous & ~m_announced_mask;
    if (!arrived)
        return;

    for (HidNpadIdType npad : kArrivalScan)
    {
        const uint32_t bit = 1UL << (uint32_t)npad;
        if (!(arrived & bit))
            continue;
        if (npad == m_bound_npad)
            continue;
        if (m_couch_roster.find(npad) != m_couch_roster.end())
            continue;

        m_announced_mask |= bit;
        m_arrival_pending = true;
        log::info("Couch: controller appeared on npad {}", (int)npad);
        auto callback = m_on_pad_arrived;
        callbacks.emplace_back([callback, npad]() { callback(npad); });
        return;
    }
}

void ExtendedPadProvider::pollCouchClaim(Callbacks& callbacks)
{
    if (!m_couch_claim_active)
        return;

    auto pads = describePadsLocked();
    const auto now = akira::platform::extended::timing::Clock::now();
    for (auto it = m_claim_started_at.begin(); it != m_claim_started_at.end();) {
        const bool present = std::any_of(pads.begin(), pads.end(), [it](const auto& pad) {
            return pad.npad == it->first;
        });
        if (!present)
            it = m_claim_started_at.erase(it);
        else
            ++it;
    }
    for (const auto& desc : pads)
    {
        HidNpadIdType npad = desc.npad;
        if (!isClaimableNpadLocked(npad))
            continue;
        if (m_couch_roster.find(npad) != m_couch_roster.end())
            continue;

        PadState pad;
        padInitializeWithMask(&pad, 1UL << (uint32_t)npad);
        padUpdate(&pad);
        u64 btns = padGetButtons(&pad);
        bool combo = (btns & HidNpadButton_ZL) && (btns & HidNpadButton_ZR);

        if (combo)
        {
            auto [started, firstSample] = m_claim_started_at.try_emplace(npad, now);
            if (!firstSample && now - started->second >= akira::platform::extended::timing::CouchClaimHold)
            {
                uint8_t slot = couchJoinLocked(npad);
                m_claim_started_at.clear();
                m_couch_claim_active = false;
                if (slot != kNoCouchSlot && m_on_couch_joined) {
                    auto callback = m_on_couch_joined;
                    callbacks.emplace_back([callback, npad, slot]() { callback(npad, slot); });
                }
                return;
            }
        }
        else
        {
            m_claim_started_at.erase(npad);
        }
    }
}

}
