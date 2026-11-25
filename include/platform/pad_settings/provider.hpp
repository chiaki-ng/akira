#pragma once

#include <functional>
#include <optional>
#include <string>

#include "play/rumble_profile.hpp"

namespace akira::play {
class ControllerSettings;
}

namespace akira::platform
{

class PadSettingsProvider
{
public:
    struct DirectPreview {
        std::function<void()> stop;
    };

    virtual ~PadSettingsProvider() = default;

    virtual std::string rumbleKey(play::ControllerSettings& settings) = 0;
    virtual std::string currentKey(const play::ControllerSettings& settings) const = 0;
    virtual std::optional<play::PadOutputMode> outputMode(const play::ControllerSettings& settings) const = 0;
    virtual void useProfile(const play::RumbleProfile& profile) = 0;
    virtual bool isUnitKey(const std::string& key) const = 0;
    virtual bool directOutput() const = 0;
    virtual bool tunesMotors() const = 0;

    virtual bool outputReleased() const = 0;
    virtual bool drivesLightbar() const = 0;
    virtual void refreshOutputGates() = 0;
    virtual std::optional<DirectPreview> previewDirect(const play::RumbleProfile& profile) = 0;
};

}
