#pragma once

#include <functional>
#include <optional>
#include <string>

#include "play/rumble_profile.hpp"

namespace brls
{
class Box;
}

namespace akira::settings {
class SettingsManager;
}

namespace akira::platform
{

class PadSettingsProvider
{
public:
    using Action = std::function<void()>;

    virtual ~PadSettingsProvider() = default;

    virtual std::string rumbleKey(settings::SettingsManager& settings) = 0;
    virtual std::string currentKey(const settings::SettingsManager& settings) const = 0;
    virtual std::optional<std::string> outputSummary(const settings::SettingsManager& settings) const = 0;
    virtual void useProfile(const play::RumbleProfile& profile) = 0;
    virtual bool isUnitKey(const std::string& key) const = 0;
    virtual bool directOutput() const = 0;
    virtual bool tunesMotors() const = 0;

    virtual void addOutputRows(brls::Box& content, play::RumbleProfile& profile,
                               const Action& store, const Action& reopen) = 0;
    virtual void addLightbarRows(brls::Box& content, play::RumbleProfile& profile,
                                 const Action& store, const Action& reopen) = 0;
    virtual bool previewDirect(const play::RumbleProfile& profile) = 0;
};

}
