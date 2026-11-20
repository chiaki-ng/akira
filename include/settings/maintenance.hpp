#pragma once

#include <functional>
#include <string>

namespace akira::settings {

class Maintenance {
public:
    struct UpdateCheck {
        enum class Outcome { Failed, UpToDate, Available };

        Outcome outcome = Outcome::Failed;
        std::string version;

        friend bool operator==(const UpdateCheck&, const UpdateCheck&) = default;
    };

    struct UpdateProgress {
        enum class Stage { Downloading, Verifying, Installed, Failed };

        Stage stage = Stage::Downloading;
        int percent = 0;
        std::string error;

        friend bool operator==(const UpdateProgress&, const UpdateProgress&) = default;
    };

    virtual ~Maintenance() = default;

    virtual auto legacyProfile() const -> bool = 0;
    virtual auto localSubnet() const -> std::string = 0;
    virtual void restartDiscovery() = 0;
    virtual void flushTrophyCache() = 0;
    virtual void accountNameDisplayChanged() = 0;
    virtual void consolesChanged() = 0;
    virtual void probeTls(std::function<void(std::string)> report) = 0;
    virtual void checkForUpdate(std::string channel, std::function<void(UpdateCheck)> done) = 0;
    virtual void simulateUpdate(std::string server, std::function<void(UpdateCheck)> done) = 0;
    virtual void installUpdate(std::function<void(UpdateProgress)> progress) = 0;
    virtual void cancelUpdate() = 0;
    virtual void quit() = 0;
};

}
