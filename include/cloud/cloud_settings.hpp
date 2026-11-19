#pragma once

#include <string>

#include <libchiaking/platform/platform.hpp>

#include "cloud/saved_datacenters.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::cloud {

class CloudSettings final : public settings::Section {
public:
    explicit CloudSettings(settings::SettingsStore& store);
    ~CloudSettings() override;

    CloudSettings(const CloudSettings&) = delete;
    CloudSettings& operator=(const CloudSettings&) = delete;

    void write(toml::table& config) const override;

    int videoBitrate(bool pscloud) const { return service(pscloud).bitrate; }
    void setVideoBitrate(bool pscloud, int value) { service(pscloud).bitrate = value; }
    int videoResolution(bool pscloud) const { return service(pscloud).resolution; }
    void setVideoResolution(bool pscloud, int value) { service(pscloud).resolution = value; }
    bool fsrEnabled(bool pscloud) const { return service(pscloud).fsr; }
    void setFsrEnabled(bool pscloud, bool enabled) { service(pscloud).fsr = enabled; }

    int sortState() const { return sort; }
    void setSortState(int value) { sort = value; }
    const std::string& filter() const { return filterText; }
    void setFilter(std::string value) { filterText = std::move(value); }

    const std::string& duid() const { return cloudDuid; }
    void ensureDuid(libchiaking::platform::SecureRandomProvider& random);

    SavedDatacenters& datacenters() { return saved; }

private:
    struct Service {
        int bitrate = 10000;
        int resolution = 1080;
        bool fsr = false;
    };

    static bool isValidDuid(const std::string& duid);

    void read(const toml::table& config);
    Service& service(bool pscloud) { return pscloud ? pscloudVideo : psnowVideo; }
    const Service& service(bool pscloud) const { return pscloud ? pscloudVideo : psnowVideo; }

    settings::SettingsStore& store;
    Service pscloudVideo;
    Service psnowVideo;
    int sort = 0;
    std::string filterText;
    std::string cloudDuid;
    SavedDatacenters saved;
};

}
