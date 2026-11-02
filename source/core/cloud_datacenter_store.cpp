#include "core/cloud_datacenter_store.hpp"
#include "core/settings_manager.hpp"

#include <chrono>
#include <utility>

namespace akira {

using libchiaking::unexpected;
using libchiaking::play::DatacenterName;

CloudDatacenterStore::CloudDatacenterStore(SettingsManager& settings)
    : settings(settings)
{
}

auto CloudDatacenterStore::load(ServiceGeneration service)
    -> libchiaking::Expected<std::vector<Measurement>, Error>
{
    std::vector<Measurement> measurements;
    for (const cloud::Datacenter& datacenter : settings.getCloudDatacenters(pscloud(service))) {
        auto name = DatacenterName::parse(datacenter.name);
        if (!name || !datacenter.measured)
            continue;

        Measurement::Probe probe;
        for (const int sample : datacenter.rttSamples)
            probe.samples.emplace_back(sample);
        probe.mtu_in = datacenter.mtuIn;
        probe.mtu_out = datacenter.mtuOut;
        probe.port = datacenter.port;
        probe.public_address = datacenter.publicIp;
        probe.max_bandwidth = datacenter.maxBandwidth;

        const Measurement::Time measuredAt{std::chrono::milliseconds{datacenter.measuredAtMs}};
        if (auto measurement = Measurement::create(std::move(*name), Measurement::RoundTrip{datacenter.rttMs},
                                                   measuredAt, std::move(probe)))
            measurements.push_back(std::move(*measurement));
    }
    return measurements;
}

auto CloudDatacenterStore::save(ServiceGeneration service, std::span<const Measurement> measurements)
    -> libchiaking::Expected<void, Error>
{
    std::vector<cloud::Datacenter> datacenters;
    datacenters.reserve(measurements.size());
    for (const Measurement& measurement : measurements) {
        cloud::Datacenter datacenter;
        datacenter.name = measurement.name().text();
        datacenter.rttMs = static_cast<int>(measurement.round_trip().count());
        for (const auto sample : measurement.probe().samples)
            datacenter.rttSamples.push_back(static_cast<int>(sample.count()));
        datacenter.mtuIn = measurement.probe().mtu_in;
        datacenter.mtuOut = measurement.probe().mtu_out;
        datacenter.port = measurement.probe().port;
        datacenter.publicIp = measurement.probe().public_address;
        datacenter.maxBandwidth = measurement.probe().max_bandwidth;
        datacenter.measured = true;
        datacenter.measuredAtMs = measurement.measured_at().time_since_epoch().count();
        datacenters.push_back(std::move(datacenter));
    }
    settings.setCloudDatacenters(pscloud(service), std::move(datacenters));
    if (settings.writeStores() != 0)
        return unexpected(Error::unavailable);
    return {};
}

auto CloudDatacenterStore::pscloud(ServiceGeneration service) noexcept -> bool
{
    return service == ServiceGeneration::ps5_cloud;
}

}
