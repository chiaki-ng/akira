#include "test_util.hpp"

#include "accounts/account_settings.hpp"
#include "app/app_settings.hpp"
#include "cloud/cloud_settings.hpp"
#include "consoles/console_settings.hpp"
#include "network/network_settings.hpp"
#include "play/controller_settings.hpp"
#include "play/remap_target.hpp"
#include "play/stream_settings.hpp"
#include "play/swipe_direction.hpp"
#include "play/switch_button.hpp"
#include "settings/developer_settings.hpp"
#include "settings/migrations/registry.hpp"
#include "settings/settings_store.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using akira::play::bits;
using akira::play::remapKey;
using akira::play::RemapTarget;
using akira::play::SwitchButton;

constexpr std::string_view fixtureBody = R"toml(
active_profile_id = 2

[video]
zero_copy = true
software_hevc = true
renderer = "vulkan"

[video.local]
resolution = "1080p"
fps = 30
bitrate = 15000
fsr_enabled = true
fsr_target = 1440

[video.remote]
resolution = "540p"
fps = 60
bitrate = 5000
fsr_enabled = false
fsr_target = 0

[video.vpn]
resolution = "360p"
fps = 30
bitrate = 2000
fsr_enabled = true
fsr_target = 720

[video.cloud.pscloud]
resolution = 720
bitrate = 12000
fsr_enabled = true

[video.cloud.psnow]
resolution = 1080
bitrate = 8000
fsr_enabled = false

[picture]
dithering_enabled = true
dithering_strength = 4.5
rcas_enabled = true
rcas_sharpness = 0.25

[overlay]
stats_x = 0.25
stats_y = 0.75

[cloud]
duid = "00000007004100800123456789abcdef0123456789abcdef"
sort_state = 2
filter = "ps5"

[[cloud.datacenters.pscloud]]
name = "fra-1"
rtt = 21
rtts = [20, 21, 22]
mtu_in = 1454
mtu_out = 1454
port = 2053
public_ip = "203.0.113.7"
max_bandwidth = 50000
measured = true
measured_at = 1700000000000

[[cloud.datacenters.psnow]]
name = "ams-2"
rtt = 30
rtts = []
mtu_in = 1400
mtu_out = 1380
port = 2054
public_ip = "203.0.113.8"
max_bandwidth = 30000
measured = false

[network]
holepunch_retry = true
port_guessing = false
port_guessing_count = 50
port_guessing_socks = 90
discovery_subnets = "192.168.1.0/24"
custom_dns = "1.1.1.1, 9.9.9.9"
companion_port = 9090

[stream]
auto_reconnect = false
sleep_on_exit = true
request_idr_on_fec_failure = false
packet_loss_max = 0.125

[stream.takion_versions]
ps4 = 9
ps5 = 15
ps5_cloud = 23
ps_now = 12

[ui]
theme = "glass"
hide_account_name = true
connection_show_stages = false

[updates]
channel = "beta"
auto_check = false
last_check = 1700000000
install_path = "sdmc:/switch/akira/akira.nro"

[debug]
locale = "ja"
file_logging = true
silence_all_logs = true
lwip_log = true
wireguard_log = true
render_log = true
chiaki_log_level = "debug"
discovery_log = true
ffmpeg_log = true
ipc_stats = true
fake_hosts = true
power_user_menu_unlocked = true
unlock_bitrate_max = true
update_server = "http://10.0.0.2:8000"

[input]
haptic = 1
gyro_source = 2

[input.rumble.default]
strength = 1.0
ceiling = 1.0
per_motor = true
haptic_intensity = 3
output_mode = 0
lightbar_enabled = false
lightbar_r = 0
lightbar_g = 128
lightbar_b = 255
rumble_source = 1

[input.rumble.dualsense]
strength = 0.75
ceiling = 0.5
per_motor = true
haptic_intensity = 4
output_mode = 0
lightbar_enabled = true
lightbar_r = 255
lightbar_g = 16
lightbar_b = 32
rumble_source = 2

[input.rumble.generic]
strength = 0.5
ceiling = 1.0
per_motor = true
haptic_intensity = 1
output_mode = 1
lightbar_enabled = false
lightbar_r = 0
lightbar_g = 128
lightbar_b = 255
rumble_source = 0

[input.rumble.joycon]
strength = 1.0
ceiling = 0.75
per_motor = false
freq_low = 140.0
freq_high = 185.0
envelope_attack = 0.5
envelope_decay = 0.75
haptic_intensity = 2
output_mode = 0
lightbar_enabled = false
lightbar_r = 0
lightbar_g = 128
lightbar_b = 255
rumble_source = 1

[input.rumble.switch]
strength = 0.5
ceiling = 0.75
per_motor = false
freq_low = 160.0
freq_high = 320.0
envelope_attack = 0.25
envelope_decay = 0.875
haptic_intensity = 5
output_mode = 0
lightbar_enabled = false
lightbar_r = 0
lightbar_g = 128
lightbar_b = 255
rumble_source = 1

[input.rumble.switchpro]
strength = 1.0
ceiling = 1.0
per_motor = false
freq_low = 100.0
freq_high = 200.0
envelope_attack = 1.0
envelope_decay = 0.5
haptic_intensity = 0
output_mode = 0
lightbar_enabled = false
lightbar_r = 0
lightbar_g = 128
lightbar_b = 255
rumble_source = 2

[input.button_mapping]
cross = ["A"]
circle = ["B"]
square = ["X"]
triangle = ["Y"]
l1 = ["L"]
r1 = ["R"]
l2 = ["ZL"]
r2 = ["ZR"]
l3 = ["L Stick"]
r3 = ["R Stick"]
options = ["Plus"]
share = ["Minus", "D-Up"]
touchpad = ["SL(R)"]
ps = ["L", "R", "Minus"]
swipe_up = ["RS-Up"]
swipe_down = []
swipe_left = ["LS-Left"]
swipe_right = []
touchpad_enabled = false
swipe_up_enabled = false
swipe_down_enabled = true
swipe_left_enabled = false
swipe_right_enabled = true

[[profiles]]
profile_id = 1
online_id = "HomePlayer"
account_id = "AQIDBAUGBwg="
avatar_url = "https://example.invalid/avatar.png"
trophies_enabled = false
legacy = true

[[profiles.cloud_shortcuts]]
product_id = "EP0001-CUSA00001_00-GAME000000000000"
name = "Example Game"
image_url = "https://example.invalid/square.png"
landscape_image_url = "https://example.invalid/wide.png"
concept_id = "10000001"
category = "GAME"
service_type = "PS_PLUS"
platform = "PS5"
is_owned = true
stream_service_type = "pscloud"
stream_identifier = "stream-1"
entitlement_id = "EP0001-CUSA00001_00-GAME000000000000"
store_product_id = "EP0001-CUSA00001_00-STORE00000000000"
concept_url = "https://example.invalid/concept/10000001"
plus_catalog = true

[[profiles.cloud_shortcuts]]
product_id = "UP0002-PPSA00002_00-GAME000000000000"
name = "Second Game"

[profiles.sign_in]
npsso = "npsso-token-value"
npsso_obtained_at = 1700000000000
npsso_expires_at = 1705184000000
app_token = "app-token-value"
app_token_expires_at = 1700003600000
app_token_obtained_at = 1700000000000
app_refresh_token = "app-refresh-value"
app_refresh_token_obtained_at = 1700000000000
app_refresh_token_expires_at = 1705184000000
remote_play_token = "remote-play-value"
remote_play_token_expires_at = 1700003600000
remote_play_token_obtained_at = 1700000000000
remote_play_refresh_token = "remote-play-refresh-value"
remote_play_refresh_token_obtained_at = 1700000000000
remote_play_refresh_token_expires_at = 1705184000000
region_country = "GB"
region_language = "en"
device_id = "00000007004100800123456789abcdef0123456789abcdef"

[[profiles.pairings]]
hardware_id = "a0b1c2d3e4f5"
rp_regist_key = "ERITFBUWFxgZGhscHR4fIA=="
rp_key = "ISIjJCUmJygpKissLS4vMA=="
rp_key_type = 1
device_uid = "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20"

[[profiles]]
profile_id = 2
online_id = "RemotePlayer"
account_id = "CQoLDA0ODxA="

[[consoles]]
console_id = 3
nickname = "Living Room"
server_nickname = "PS5-123"
host_addr = "192.168.1.20"
target = 1000100
hardware_id = "a0b1c2d3e4f5"
host_type = 2
console_pin = "1234"
remote_duid = "abcdef"
haptic = 2
takion_version = 15

[[consoles]]
console_id = 7
nickname = "Office (Remote)"
host_addr = "office.example.invalid"
target = 1000
host_type = 3

[[registrations]]
console_id = 3
profile_id = 1
rp_key = "AQIDBAUGBwgJCgsMDQ4PEA=="
rp_regist_key = "EBEPDg0MCwoJCAcGBQQDAg=="
rp_key_type = 1

[[registrations]]
console_id = 7
profile_id = 2
rp_key = "MTIzNDU2Nzg5MDEyMzQ1Ng=="
rp_regist_key = "YWJjZGVmZ2hpamtsbW5vcA=="
rp_key_type = 0

[[paired_consoles]]
hardware_id = "a0b1c2d3e4f5"
firmware = "ps5_1"
name = "PS5-123"
name_source = "pairing"
address = "192.168.1.20"
)toml";

std::string fixture()
{
    return std::format("version = {}\n{}", akira::settings::migrations::buildSettingsMigrator().latest_version(),
                       fixtureBody);
}

class ScratchDirectory {
public:
    ScratchDirectory()
        : root(std::filesystem::temp_directory_path() /
               std::format("akira-settings-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(),
                           counter++))
    {
        std::filesystem::create_directories(root);
    }

    ~ScratchDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    const std::filesystem::path& path() const { return root; }

    void write(const std::string& name, const std::string& text) const
    {
        std::ofstream(root / name, std::ios::trunc) << text;
    }

private:
    static inline std::atomic<int> counter{0};
    std::filesystem::path root;
};

class LoadedSettings {
public:
    explicit LoadedSettings(const std::filesystem::path& directory)
        : store(directory),
          developer(store),
          stream(store, false),
          controllers(store),
          cloud(store),
          network(store, [this](const std::vector<akira::network::Ipv4Address>& servers) { dnsServers = servers; }),
          accounts(store, [this](bool legacy) { legacyGate = legacy; }),
          consoles(store),
          app(store)
    {
        store.finishLoading();
    }

    std::vector<akira::network::Ipv4Address> dnsServers;
    std::optional<bool> legacyGate;
    akira::settings::SettingsStore store;
    akira::settings::DeveloperSettings developer;
    akira::play::StreamSettings stream;
    akira::play::ControllerSettings controllers;
    akira::cloud::CloudSettings cloud;
    akira::network::NetworkSettings network;
    akira::accounts::AccountSettings accounts;
    akira::consoles::ConsoleSettings consoles;
    akira::app::AppSettings app;
};

std::string printed(const toml::table& table)
{
    std::ostringstream out;
    out << table;
    return out.str();
}

bool sameTables(const toml::table& written, const toml::table& expected)
{
    if (written == expected)
        return true;
    std::printf("      written:\n%s\n      expected:\n%s\n", printed(written).c_str(), printed(expected).c_str());
    return false;
}

}

TEST(settings_round_trip_writes_the_fixture_back_unchanged)
{
    ScratchDirectory directory;
    directory.write("akira.toml", fixture());
    {
        LoadedSettings settings(directory.path());
        settings.store.save();
    }
    const toml::table written = toml::parse_file((directory.path() / "akira.toml").string()).table();
    CHECK(sameTables(written, toml::parse(fixture()).table()));
    CHECK(!std::filesystem::exists(directory.path() / std::format("akira.toml.v{}.bak",
        akira::settings::migrations::buildSettingsMigrator().latest_version())));
}

TEST(settings_round_trip_is_stable_across_a_second_save)
{
    ScratchDirectory directory;
    directory.write("akira.toml", fixture());
    {
        LoadedSettings settings(directory.path());
        settings.store.save();
    }
    const toml::table first = toml::parse_file((directory.path() / "akira.toml").string()).table();
    {
        LoadedSettings settings(directory.path());
        settings.store.save();
    }
    CHECK(sameTables(toml::parse_file((directory.path() / "akira.toml").string()).table(), first));
}

TEST(settings_round_trip_reads_every_typed_value)
{
    using namespace akira;
    ScratchDirectory directory;
    directory.write("akira.toml", fixture());
    LoadedSettings settings(directory.path());

    const auto& stream = settings.stream;
    CHECK(stream.localVideoResolution() == play::VideoResolution::P1080);
    CHECK(stream.localVideoFps() == play::VideoFps::Fps30);
    CHECK_EQ(stream.localVideoBitrate(), 15000);
    CHECK(stream.localFsrEnabled());
    CHECK_EQ(stream.fsrTarget(play::StreamProfile::Local), 1440);
    CHECK(stream.remoteVideoResolution() == play::VideoResolution::P540);
    CHECK(stream.remoteVideoFps() == play::VideoFps::Fps60);
    CHECK_EQ(stream.remoteVideoBitrate(), 5000);
    CHECK(!stream.remoteFsrEnabled());
    CHECK(stream.vpnVideoResolution() == play::VideoResolution::P360);
    CHECK(stream.vpnVideoFps() == play::VideoFps::Fps30);
    CHECK_EQ(stream.vpnVideoBitrate(), 2000);
    CHECK(stream.vpnFsrEnabled());
    CHECK_EQ(stream.fsrTarget(play::StreamProfile::Vpn), 720);
    CHECK(stream.zeroCopyVideo());
    CHECK(stream.softwareHevc());
    CHECK(stream.videoRenderer() == play::RendererKind::Vulkan);
    CHECK(stream.ditheringEnabled());
    CHECK_EQ(stream.ditheringStrength(), 4.5f);
    CHECK(stream.rcasEnabled());
    CHECK_EQ(stream.rcasSharpness(), 0.25f);
    CHECK_EQ(stream.statsOverlayX(), 0.25f);
    CHECK_EQ(stream.statsOverlayY(), 0.75f);
    CHECK(!stream.requestIdrOnFecFailure());
    CHECK_EQ(stream.packetLossMax(), 0.125f);
    CHECK(stream.unlockBitrateMax());
    CHECK_EQ(stream.maxBitrate(play::VideoResolution::P1080), 50000);
    CHECK_EQ(stream.minBitrate(play::VideoResolution::P720), 5000);
    CHECK_EQ(stream.upscaleTargetHeight(play::StreamProfile::Local, play::VideoResolution::P1080), 0);
    CHECK_EQ(stream.upscaleTargetHeight(play::StreamProfile::Vpn, play::VideoResolution::P360), 720);

    const auto& controllers = settings.controllers;
    CHECK(controllers.hapticPreset() == play::HapticPreset::Weak);
    CHECK(controllers.gyroSource() == play::GyroSource::Right);
    const play::RumbleProfile dualSense = controllers.rumbleProfile(play::kRumbleKeyDualSense);
    CHECK_EQ(dualSense.strength, 0.75f);
    CHECK_EQ(dualSense.ceiling, 0.5f);
    CHECK(dualSense.haptic_intensity == play::HapticIntensity::Strong);
    CHECK(dualSense.lightbar_enabled);
    CHECK_EQ(static_cast<int>(dualSense.lightbar_r), 255);
    CHECK(dualSense.rumble_source == play::RumbleSource::Game);
    const play::RumbleProfile switchProfile = controllers.rumbleProfile(play::kRumbleKeySwitch);
    CHECK(!switchProfile.per_motor);
    CHECK_EQ(switchProfile.freq_high, 320.0f);
    CHECK_EQ(switchProfile.envelope_decay, 0.875f);
    CHECK(controllers.rumbleProfile(play::kRumbleKeyGeneric).output_mode == play::PadOutputMode::Basic);
    const auto& mapping = controllers.buttonMapping();
    CHECK(mapping.at(remapKey(RemapTarget::Cross)) == std::vector<uint64_t>{bits(SwitchButton::A)});
    CHECK(mapping.at(remapKey(RemapTarget::Ps)) ==
          (std::vector<uint64_t>{bits(SwitchButton::L), bits(SwitchButton::R), bits(SwitchButton::Minus)}));
    CHECK(mapping.at(play::SWIPE_TOUCHPAD_UP) == std::vector<uint64_t>{bits(SwitchButton::StickRUp)});
    CHECK(mapping.at(play::SWIPE_TOUCHPAD_DOWN).empty());
    CHECK(!controllers.isButtonEnabled(remapKey(RemapTarget::Touchpad)));
    CHECK(!controllers.isButtonEnabled(play::SWIPE_TOUCHPAD_UP));
    CHECK(controllers.isButtonEnabled(play::SWIPE_TOUCHPAD_DOWN));
    CHECK(!controllers.isButtonEnabled(play::SWIPE_TOUCHPAD_LEFT));
    CHECK(controllers.isButtonEnabled(play::SWIPE_TOUCHPAD_RIGHT));

    const auto& cloud = settings.cloud;
    CHECK_EQ(cloud.videoResolution(true), 720);
    CHECK_EQ(cloud.videoBitrate(true), 12000);
    CHECK(cloud.fsrEnabled(true));
    CHECK_EQ(cloud.videoResolution(false), 1080);
    CHECK_EQ(cloud.videoBitrate(false), 8000);
    CHECK(!cloud.fsrEnabled(false));
    CHECK_EQ(cloud.sortState(), 2);
    CHECK_EQ(cloud.filter(), std::string("ps5"));
    CHECK_EQ(cloud.duid(), std::string("00000007004100800123456789abcdef0123456789abcdef"));
    const auto datacenters = settings.cloud.datacenters().datacenters(true);
    REQUIRE(datacenters.size() == 1);
    CHECK_EQ(datacenters.front().name, std::string("fra-1"));
    CHECK(datacenters.front().rttSamples == (std::vector<int>{20, 21, 22}));
    CHECK_EQ(datacenters.front().measuredAtMs, int64_t{1700000000000});
    CHECK_EQ(settings.cloud.datacenters().datacenters(false).size(), size_t{1});

    const auto& network = settings.network;
    CHECK(network.holepunchRetry());
    CHECK(!network.portGuessing());
    CHECK_EQ(network.portGuessingCount(), 50);
    CHECK_EQ(network.portGuessingSocks(), 90);
    CHECK_EQ(network.discoverySubnets(), std::string("192.168.1.0/24"));
    REQUIRE(network.customDnsServers().size() == 2);
    CHECK_EQ(network.customDnsServers()[1].toString(), std::string("9.9.9.9"));
    CHECK_EQ(settings.dnsServers.size(), size_t{2});
    CHECK(!network.autoReconnect());
    CHECK_EQ(network.companionPort(), 9090);

    const auto& developer = settings.developer;
    CHECK_EQ(developer.takionVersion(settings::TakionTarget::PS4), 9);
    CHECK_EQ(developer.takionVersion(settings::TakionTarget::PS5), 15);
    CHECK_EQ(developer.takionVersion(settings::TakionTarget::PS5Cloud), 23);
    CHECK_EQ(developer.takionVersion(settings::TakionTarget::PSNow), 12);
    CHECK(developer.powerUserMenuUnlocked());
    CHECK(developer.ipcStatsEnabled());
    CHECK(developer.fakeHosts());
    CHECK_EQ(developer.updateServer(), std::string("http://10.0.0.2:8000"));
    CHECK(developer.silenceAllLogs());
    CHECK(!developer.fileLogging());
    CHECK(developer.chiakiLogVerbosity() == settings::ChiakiLogVerbosity::Debug);
    CHECK(developer.chiakiDebugLogging());
    CHECK(developer.lwipLog());
    CHECK(developer.wireguardLog());
    CHECK(developer.renderLog());
    CHECK(developer.discoveryLog());
    CHECK(developer.ffmpegLog());

    const auto& app = settings.app;
    CHECK_EQ(app.uiTheme(), std::string("glass"));
    CHECK(app.hideAccountName());
    CHECK_EQ(app.maskAccountName("RemotePlayer"), std::string("Re***"));
    CHECK(!app.connectionShowStages());
    CHECK(app.sleepOnExit());
    CHECK_EQ(app.debugLocale(), std::string("ja"));
    CHECK_EQ(app.updateChannel(), std::string("beta"));
    CHECK(!app.autoCheckUpdates());
    CHECK_EQ(app.lastUpdateCheck(), int64_t{1700000000});
    CHECK_EQ(app.updateInstallPath(), std::string("sdmc:/switch/akira/akira.nro"));

    const auto& accounts = settings.accounts;
    REQUIRE(accounts.profiles().size() == 2);
    CHECK_EQ(accounts.activeProfileId(), int64_t{2});
    CHECK_EQ(accounts.psnOnlineId(), std::string("RemotePlayer"));
    CHECK_EQ(accounts.psnAccountId(), std::string("CQoLDA0ODxA="));
    CHECK(!accounts.isLegacyProfileActive());
    CHECK(settings.legacyGate == std::optional<bool>{false});
    const accounts::Profile* home = accounts.findProfile(1);
    REQUIRE(home != nullptr);
    CHECK(home->legacy);
    CHECK(!home->trophiesEnabled);
    REQUIRE(home->cloudShortcuts.size() == 2);
    CHECK(home->cloudShortcuts.front().plusCatalog);
    CHECK(accounts.cloudShortcuts().empty());
    const auto signIns = settings.accounts.signIns().load_saved_users();
    REQUIRE(signIns.has_value());
    CHECK_EQ(signIns->size(), size_t{1});

    const auto& records = settings.consoles.records();
    REQUIRE(records.size() == 2);
    const consoles::HostRecord& livingRoom = records.at("Living Room");
    CHECK_EQ(livingRoom.consoleId, int64_t{3});
    CHECK(livingRoom.hostType == consoles::HostType::Manual);
    CHECK_EQ(livingRoom.consolePin, std::string("1234"));
    CHECK_EQ(livingRoom.haptic, 2);
    CHECK(livingRoom.pinnedTakionVersion == std::optional<std::uint32_t>{15});
    REQUIRE(livingRoom.hardwareId.has_value());
    CHECK_EQ(livingRoom.hardwareId->text(), std::string("a0b1c2d3e4f5"));
    REQUIRE(livingRoom.legacyRegistrations.size() == 1);
    CHECK_EQ(livingRoom.legacyRegistrations.front().profileId, int64_t{1});
    const consoles::HostRecord& office = records.at("Office (Remote)");
    CHECK(office.hostType == consoles::HostType::Remote);
    CHECK(office.firmware.has_value() && office.firmware->family() == libchiaking::consoles::Firmware::Family::ps4_10);
    CHECK_EQ(settings.consoles.claimConsoleId(), int64_t{8});
    const auto saved = settings.consoles.pairings().load_saved_consoles();
    REQUIRE(saved.has_value());
    CHECK_EQ(saved->consoles.size(), size_t{1});
    CHECK_EQ(saved->pairings.size(), size_t{1});
}

TEST(settings_store_writes_only_the_live_stores_on_a_live_save)
{
    ScratchDirectory directory;
    directory.write("akira.toml", fixture());
    {
        LoadedSettings settings(directory.path());
        settings.app.setUiTheme("cobalt");
        settings.cloud.datacenters().setDatacenters(false, {});
        CHECK(settings.store.saveLive());
    }
    const toml::table written = toml::parse_file((directory.path() / "akira.toml").string()).table();
    CHECK_EQ(written["ui"]["theme"].value_or(std::string{}), std::string("glass"));
    CHECK(!written["cloud"]["datacenters"]["psnow"]);
    CHECK(written["cloud"]["datacenters"]["pscloud"].is_array());
}

TEST(settings_store_migrates_and_backs_up_an_old_config)
{
    ScratchDirectory directory;
    directory.write("akira.toml", "version = 11\n[video]\nrenderer = \"vulkan\"\n");
    {
        LoadedSettings settings(directory.path());
        CHECK(settings.stream.videoRenderer() == akira::play::RendererKind::Drako3d);
    }
    CHECK(std::filesystem::exists(directory.path() / "akira.toml.v11.bak"));
    const toml::table written = toml::parse_file((directory.path() / "akira.toml").string()).table();
    CHECK_EQ(written["version"].value_or(int64_t{0}), akira::settings::migrations::buildSettingsMigrator().latest_version());
    CHECK(written["input"]["rumble"]["switch"].is_table());
}

TEST(settings_store_starts_from_defaults_without_a_config)
{
    ScratchDirectory directory;
    LoadedSettings settings(directory.path());
    CHECK(settings.stream.localVideoResolution() == akira::play::VideoResolution::P720);
    CHECK_EQ(settings.stream.localVideoBitrate(), 10000);
    CHECK(!settings.controllers.hasRumbleProfile(akira::play::kRumbleKeySwitch));
    CHECK(settings.consoles.records().empty());
    CHECK(!std::filesystem::exists(directory.path() / "akira.toml"));
}

TEST(settings_store_converts_a_legacy_config)
{
    ScratchDirectory directory;
    directory.write("akira.conf",
        "video_resolution = \"1080p\"\n"
        "video_fps = 30\n"
        "companion_port = 70000\n"
        "[[Manual] Den]\n"
        "host_addr = \"192.168.1.30\"\n"
        "psn_online_id = \"OldPlayer\"\n"
        "psn_account_id = \"AQIDBAUGBwg=\"\n"
        "console_pin = \"4321\"\n"
        "target = 1000100\n"
        "haptic = 2\n"
        "rp_key = \"AQIDBAUGBwgJCgsMDQ4PEA==\"\n"
        "rp_regist_key = \"EBEPDg0MCwoJCAcGBQQDAg==\"\n"
        "rp_key_type = 1\n");
    {
        LoadedSettings settings(directory.path());
        CHECK(settings.stream.localVideoResolution() == akira::play::VideoResolution::P1080);
        CHECK(settings.stream.remoteVideoFps() == akira::play::VideoFps::Fps30);
        CHECK_EQ(settings.stream.localVideoBitrate(), 10000);
        CHECK_EQ(settings.network.companionPort(), 8080);
        CHECK_EQ(settings.accounts.psnOnlineId(), std::string("OldPlayer"));
        CHECK_EQ(settings.accounts.activeProfileId(), int64_t{1});
        const auto& records = settings.consoles.records();
        REQUIRE(records.size() == 1);
        const auto& den = records.at("Den");
        CHECK(den.hostType == akira::consoles::HostType::Manual);
        CHECK_EQ(den.hostAddr, std::string("192.168.1.30"));
        CHECK_EQ(den.consolePin, std::string("4321"));
        CHECK_EQ(den.haptic, 2);
        CHECK_EQ(den.consoleId, int64_t{1});
        REQUIRE(den.legacyRegistrations.size() == 1);
        CHECK_EQ(den.legacyRegistrations.front().profileId, int64_t{1});
    }
    CHECK(std::filesystem::exists(directory.path() / "akira.toml"));
}
