#pragma once

#include <functional>
#include <optional>
#include <string>

#include "input/rumble_profile.hpp"

namespace brls
{
class Box;
}

class SettingsManager;

namespace akira::platform
{

class PadSettings
{
public:
    using Action = std::function<void()>;

    virtual ~PadSettings() = default;

    virtual std::string rumbleKey(SettingsManager& settings) = 0;
    virtual std::string currentKey(const SettingsManager& settings) const = 0;
    virtual std::optional<std::string> outputSummary(const SettingsManager& settings) const = 0;
    virtual void useProfile(const input::RumbleProfile& profile) = 0;
    virtual bool isUnitKey(const std::string& key) const = 0;
    virtual bool directOutput() const = 0;
    virtual bool tunesMotors() const = 0;

    virtual void addOutputRows(brls::Box& content, input::RumbleProfile& profile,
                               const Action& store, const Action& reopen) = 0;
    virtual void addLightbarRows(brls::Box& content, input::RumbleProfile& profile,
                                 const Action& store, const Action& reopen) = 0;
    virtual bool previewDirect(const input::RumbleProfile& profile) = 0;
};

}
