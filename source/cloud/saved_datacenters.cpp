#include "cloud/saved_datacenters.hpp"
#include "settings/settings_store.hpp"

#include <utility>

namespace akira::cloud {

SavedDatacenters::SavedDatacenters(settings::SettingsStore& store)
    : store(store)
{
    const auto& config = store.loaded();
    pscloudDatacenters = readList(config["cloud"]["datacenters"]["pscloud"].as_array());
    psnowDatacenters = readList(config["cloud"]["datacenters"]["psnow"].as_array());
    store.attachLive(*this);
}

SavedDatacenters::~SavedDatacenters()
{
    store.detach(*this);
}

std::vector<Datacenter> SavedDatacenters::datacenters(bool pscloud) const
{
    std::lock_guard<std::mutex> lock(mutex);
    return pscloud ? pscloudDatacenters : psnowDatacenters;
}

void SavedDatacenters::setDatacenters(bool pscloud, std::vector<Datacenter> datacenters)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (pscloud)
        pscloudDatacenters = std::move(datacenters);
    else
        psnowDatacenters = std::move(datacenters);
}

void SavedDatacenters::write(toml::table& config) const
{
    toml::table datacenters;
    std::lock_guard<std::mutex> lock(mutex);
    if (!pscloudDatacenters.empty())
        datacenters.insert("pscloud", rowsOf(pscloudDatacenters));
    if (!psnowDatacenters.empty())
        datacenters.insert("psnow", rowsOf(psnowDatacenters));
    if (!datacenters.empty())
        settings::SettingsStore::table(config, "cloud").insert_or_assign("datacenters", std::move(datacenters));
}

std::vector<Datacenter> SavedDatacenters::readList(const toml::array* rows)
{
    std::vector<Datacenter> out;
    if (!rows)
        return out;

    for (const auto& node : *rows) {
        const auto* t = node.as_table();
        if (!t) continue;

        Datacenter dc;
        dc.name = (*t)["name"].value<std::string>().value_or("");
        if (dc.name.empty()) continue;

        dc.rttMs = static_cast<int>((*t)["rtt"].value<int64_t>().value_or(0));
        dc.mtuIn = static_cast<int>((*t)["mtu_in"].value<int64_t>().value_or(0));
        dc.mtuOut = static_cast<int>((*t)["mtu_out"].value<int64_t>().value_or(0));
        dc.port = static_cast<int>((*t)["port"].value<int64_t>().value_or(0));
        dc.publicIp = (*t)["public_ip"].value<std::string>().value_or("");
        dc.maxBandwidth = static_cast<int>((*t)["max_bandwidth"].value<int64_t>().value_or(0));
        dc.measured = (*t)["measured"].value<bool>().value_or(false);
        dc.measuredAtMs = (*t)["measured_at"].value<int64_t>().value_or(0);

        if (const auto* rtts = t->get_as<toml::array>("rtts")) {
            for (const auto& rtt : *rtts) {
                if (auto val = rtt.value<int64_t>())
                    dc.rttSamples.push_back(static_cast<int>(*val));
            }
        }

        out.push_back(std::move(dc));
    }
    return out;
}

toml::array SavedDatacenters::rowsOf(const std::vector<Datacenter>& datacenters)
{
    toml::array rows;
    for (const Datacenter& dc : datacenters) {
        toml::table t;
        t.insert("name", dc.name);
        t.insert("rtt", dc.rttMs);

        toml::array rtts;
        for (int sample : dc.rttSamples)
            rtts.push_back(sample);
        t.insert("rtts", rtts);

        t.insert("mtu_in", dc.mtuIn);
        t.insert("mtu_out", dc.mtuOut);
        t.insert("port", dc.port);
        t.insert("public_ip", dc.publicIp);
        t.insert("max_bandwidth", dc.maxBandwidth);
        t.insert("measured", dc.measured);
        if (dc.measuredAtMs > 0) t.insert("measured_at", dc.measuredAtMs);
        rows.push_back(std::move(t));
    }
    return rows;
}

}
