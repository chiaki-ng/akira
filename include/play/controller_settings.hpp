#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "play/rumble_profile.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::play {

enum class HapticPreset {
    Disabled = 0,
    Weak = 1,
    Strong = 2,

    ConsoleRumble = 3
};

enum class GyroSource {
    Auto = 0,
    Left = 1,
    Right = 2
};

using ButtonMapping = std::map<uint32_t, std::vector<uint64_t>>;

class ControllerSettings final : public settings::Section {
public:
    explicit ControllerSettings(settings::SettingsStore& store);
    ~ControllerSettings() override;

    ControllerSettings(const ControllerSettings&) = delete;
    ControllerSettings& operator=(const ControllerSettings&) = delete;

    void write(toml::table& config) const override;

    HapticPreset hapticPreset() const { return haptic; }

    GyroSource gyroSource() const { return gyro; }
    void setGyroSource(GyroSource source) { gyro = source; }

    RumbleProfile rumbleProfile(const std::string& key) const;
    void setRumbleProfile(const std::string& key, const RumbleProfile& profile);
    bool hasRumbleProfile(const std::string& key) const;
    bool seedRumbleProfile(const std::string& key, const RumbleProfile* inheritFrom = nullptr);
    void resetRumbleProfile(const std::string& key);

    const ButtonMapping& buttonMapping() const { return mapping; }
    void setButtonMapping(const ButtonMapping& value) { mapping = value; }
    static ButtonMapping defaultButtonMapping();

    void setTouchpadEnabled(bool enabled) { touchpad = enabled; }
    void setSwipeUpEnabled(bool enabled) { swipeUp = enabled; }
    void setSwipeDownEnabled(bool enabled) { swipeDown = enabled; }
    void setSwipeLeftEnabled(bool enabled) { swipeLeft = enabled; }
    void setSwipeRightEnabled(bool enabled) { swipeRight = enabled; }
    bool isButtonEnabled(uint32_t chiakiButton) const;

private:
    static RumbleProfile defaultRumbleProfile(const std::string& key);
    static RumbleSource sourceFromLegacyPreset(HapticPreset preset);
    static RumbleProfile legacySwitchProfile(const toml::table* rumble);
    static std::string hidButtonName(uint64_t button);
    static uint64_t hidButtonOf(const std::string& name);
    static std::string chiakiButtonKey(uint32_t button);
    static uint32_t chiakiButtonOf(const std::string& key);

    void read(const toml::table& config);
    void readRumble(const toml::table& rumble);
    void readButtonMapping(const toml::table& table);

    settings::SettingsStore& store;

    HapticPreset haptic = HapticPreset::Disabled;
    GyroSource gyro = GyroSource::Auto;
    std::map<std::string, RumbleProfile> rumbleProfiles;
    ButtonMapping mapping = defaultButtonMapping();
    bool touchpad = true;
    bool swipeUp = true;
    bool swipeDown = true;
    bool swipeLeft = true;
    bool swipeRight = true;
};

}
