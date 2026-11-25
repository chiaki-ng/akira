#pragma once

#include <mutex>
#include <vector>

#include "cloud/models.hpp"
#include "settings/section.hpp"

namespace akira::settings {
class SettingsStore;
}

namespace akira::cloud {

class SavedDatacenters final : public settings::Section {
public:
    explicit SavedDatacenters(settings::SettingsStore& store);
    ~SavedDatacenters() override;

    SavedDatacenters(const SavedDatacenters&) = delete;
    SavedDatacenters& operator=(const SavedDatacenters&) = delete;

    void write(toml::table& config) const override;

    std::vector<Datacenter> datacenters(bool pscloud) const;
    void setDatacenters(bool pscloud, std::vector<Datacenter> datacenters);

private:
    static std::vector<Datacenter> readList(const toml::array* rows);
    static toml::array rowsOf(const std::vector<Datacenter>& datacenters);

    settings::SettingsStore& store;
    mutable std::mutex mutex;
    std::vector<Datacenter> pscloudDatacenters;
    std::vector<Datacenter> psnowDatacenters;
};

}
