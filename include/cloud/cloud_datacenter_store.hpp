#pragma once

#include <span>
#include <vector>

#include <libchiaking/play_types.hpp>

namespace akira::settings {
class SettingsStore;
}

namespace akira::cloud {

class SavedDatacenters;

class CloudDatacenterStore final : public libchiaking::play::DatacenterStore {
public:
    using Measurement = libchiaking::play::DatacenterMeasurement;
    using ServiceGeneration = libchiaking::catalog::ServiceGeneration;

    CloudDatacenterStore(SavedDatacenters& saved, settings::SettingsStore& store);

    [[nodiscard]] auto load(ServiceGeneration service)
        -> libchiaking::Expected<std::vector<Measurement>, Error> override;
    [[nodiscard]] auto save(ServiceGeneration service, std::span<const Measurement> measurements)
        -> libchiaking::Expected<void, Error> override;

private:
    static auto pscloud(ServiceGeneration service) noexcept -> bool;

    SavedDatacenters& saved;
    settings::SettingsStore& store;
};

}
