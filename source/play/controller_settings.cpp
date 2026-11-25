#include "play/controller_settings.hpp"
#include "play/remap_target.hpp"
#include "play/swipe_direction.hpp"
#include "play/switch_button.hpp"
#include "settings/settings_store.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace akira::play {

ControllerSettings::ControllerSettings(settings::SettingsStore& store)
    : store(store)
{
    read(store.loaded());
    store.attach(*this);
}

ControllerSettings::~ControllerSettings()
{
    store.detach(*this);
}

void ControllerSettings::read(const toml::table& config)
{
    const auto input = config["input"];
    if (auto val = input["haptic"].value<int64_t>())
        haptic = static_cast<HapticPreset>(*val);
    if (auto val = input["gyro_source"].value<int64_t>())
        gyro = static_cast<GyroSource>(*val);

    const auto* rumble = input["rumble"].as_table();
    if (rumble)
        readRumble(*rumble);

    if (!config.empty()) {
        if (rumbleProfiles.find(kRumbleKeySwitch) == rumbleProfiles.end()) {
            rumbleProfiles[kRumbleKeySwitch] = legacySwitchProfile(rumble);
            log::info("rumble profiles: migrated existing settings into the switch profile");
        }

        const auto oldSwitch = rumbleProfiles.find(kRumbleKeySwitch);
        const RumbleProfile inherited =
            oldSwitch != rumbleProfiles.end() ? oldSwitch->second : SwitchRumbleProfile();
        for (const char* key : {kRumbleKeyJoyCon, kRumbleKeySwitchPro}) {
            if (rumbleProfiles.find(key) == rumbleProfiles.end())
                rumbleProfiles[key] = inherited;
        }

        if (rumbleProfiles.find(kRumbleKeyDualSense) == rumbleProfiles.end()) {
            RumbleProfile dualSense = DefaultRumbleProfile();
            dualSense.output_mode = PadOutputMode::Native;
            dualSense.rumble_source = sourceFromLegacyPreset(haptic);
            rumbleProfiles[kRumbleKeyDualSense] = dualSense;
        }
        if (rumbleProfiles.find(kRumbleKeyGeneric) == rumbleProfiles.end())
            rumbleProfiles[kRumbleKeyGeneric] = DefaultRumbleProfile();
        if (rumbleProfiles.find(kRumbleKeyDefault) == rumbleProfiles.end())
            rumbleProfiles[kRumbleKeyDefault] = DefaultRumbleProfile();
    }

    if (const auto* table = input["button_mapping"].as_table())
        readButtonMapping(*table);
}

void ControllerSettings::readRumble(const toml::table& rumble)
{
    for (const auto& [key, node] : rumble) {
        const auto* entry = node.as_table();
        if (entry == nullptr)
            continue;

        std::string name(key.str());
        RumbleProfile profile = defaultRumbleProfile(name);

        if (auto v = (*entry)["strength"].value<double>())
            profile.strength = std::max(0.0f, std::min(1.0f, static_cast<float>(*v)));
        if (auto v = (*entry)["ceiling"].value<double>())
            profile.ceiling = std::max(0.0f, std::min(1.0f, static_cast<float>(*v)));
        if (auto v = (*entry)["per_motor"].value<bool>())
            profile.per_motor = *v;
        if (auto v = (*entry)["freq_low"].value<double>())
            profile.freq_low = std::max(40.0f, std::min(320.0f, static_cast<float>(*v)));
        if (auto v = (*entry)["freq_high"].value<double>())
            profile.freq_high = std::max(40.0f, std::min(320.0f, static_cast<float>(*v)));
        if (auto v = (*entry)["envelope_attack"].value<double>())
            profile.envelope_attack = std::max(0.20f, std::min(1.00f, static_cast<float>(*v)));
        if (auto v = (*entry)["envelope_decay"].value<double>())
            profile.envelope_decay = std::max(0.50f, std::min(0.95f, static_cast<float>(*v)));
        if (auto v = (*entry)["haptic_intensity"].value<int64_t>()) {
            const int64_t clamped = std::max<int64_t>(0, std::min<int64_t>(5, *v));
            profile.haptic_intensity = static_cast<HapticIntensity>(clamped);
        }
        if (auto v = (*entry)["direct_output"].value<bool>())
            profile.output_mode = *v ? PadOutputMode::Native : PadOutputMode::Basic;
        if (auto v = (*entry)["output_mode"].value<int64_t>())
            profile.output_mode = (*v == 1) ? PadOutputMode::Basic : PadOutputMode::Native;

        if (auto v = (*entry)["lightbar_enabled"].value<bool>())
            profile.lightbar_enabled = *v;
        if (auto v = (*entry)["lightbar_r"].value<int64_t>())
            profile.lightbar_r = static_cast<std::uint8_t>(std::clamp<int64_t>(*v, 0, 255));
        if (auto v = (*entry)["lightbar_g"].value<int64_t>())
            profile.lightbar_g = static_cast<std::uint8_t>(std::clamp<int64_t>(*v, 0, 255));
        if (auto v = (*entry)["lightbar_b"].value<int64_t>())
            profile.lightbar_b = static_cast<std::uint8_t>(std::clamp<int64_t>(*v, 0, 255));

        auto source = (*entry)["rumble_source"].value<int64_t>();
        if (!source)
            source = (*entry)["console_feedback"].value<int64_t>();
        if (source) {
            const int64_t clamped = std::max<int64_t>(0, std::min<int64_t>(2, *source));
            profile.rumble_source = static_cast<RumbleSource>(clamped);
        } else {
            profile.rumble_source = sourceFromLegacyPreset(haptic);
            if (haptic == HapticPreset::Weak)
                profile.haptic_intensity = HapticIntensity::Weak;
        }

        rumbleProfiles[name] = profile;
    }
}

RumbleProfile ControllerSettings::legacySwitchProfile(const toml::table* rumble)
{
    RumbleProfile migrated = SwitchRumbleProfile();
    migrated.freq_low = 140.0f;
    migrated.freq_high = 185.0f;
    migrated.envelope_attack = 0.60f;
    migrated.envelope_decay = 0.85f;
    if (rumble == nullptr)
        return migrated;
    if (auto val = (*rumble)["freq_low"].value<double>())
        migrated.freq_low = std::max(40.0f, std::min(320.0f, static_cast<float>(*val)));
    if (auto val = (*rumble)["freq_high"].value<double>())
        migrated.freq_high = std::max(40.0f, std::min(320.0f, static_cast<float>(*val)));
    if (auto val = (*rumble)["envelope_decay"].value<double>())
        migrated.envelope_decay = std::max(0.50f, std::min(0.95f, static_cast<float>(*val)));
    if (auto val = (*rumble)["envelope_attack"].value<double>())
        migrated.envelope_attack = std::max(0.20f, std::min(1.00f, static_cast<float>(*val)));
    return migrated;
}

void ControllerSettings::readButtonMapping(const toml::table& table)
{
    for (const auto& [key, value] : table) {
        const uint32_t chiakiButton = chiakiButtonOf(std::string(key.str()));
        if (chiakiButton == 0)
            continue;

        if (const auto* buttons = value.as_array()) {
            std::vector<uint64_t> combo;
            for (const auto& element : *buttons) {
                if (auto name = element.value<std::string>()) {
                    if (const uint64_t hidButton = hidButtonOf(*name); hidButton != 0)
                        combo.push_back(hidButton);
                }
            }
            mapping[chiakiButton] = combo;
        }
    }
    if (auto val = table["touchpad_enabled"].value<bool>())
        touchpad = *val;
    if (auto val = table["swipe_up_enabled"].value<bool>())
        swipeUp = *val;
    if (auto val = table["swipe_down_enabled"].value<bool>())
        swipeDown = *val;
    if (auto val = table["swipe_left_enabled"].value<bool>())
        swipeLeft = *val;
    if (auto val = table["swipe_right_enabled"].value<bool>())
        swipeRight = *val;

    log::info("Loaded button mapping from config");
}

void ControllerSettings::write(toml::table& config) const
{
    toml::table input;
    input.insert("haptic", std::to_underlying(haptic));
    input.insert("gyro_source", std::to_underlying(gyro));

    toml::table rumble;
    for (const auto& [key, profile] : rumbleProfiles) {
        toml::table entry;
        entry.insert("strength", static_cast<double>(profile.strength));
        entry.insert("ceiling", static_cast<double>(profile.ceiling));
        entry.insert("per_motor", profile.per_motor);

        if (!profile.per_motor) {
            entry.insert("freq_low", static_cast<double>(profile.freq_low));
            entry.insert("freq_high", static_cast<double>(profile.freq_high));
            entry.insert("envelope_attack", static_cast<double>(profile.envelope_attack));
            entry.insert("envelope_decay", static_cast<double>(profile.envelope_decay));
        }

        entry.insert("haptic_intensity", static_cast<int64_t>(std::to_underlying(profile.haptic_intensity)));
        entry.insert("output_mode", static_cast<int64_t>(std::to_underlying(profile.output_mode)));
        entry.insert("lightbar_enabled", profile.lightbar_enabled);
        entry.insert("lightbar_r", static_cast<int64_t>(profile.lightbar_r));
        entry.insert("lightbar_g", static_cast<int64_t>(profile.lightbar_g));
        entry.insert("lightbar_b", static_cast<int64_t>(profile.lightbar_b));
        entry.insert("rumble_source", static_cast<int64_t>(std::to_underlying(profile.rumble_source)));

        rumble.insert(key, std::move(entry));
    }
    input.insert("rumble", std::move(rumble));

    toml::table buttons;
    if (mapping != defaultButtonMapping()) {
        for (const auto& [chiakiButton, combo] : mapping) {
            const std::string key = chiakiButtonKey(chiakiButton);
            if (key.empty())
                continue;

            toml::array names;
            for (const uint64_t hidButton : combo) {
                if (std::string name = hidButtonName(hidButton); !name.empty())
                    names.push_back(std::move(name));
            }
            buttons.insert(key, std::move(names));
        }
    }
    buttons.insert("touchpad_enabled", touchpad);
    buttons.insert("swipe_up_enabled", swipeUp);
    buttons.insert("swipe_down_enabled", swipeDown);
    buttons.insert("swipe_left_enabled", swipeLeft);
    buttons.insert("swipe_right_enabled", swipeRight);
    input.insert("button_mapping", std::move(buttons));

    config.insert("input", std::move(input));
}

RumbleProfile ControllerSettings::rumbleProfile(const std::string& key) const
{
    const auto it = rumbleProfiles.find(key);
    if (it != rumbleProfiles.end())
        return it->second;
    return defaultRumbleProfile(key);
}

bool ControllerSettings::hasRumbleProfile(const std::string& key) const
{
    return rumbleProfiles.find(key) != rumbleProfiles.end();
}

void ControllerSettings::setRumbleProfile(const std::string& key, const RumbleProfile& profile)
{
    if (key.empty())
        return;
    rumbleProfiles[key] = profile;
}

bool ControllerSettings::seedRumbleProfile(const std::string& key, const RumbleProfile* inheritFrom)
{
    if (key.empty() || hasRumbleProfile(key))
        return false;
    rumbleProfiles[key] = inheritFrom != nullptr ? *inheritFrom : defaultRumbleProfile(key);
    return true;
}

void ControllerSettings::resetRumbleProfile(const std::string& key)
{
    if (key.empty())
        return;
    rumbleProfiles[key] = defaultRumbleProfile(key);
}

bool ControllerSettings::isButtonEnabled(uint32_t chiakiButton) const
{
    switch (chiakiButton) {
        case remapKey(RemapTarget::Touchpad): return touchpad;
        case SWIPE_TOUCHPAD_UP: return swipeUp;
        case SWIPE_TOUCHPAD_DOWN: return swipeDown;
        case SWIPE_TOUCHPAD_LEFT: return swipeLeft;
        case SWIPE_TOUCHPAD_RIGHT: return swipeRight;
        default: return true;
    }
}

ButtonMapping ControllerSettings::defaultButtonMapping()
{
    ButtonMapping defaults;
    defaults[remapKey(RemapTarget::Cross)] = {bits(SwitchButton::B)};
    defaults[remapKey(RemapTarget::Moon)] = {bits(SwitchButton::A)};
    defaults[remapKey(RemapTarget::Box)] = {bits(SwitchButton::Y)};
    defaults[remapKey(RemapTarget::Pyramid)] = {bits(SwitchButton::X)};
    defaults[remapKey(RemapTarget::L1)] = {bits(SwitchButton::L)};
    defaults[remapKey(RemapTarget::R1)] = {bits(SwitchButton::R)};
    defaults[remapKey(RemapTarget::L2)] = {bits(SwitchButton::ZL)};
    defaults[remapKey(RemapTarget::R2)] = {bits(SwitchButton::ZR)};
    defaults[remapKey(RemapTarget::L3)] = {bits(SwitchButton::StickL)};
    defaults[remapKey(RemapTarget::R3)] = {bits(SwitchButton::StickR)};
    defaults[remapKey(RemapTarget::Options)] = {bits(SwitchButton::Plus)};
    defaults[remapKey(RemapTarget::Share)] = {};
    defaults[remapKey(RemapTarget::Touchpad)] = {};
    defaults[remapKey(RemapTarget::Ps)] = {bits(SwitchButton::Minus)};
    defaults[SWIPE_TOUCHPAD_UP] = {};
    defaults[SWIPE_TOUCHPAD_DOWN] = {};
    defaults[SWIPE_TOUCHPAD_LEFT] = {};
    defaults[SWIPE_TOUCHPAD_RIGHT] = {};
    return defaults;
}

RumbleProfile ControllerSettings::defaultRumbleProfile(const std::string& key)
{
    return key == kRumbleKeySwitch ? SwitchRumbleProfile() : DefaultRumbleProfile();
}

RumbleSource ControllerSettings::sourceFromLegacyPreset(HapticPreset preset)
{
    switch (preset) {
        case HapticPreset::Disabled:      return RumbleSource::Off;
        case HapticPreset::ConsoleRumble: return RumbleSource::Game;
        default:                          return RumbleSource::Derived;
    }
}

std::string ControllerSettings::hidButtonName(uint64_t button)
{
    switch (static_cast<SwitchButton>(button)) {
        case SwitchButton::A: return "A";
        case SwitchButton::B: return "B";
        case SwitchButton::X: return "X";
        case SwitchButton::Y: return "Y";
        case SwitchButton::L: return "L";
        case SwitchButton::R: return "R";
        case SwitchButton::ZL: return "ZL";
        case SwitchButton::ZR: return "ZR";
        case SwitchButton::Plus: return "Plus";
        case SwitchButton::Minus: return "Minus";
        case SwitchButton::StickL: return "L Stick";
        case SwitchButton::StickR: return "R Stick";
        case SwitchButton::LeftSL: return "SL(L)";
        case SwitchButton::LeftSR: return "SR(L)";
        case SwitchButton::RightSL: return "SL(R)";
        case SwitchButton::RightSR: return "SR(R)";
        case SwitchButton::Up: return "D-Up";
        case SwitchButton::Down: return "D-Down";
        case SwitchButton::Left: return "D-Left";
        case SwitchButton::Right: return "D-Right";
        case SwitchButton::StickLUp: return "LS-Up";
        case SwitchButton::StickLDown: return "LS-Down";
        case SwitchButton::StickLLeft: return "LS-Left";
        case SwitchButton::StickLRight: return "LS-Right";
        case SwitchButton::StickRUp: return "RS-Up";
        case SwitchButton::StickRDown: return "RS-Down";
        case SwitchButton::StickRLeft: return "RS-Left";
        case SwitchButton::StickRRight: return "RS-Right";
        default: return "";
    }
}

uint64_t ControllerSettings::hidButtonOf(const std::string& name)
{
    static constexpr std::array<std::pair<std::string_view, SwitchButton>, 28> names{{
        {"A", SwitchButton::A},
        {"B", SwitchButton::B},
        {"X", SwitchButton::X},
        {"Y", SwitchButton::Y},
        {"L", SwitchButton::L},
        {"R", SwitchButton::R},
        {"ZL", SwitchButton::ZL},
        {"ZR", SwitchButton::ZR},
        {"Plus", SwitchButton::Plus},
        {"Minus", SwitchButton::Minus},
        {"L Stick", SwitchButton::StickL},
        {"R Stick", SwitchButton::StickR},
        {"SL(L)", SwitchButton::LeftSL},
        {"SR(L)", SwitchButton::LeftSR},
        {"SL(R)", SwitchButton::RightSL},
        {"SR(R)", SwitchButton::RightSR},
        {"D-Up", SwitchButton::Up},
        {"D-Down", SwitchButton::Down},
        {"D-Left", SwitchButton::Left},
        {"D-Right", SwitchButton::Right},
        {"LS-Up", SwitchButton::StickLUp},
        {"LS-Down", SwitchButton::StickLDown},
        {"LS-Left", SwitchButton::StickLLeft},
        {"LS-Right", SwitchButton::StickLRight},
        {"RS-Up", SwitchButton::StickRUp},
        {"RS-Down", SwitchButton::StickRDown},
        {"RS-Left", SwitchButton::StickRLeft},
        {"RS-Right", SwitchButton::StickRRight},
    }};
    const auto found = std::ranges::find(names, name, &std::pair<std::string_view, SwitchButton>::first);
    return found == names.end() ? 0 : bits(found->second);
}

std::string ControllerSettings::chiakiButtonKey(uint32_t button)
{
    switch (button) {
        case remapKey(RemapTarget::Cross): return "cross";
        case remapKey(RemapTarget::Moon): return "circle";
        case remapKey(RemapTarget::Box): return "square";
        case remapKey(RemapTarget::Pyramid): return "triangle";
        case remapKey(RemapTarget::L1): return "l1";
        case remapKey(RemapTarget::R1): return "r1";
        case remapKey(RemapTarget::L2): return "l2";
        case remapKey(RemapTarget::R2): return "r2";
        case remapKey(RemapTarget::L3): return "l3";
        case remapKey(RemapTarget::R3): return "r3";
        case remapKey(RemapTarget::Options): return "options";
        case remapKey(RemapTarget::Share): return "share";
        case remapKey(RemapTarget::Touchpad): return "touchpad";
        case remapKey(RemapTarget::Ps): return "ps";
        case SWIPE_TOUCHPAD_UP: return "swipe_up";
        case SWIPE_TOUCHPAD_DOWN: return "swipe_down";
        case SWIPE_TOUCHPAD_LEFT: return "swipe_left";
        case SWIPE_TOUCHPAD_RIGHT: return "swipe_right";
        default: return "";
    }
}

uint32_t ControllerSettings::chiakiButtonOf(const std::string& key)
{
    static constexpr std::array<std::pair<std::string_view, uint32_t>, 18> keys{{
        {"cross", remapKey(RemapTarget::Cross)},
        {"circle", remapKey(RemapTarget::Moon)},
        {"square", remapKey(RemapTarget::Box)},
        {"triangle", remapKey(RemapTarget::Pyramid)},
        {"l1", remapKey(RemapTarget::L1)},
        {"r1", remapKey(RemapTarget::R1)},
        {"l2", remapKey(RemapTarget::L2)},
        {"r2", remapKey(RemapTarget::R2)},
        {"l3", remapKey(RemapTarget::L3)},
        {"r3", remapKey(RemapTarget::R3)},
        {"options", remapKey(RemapTarget::Options)},
        {"share", remapKey(RemapTarget::Share)},
        {"touchpad", remapKey(RemapTarget::Touchpad)},
        {"ps", remapKey(RemapTarget::Ps)},
        {"swipe_up", SWIPE_TOUCHPAD_UP},
        {"swipe_down", SWIPE_TOUCHPAD_DOWN},
        {"swipe_left", SWIPE_TOUCHPAD_LEFT},
        {"swipe_right", SWIPE_TOUCHPAD_RIGHT},
    }};
    const auto found = std::ranges::find(keys, key, &std::pair<std::string_view, uint32_t>::first);
    return found == keys.end() ? 0 : found->second;
}

}
