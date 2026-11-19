#include "cloud/cloud_settings.hpp"
#include "settings/settings_store.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <string_view>

namespace akira::cloud {

CloudSettings::CloudSettings(settings::SettingsStore& store)
    : store(store), saved(store)
{
    read(store.loaded());
    store.attach(*this);
}

CloudSettings::~CloudSettings()
{
    store.detach(*this);
}

void CloudSettings::read(const toml::table& config)
{
    const auto video = config["video"]["cloud"];
    pscloudVideo.resolution = static_cast<int>(video["pscloud"]["resolution"].value<int64_t>().value_or(1080));
    psnowVideo.resolution = static_cast<int>(video["psnow"]["resolution"].value<int64_t>().value_or(1080));
    pscloudVideo.bitrate = static_cast<int>(video["pscloud"]["bitrate"].value<int64_t>().value_or(10000));
    psnowVideo.bitrate = static_cast<int>(video["psnow"]["bitrate"].value<int64_t>().value_or(10000));
    if (auto val = video["pscloud"]["fsr_enabled"].value<bool>())
        pscloudVideo.fsr = *val;
    if (auto val = video["psnow"]["fsr_enabled"].value<bool>())
        psnowVideo.fsr = *val;

    cloudDuid = config["cloud"]["duid"].value<std::string>().value_or("");
    sort = static_cast<int>(config["cloud"]["sort_state"].value<int64_t>().value_or(0));
    filterText = config["cloud"]["filter"].value<std::string>().value_or("");
}

void CloudSettings::write(toml::table& config) const
{
    auto serviceTable = [](const Service& source) {
        toml::table t;
        t.insert("resolution", source.resolution);
        t.insert("bitrate", source.bitrate);
        t.insert("fsr_enabled", source.fsr);
        return t;
    };
    toml::table video;
    video.insert("pscloud", serviceTable(pscloudVideo));
    video.insert("psnow", serviceTable(psnowVideo));
    settings::SettingsStore::table(config, "video").insert("cloud", std::move(video));

    auto cloud = [&config]() -> toml::table& { return settings::SettingsStore::table(config, "cloud"); };
    if (!cloudDuid.empty()) cloud().insert("duid", cloudDuid);
    if (sort != 0) cloud().insert("sort_state", sort);
    if (!filterText.empty()) cloud().insert("filter", filterText);
}

bool CloudSettings::isValidDuid(const std::string& duid)
{
    constexpr std::string_view prefix = "0000000700410080";
    return duid.size() == 48 && duid.starts_with(prefix) &&
        std::ranges::all_of(duid, [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

void CloudSettings::ensureDuid(libchiaking::platform::SecureRandomProvider& random)
{
    if (isValidDuid(cloudDuid))
        return;

    std::array<std::byte, 16> randomBytes{};
    if (!random.fill_bytes(randomBytes)) {
        log::error("Failed to generate cloud DUID");
        cloudDuid.clear();
        return;
    }
    std::string generated("0000000700410080");
    for (const std::byte byte : randomBytes)
        std::format_to(std::back_inserter(generated), "{:02x}", std::to_integer<unsigned>(byte));
    cloudDuid = std::move(generated);
    if (!store.trySave())
        log::error("Failed to persist cloud DUID");
}

}
