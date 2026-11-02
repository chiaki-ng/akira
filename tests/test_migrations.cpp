#include "test_util.hpp"

#include <toml++/toml.hpp>

#include <string>
#include <string_view>

#include "core/migrations/m000_baseline.hpp"
#include "core/migrations/m001_unify_psn_auth.hpp"
#include "core/migrations/m002_multi_profile.hpp"
#include "core/migrations/m003_drop_hardened_nat.hpp"
#include "core/migrations/m004_cloud_datacenter_tables.hpp"
#include "core/migrations/m005_group_settings_tables.hpp"
#include "core/migrations/m006_split_cloud_video_by_service.hpp"
#include "core/migrations/m007_split_psn_sign_ins.hpp"
#include "core/migrations/registry.hpp"

namespace {

toml::table run_migrate(std::string_view src)
{
    tomlmigrate::Options opts;
    opts.version_key = "version";
    opts.assume_missing_version = 0;

    tomlmigrate::Migrator m(opts);
    chiaki_migrations::register_m000_baseline(m);
    chiaki_migrations::register_m001_unify_psn_auth(m);
    chiaki_migrations::register_m002_multi_profile(m);
    chiaki_migrations::register_m003_drop_hardened_nat(m);
    chiaki_migrations::register_m004_cloud_datacenter_tables(m);
    chiaki_migrations::register_m005_group_settings_tables(m);
    chiaki_migrations::register_m006_split_cloud_video_by_service(m);
    chiaki_migrations::register_m007_split_psn_sign_ins(m);

    toml::table doc = toml::parse(src);
    m.migrate(doc);
    return doc;
}

int64_t arr_size(const toml::table& doc, const char* key)
{
    const auto* a = doc.get_as<toml::array>(key);
    return a ? static_cast<int64_t>(a->size()) : -1;
}

const toml::table* nth(const toml::table& doc, const char* key, size_t i)
{
    const auto* a = doc.get_as<toml::array>(key);
    if (!a || i >= a->size())
        return nullptr;
    return (*a)[i].as_table();
}

std::string tstr(const toml::table* t, const char* k)
{
    if (!t)
        return "";
    auto v = (*t)[k].value<std::string>();
    return v ? *v : "";
}

int64_t tint(const toml::table* t, const char* k)
{
    if (!t)
        return -999;
    auto v = (*t)[k].value<int64_t>();
    return v ? *v : -999;
}

const toml::table* nth_in(const toml::array* a, size_t i)
{
    if (!a || i >= a->size())
        return nullptr;
    return (*a)[i].as_table();
}

int64_t size_of(const toml::array* a)
{
    return a ? static_cast<int64_t>(a->size()) : -1;
}

int64_t version_of(const toml::table& doc)
{
    auto v = doc["version"].value<int64_t>();
    return v ? *v : -1;
}

const toml::table* find_str(const toml::table& doc, const char* key,
                            const char* field, const std::string& val)
{
    const auto* a = doc.get_as<toml::array>(key);
    if (!a)
        return nullptr;
    for (const auto& e : *a) {
        const toml::table* t = e.as_table();
        if (t && tstr(t, field) == val)
            return t;
    }
    return nullptr;
}

const toml::table* find_int(const toml::table& doc, const char* key,
                            const char* field, int64_t val)
{
    const auto* a = doc.get_as<toml::array>(key);
    if (!a)
        return nullptr;
    for (const auto& e : *a) {
        const toml::table* t = e.as_table();
        if (t && tint(t, field) == val)
            return t;
    }
    return nullptr;
}

} // namespace

TEST(migration_v2_flat_to_relational)
{
    static constexpr std::string_view src = R"(
version = 2
psn_online_id = "coolgamer"
psn_account_id = "QUFBQUFBQUE="
psn_refresh_token = "rt-token"
psn_access_token = "at-token"
psn_token_expires_at = 1700000000
global_duid = "deadbeef"
local_video_bitrate = 12000

["PS5-123"]
host_type = 1
host_addr = "192.168.1.10"
target = 1000
psn_online_id = "coolgamer"
psn_account_id = "QUFBQUFBQUE="
console_pin = "1234"
rp_key = "cnBrZXk="
rp_regist_key = "cmVnaXN0"
rp_key_type = 0
haptic = -1

["PS4-abc"]
host_type = 2
host_addr = "192.168.1.20"
target = 800
psn_online_id = "coolgamer"
psn_account_id = "QUFBQUFBQUE="
rp_key = "cA=="
rp_regist_key = "cg=="
rp_key_type = 0
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);

    CHECK_EQ(arr_size(doc, "profiles"), 1);
    const toml::table* p0 = nth(doc, "profiles", 0);
    CHECK_EQ(tint(p0, "profile_id"), 1);
    CHECK_EQ(tstr(p0, "account_id"), std::string("QUFBQUFBQUE="));
    CHECK_EQ(tstr(p0, "online_id"), std::string("coolgamer"));
    CHECK(!p0->contains("refresh_token"));
    CHECK(!p0->contains("duid"));
    CHECK_EQ(arr_size(doc, "sign_ins"), -1);

    CHECK_EQ(arr_size(doc, "consoles"), 2);
    const toml::table* cPs5 = find_str(doc, "consoles", "nickname", "PS5-123");
    const toml::table* cPs4 = find_str(doc, "consoles", "nickname", "PS4-abc");
    CHECK(cPs5 != nullptr);
    CHECK(cPs4 != nullptr);
    CHECK_EQ(tint(cPs5, "target"), 1000);
    CHECK_EQ(tstr(cPs5, "console_pin"), std::string("1234"));
    CHECK_EQ(tint(cPs4, "target"), 800);
    const int64_t idPs5 = tint(cPs5, "console_id");
    const int64_t idPs4 = tint(cPs4, "console_id");
    CHECK(idPs5 >= 1);
    CHECK(idPs4 >= 1);
    CHECK(idPs5 != idPs4);

    CHECK_EQ(arr_size(doc, "registrations"), 2);
    const toml::table* rPs5 = find_int(doc, "registrations", "console_id", idPs5);
    const toml::table* rPs4 = find_int(doc, "registrations", "console_id", idPs4);
    CHECK_EQ(tint(rPs5, "profile_id"), 1);
    CHECK_EQ(tstr(rPs5, "rp_key"), std::string("cnBrZXk="));
    CHECK_EQ(tint(rPs4, "profile_id"), 1);
    CHECK_EQ(tstr(rPs4, "rp_key"), std::string("cA=="));

    CHECK_EQ(tint(&doc, "active_profile_id"), 1);

    CHECK(!doc.contains("psn_account_id"));
    CHECK(!doc.contains("psn_refresh_token"));
    CHECK(!doc.contains("PS5-123"));
    CHECK(!doc.contains("PS4-abc"));
    CHECK(doc["video"]["local"]["bitrate"].value_or(int64_t(0)) > 0);
}

TEST(migration_differing_override_mints_local_profile)
{
    static constexpr std::string_view src = R"(
version = 2
psn_account_id = "R0xPQkFM"
psn_online_id = "mainguy"

["PS5-shared"]
host_type = 1
psn_account_id = "T1RIRVI="
psn_online_id = "friend"
rp_key = "eA=="
rp_regist_key = "eQ=="
rp_key_type = 0
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(arr_size(doc, "profiles"), 2);
    const toml::table* p0 = nth(doc, "profiles", 0);
    const toml::table* p1 = nth(doc, "profiles", 1);
    CHECK_EQ(tstr(p0, "account_id"), std::string("R0xPQkFM"));
    CHECK_EQ(tint(p1, "profile_id"), 2);
    CHECK_EQ(tstr(p1, "account_id"), std::string("T1RIRVI="));
    CHECK_EQ(tstr(p1, "online_id"), std::string("friend"));

    const toml::table* r0 = nth(doc, "registrations", 0);
    CHECK_EQ(tint(r0, "profile_id"), 2);
    CHECK_EQ(tint(&doc, "active_profile_id"), 1);
}

TEST(migration_legacy_no_version_full_chain)
{
    static constexpr std::string_view src = R"(
psn_account_id = "TEVH"

["[Auto] PS5-old"]
host_addr = "10.0.0.5"
target = 1000
rp_key = "bA=="
rp_regist_key = "bQ=="
rp_key_type = 0
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK_EQ(arr_size(doc, "profiles"), 1);
    CHECK_EQ(tstr(nth(doc, "profiles", 0), "account_id"), std::string("TEVH"));

    CHECK_EQ(arr_size(doc, "consoles"), 1);
    CHECK_EQ(tstr(nth(doc, "consoles", 0), "nickname"), std::string("PS5-old"));

    CHECK_EQ(arr_size(doc, "registrations"), 1);
    CHECK_EQ(tint(nth(doc, "registrations", 0), "profile_id"), 1);
    CHECK_EQ(tint(&doc, "active_profile_id"), 1);
    CHECK(!doc.contains("[Auto] PS5-old"));
}

TEST(migration_already_relational_is_noop)
{
    static constexpr std::string_view src = R"(
version = 3
active_profile_id = 1

[[profiles]]
profile_id = 1
account_id = "WA=="

[[consoles]]
console_id = 1
nickname = "PS5"
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK_EQ(arr_size(doc, "profiles"), 1);
    CHECK_EQ(arr_size(doc, "consoles"), 1);
    CHECK_EQ(tstr(nth(doc, "profiles", 0), "account_id"), std::string("WA=="));
}

TEST(migration_empty_new_user)
{
    static constexpr std::string_view src = "";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK(!doc.contains("profiles"));
    CHECK(!doc.contains("consoles"));
    CHECK(!doc.contains("active_profile_id"));
}

TEST(migration_account_without_consoles)
{
    static constexpr std::string_view src = R"(
version = 2
psn_account_id = "TUU="
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(arr_size(doc, "profiles"), 1);
    CHECK(!doc.contains("consoles"));
    CHECK(!doc.contains("registrations"));
    CHECK_EQ(tint(&doc, "active_profile_id"), 1);
}

TEST(migration_drops_hardened_nat_traversal)
{
    static constexpr std::string_view src = R"(
version = 3
hardened_nat_traversal = true
port_guessing = true
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK(!doc.contains("hardened_nat_traversal"));
    CHECK(doc["network"]["port_guessing"].value<bool>().value_or(false));
}

TEST(migration_without_hardened_nat_key_is_safe)
{
    static constexpr std::string_view src = R"(
version = 3
port_guessing = true
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK(!doc.contains("hardened_nat_traversal"));
    CHECK(doc["network"]["port_guessing"].value<bool>().value_or(false));
}

TEST(migration_explodes_cloud_datacenter_blobs)
{
    static constexpr std::string_view src = R"(
version = 4
cloud_datacenters_pscloud = '[{"dataCenter":"lonb","rtt":1,"rtts":[1,3],"mtu_in":1454,"mtu_out":1454,"port":40101,"publicIp":"senkusha.lonb.prod.playstation-cloud.com","maxBandwidth":25000,"measured":true},{"dataCenter":"mila","rtt":7,"rtts":[7],"mtu_in":1454,"mtu_out":1254,"port":40101,"publicIp":"senkusha.mila.prod.playstation-cloud.com","maxBandwidth":25000,"measured":false}]'
cloud_datacenters_psnow = '[{"dataCenter":"lona","rtt":2,"rtts":[2],"mtu_in":1454,"mtu_out":1454,"port":2053,"publicIp":"104.142.177.153","maxBandwidth":25000,"measured":true}]'
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    const toml::array* pscloud = doc["cloud"]["datacenters"]["pscloud"].as_array();
    const toml::array* psnow = doc["cloud"]["datacenters"]["psnow"].as_array();
    CHECK_EQ(size_of(pscloud), 2);
    CHECK_EQ(size_of(psnow), 1);

    const toml::table* lonb = nth_in(pscloud, 0);
    CHECK_EQ(tstr(lonb, "name"), std::string("lonb"));
    CHECK_EQ(tint(lonb, "rtt"), 1);
    CHECK_EQ(tint(lonb, "mtu_in"), 1454);
    CHECK_EQ(tint(lonb, "mtu_out"), 1454);
    CHECK_EQ(tint(lonb, "port"), 40101);
    CHECK_EQ(tstr(lonb, "public_ip"), std::string("senkusha.lonb.prod.playstation-cloud.com"));
    CHECK_EQ(tint(lonb, "max_bandwidth"), 25000);
    CHECK((*lonb)["measured"].value<bool>().value_or(false));

    const auto* rtts = lonb->get_as<toml::array>("rtts");
    CHECK(rtts != nullptr);
    CHECK_EQ(rtts ? static_cast<int64_t>(rtts->size()) : -1, 2);
    CHECK_EQ(rtts ? (*rtts)[1].value<int64_t>().value_or(-1) : -1, 3);

    const toml::table* mila = nth_in(pscloud, 1);
    CHECK(!(*mila)["measured"].value<bool>().value_or(true));

    const toml::table* lona = nth_in(psnow, 0);
    CHECK_EQ(tstr(lona, "public_ip"), std::string("104.142.177.153"));
    CHECK_EQ(tint(lona, "port"), 2053);
}

TEST(migration_drops_unusable_cloud_datacenter_blobs)
{
    static constexpr std::string_view src = R"(
version = 4
cloud_datacenters_pscloud = 'not json at all'
cloud_datacenters_psnow = '[]'
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK(!doc.contains("cloud_datacenters_pscloud"));
    CHECK(!doc.contains("cloud_datacenters_psnow"));
    CHECK(!doc.contains("cloud"));
}

TEST(migration_groups_flat_keys_into_domain_tables)
{
    static constexpr std::string_view src = R"(
version = 5
local_video_resolution = '1080p'
local_video_fps = 60
local_video_bitrate = 23500
local_fsr_enabled = true
cloud_video_resolution = 1080
cloud_video_bitrate = 23500
cloud_fsr_enabled = false
rcas_enabled = true
rcas_sharpness = 0.43
haptic = 2
gyro_source = 1
cloud_datacenter_pscloud = 'lonb'
cloud_attr_passed = true
cloud_favorites = "EP0001-A\nEP0002-B"
port_guessing = true
port_guessing_count = 75
companion_port = 8080
packet_loss_max = 0.035
auto_reconnect = false
sleep_on_exit = true
ui_theme = 'playstation'
hide_account_name = true
psn_request_budget = 105
update_channel = 'rc'
auto_check_updates = true
last_update_check = 1785538775
debug_chiaki_log = true
enable_file_logging = true
dev_update_server = '192.168.20.123:8099'
power_user_menu_unlocked = true

[picture_adjustments]
enable_dithering = true
dithering_strength = 4.0

[rumble]
freq_low = 140.0

[button_mapping]
touchpad_enabled = true
cross = [ 'A' ]
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);

    CHECK_EQ(doc["video"]["local"]["resolution"].value_or(std::string()), std::string("1080p"));
    CHECK_EQ(doc["video"]["local"]["fps"].value_or(int64_t(0)), 60);
    CHECK_EQ(doc["video"]["local"]["bitrate"].value_or(int64_t(0)), 23500);
    CHECK(doc["video"]["local"]["fsr_enabled"].value_or(false));
    CHECK_EQ(doc["video"]["cloud"]["pscloud"]["resolution"].value_or(int64_t(0)), 1080);
    CHECK_EQ(doc["video"]["cloud"]["pscloud"]["bitrate"].value_or(int64_t(0)), 23500);
    CHECK(!doc["video"]["cloud"]["pscloud"]["fsr_enabled"].value_or(true));
    CHECK(!doc["video"].as_table()->contains("remote"));

    CHECK(doc["picture"]["dithering_enabled"].value_or(false));
    CHECK_EQ(doc["picture"]["dithering_strength"].value_or(0.0), 4.0);
    CHECK(doc["picture"]["rcas_enabled"].value_or(false));

    CHECK_EQ(doc["input"]["haptic"].value_or(int64_t(0)), 2);
    CHECK_EQ(doc["input"]["gyro_source"].value_or(int64_t(0)), 1);
    CHECK_EQ(doc["input"]["rumble"]["freq_low"].value_or(0.0), 140.0);
    CHECK(doc["input"]["button_mapping"]["touchpad_enabled"].value_or(false));
    CHECK_EQ(size_of(doc["input"]["button_mapping"]["cross"].as_array()), 1);

    CHECK_EQ(doc["cloud"]["datacenter_pscloud"].value_or(std::string()), std::string("lonb"));
    CHECK(doc["cloud"]["attr_passed"].value_or(false));
    const toml::array* favorites = doc["cloud"]["favorites"].as_array();
    CHECK_EQ(size_of(favorites), 2);
    CHECK_EQ(size_of(favorites) == 2 ? (*favorites)[1].value_or(std::string()) : std::string(),
             std::string("EP0002-B"));

    CHECK(doc["network"]["port_guessing"].value_or(false));
    CHECK_EQ(doc["network"]["port_guessing_count"].value_or(int64_t(0)), 75);
    CHECK_EQ(doc["network"]["companion_port"].value_or(int64_t(0)), 8080);

    CHECK_EQ(doc["stream"]["packet_loss_max"].value_or(0.0), 0.035);
    CHECK(!doc["stream"]["auto_reconnect"].value_or(true));
    CHECK(doc["stream"]["sleep_on_exit"].value_or(false));

    CHECK_EQ(doc["ui"]["theme"].value_or(std::string()), std::string("playstation"));
    CHECK(doc["ui"]["hide_account_name"].value_or(false));

    CHECK_EQ(doc["psn"]["request_budget"].value_or(int64_t(0)), 105);

    CHECK_EQ(doc["updates"]["channel"].value_or(std::string()), std::string("rc"));
    CHECK(doc["updates"]["auto_check"].value_or(false));
    CHECK_EQ(doc["updates"]["last_check"].value_or(int64_t(0)), 1785538775);

    CHECK(doc["debug"]["chiaki_log"].value_or(false));
    CHECK(doc["debug"]["file_logging"].value_or(false));
    CHECK(doc["debug"]["power_user_menu_unlocked"].value_or(false));
    CHECK_EQ(doc["debug"]["update_server"].value_or(std::string()),
             std::string("192.168.20.123:8099"));

    // Nothing but the structural keys stays at the top level.
    for (auto&& [k, v] : doc) {
        std::string name(k.str());
        (void)v;
        CHECK(name == "version" || name == "video" || name == "picture" || name == "input" ||
              name == "cloud" || name == "network" || name == "stream" || name == "ui" ||
              name == "psn" || name == "updates" || name == "debug");
    }
}

TEST(migration_explodes_cloud_shortcut_blobs)
{
    static constexpr std::string_view src = R"(
version = 5
active_profile_id = 2
cloud_shortcuts = '[{"productId":"LEGACY-1","name":"Legacy Global","streamServiceType":"psnow"}]'

[[profiles]]
profile_id = 1
cloud_shortcuts = '[{"productId":"PPSA-1","name":"Astro Bot","imageUrl":"https://x/a.png","streamServiceType":"pscloud","streamIdentifier":"CUSA-1","isOwned":true,"plusCatalog":false},{"name":"No Product Id"}]'

[[profiles]]
profile_id = 2
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK(!doc.contains("cloud_shortcuts"));

    const toml::table* p1 = nth(doc, "profiles", 0);
    const toml::array* shortcuts = p1 ? (*p1)["cloud_shortcuts"].as_array() : nullptr;
    CHECK_EQ(size_of(shortcuts), 1);

    const toml::table* astro = nth_in(shortcuts, 0);
    CHECK_EQ(tstr(astro, "product_id"), std::string("PPSA-1"));
    CHECK_EQ(tstr(astro, "name"), std::string("Astro Bot"));
    CHECK_EQ(tstr(astro, "image_url"), std::string("https://x/a.png"));
    CHECK_EQ(tstr(astro, "stream_service_type"), std::string("pscloud"));
    CHECK_EQ(tstr(astro, "stream_identifier"), std::string("CUSA-1"));
    CHECK((*astro)["is_owned"].value_or(false));
    CHECK(!astro->contains("plus_catalog"));

    // The pre-multi-profile global blob lands on the active profile.
    const toml::table* p2 = nth(doc, "profiles", 1);
    const toml::array* adopted = p2 ? (*p2)["cloud_shortcuts"].as_array() : nullptr;
    CHECK_EQ(size_of(adopted), 1);
    CHECK_EQ(tstr(nth_in(adopted, 0), "product_id"), std::string("LEGACY-1"));
}

TEST(migration_splits_cloud_video_by_service)
{
    static constexpr std::string_view src = R"(
version = 5
cloud_video_resolution = 1080
cloud_video_bitrate = 15000
cloud_fsr_enabled = true
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK(!doc["video"]["cloud"].as_table()->contains("resolution"));

    for (const char* service : {"pscloud", "psnow"}) {
        const auto* t = doc["video"]["cloud"][service].as_table();
        CHECK_EQ((*t)["resolution"].value_or(int64_t(0)), 1080);
        CHECK_EQ((*t)["bitrate"].value_or(int64_t(0)), 15000);
        CHECK((*t)["fsr_enabled"].value_or(false));
    }
}

TEST(migration_leaves_split_cloud_video_alone)
{
    static constexpr std::string_view src = R"(
version = 6

[video.cloud.pscloud]
resolution = 1080
bitrate = 15000

[video.cloud.psnow]
resolution = 720
bitrate = 5000
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK_EQ(doc["video"]["cloud"]["pscloud"]["resolution"].value_or(int64_t(0)), 1080);
    CHECK_EQ(doc["video"]["cloud"]["psnow"]["resolution"].value_or(int64_t(0)), 720);
}

TEST(migration_moves_psn_tokens_into_sign_ins)
{
    static constexpr std::string_view src = R"(
version = 7

[[profiles]]
profile_id = 1
online_id = "player"
account_id = "AQAAAAAAAAA="
npsso = "seed"
access_token = "rp-access"
refresh_token = "rp-refresh"
token_expires_at = 1700000000
mobile_sso_access_token = "app-access"
mobile_sso_refresh_token = "app-refresh"
mobile_sso_expires_at = 1700000100
duid = "0000000700410080abcdef"
npsso_valid = true

[[profiles]]
profile_id = 2
online_id = "local"
account_id = "AgAAAAAAAAA="
legacy = true
)";

    toml::table doc = run_migrate(src);

    CHECK_EQ(version_of(doc), 8);
    CHECK_EQ(arr_size(doc, "sign_ins"), 1);
    const toml::table* signIn = nth(doc, "sign_ins", 0);
    CHECK_EQ(tstr(signIn, "account"), "1");
    CHECK_EQ(tstr(signIn, "npsso"), "seed");
    CHECK_EQ(tstr(signIn, "remote_play_token"), "rp-access");
    CHECK_EQ(tstr(signIn, "remote_play_refresh_token"), "rp-refresh");
    CHECK_EQ(tint(signIn, "remote_play_token_expires_at"), int64_t(1700000000000));
    CHECK_EQ(tstr(signIn, "app_token"), "app-access");
    CHECK_EQ(tint(signIn, "app_token_expires_at"), int64_t(1700000100000));
    CHECK_EQ(tstr(signIn, "device_id"), "0000000700410080abcdef");

    const toml::table* profile = nth(doc, "profiles", 0);
    CHECK(!profile->contains("npsso"));
    CHECK(!profile->contains("access_token"));
    CHECK(!profile->contains("duid"));
    CHECK_EQ(tstr(profile, "account_id"), "AQAAAAAAAAA=");
}

TEST(m008_moves_sign_ins_and_pairings_under_their_profiles)
{
    constexpr std::string_view src = R"(
version = 8

[[profiles]]
profile_id = 1
online_id = "first"
account_id = "AQAAAAAAAAA="

[[profiles]]
profile_id = 3
online_id = "third"
account_id = "AwAAAAAAAAA="

[[sign_ins]]
account = "1"
online_id = "first"
npsso = "seed-one"
app_refresh_token = "refresh-one"

[[sign_ins]]
account = "9"
online_id = "gone"
npsso = "seed-orphan"

[[pairings]]
account = "1"
hardware_id = "aabbccddeeff"
rp_key = "a2V5"

[[pairings]]
account = "9"
hardware_id = "aabbccddeeff"
rp_key = "b3JwaGFu"

[[paired_consoles]]
hardware_id = "aabbccddeeff"
name = "PS5"
)";

    toml::table doc = toml::parse(src);
    chiaki_migrations::buildSettingsMigrator().migrate(doc);

    CHECK_EQ(version_of(doc), 10);
    CHECK(!doc.contains("sign_ins"));
    CHECK(!doc.contains("pairings"));
    CHECK_EQ(arr_size(doc, "paired_consoles"), 1);

    const toml::table* first = nth(doc, "profiles", 0);
    const toml::table* signIn = (*first)["sign_in"].as_table();
    CHECK(signIn != nullptr);
    CHECK_EQ(tstr(signIn, "npsso"), "seed-one");
    CHECK_EQ(tstr(signIn, "app_refresh_token"), "refresh-one");
    CHECK(!signIn->contains("account"));
    CHECK(!signIn->contains("online_id"));
    const toml::array* pairings = (*first)["pairings"].as_array();
    CHECK_EQ(size_of(pairings), 1);
    CHECK_EQ(tstr(nth_in(pairings, 0), "rp_key"), "a2V5");
    CHECK(!nth_in(pairings, 0)->contains("account"));

    const toml::table* third = nth(doc, "profiles", 1);
    CHECK(!third->contains("sign_in"));
    CHECK(!third->contains("pairings"));
}

TEST(m009_splits_the_takion_override_by_target)
{
    toml::table doc = toml::parse(R"(
version = 9

[stream]
takion_version = 15
packet_loss_max = 0.05
)");
    chiaki_migrations::buildSettingsMigrator().migrate(doc);

    CHECK_EQ(version_of(doc), 10);
    const toml::table* stream = doc["stream"].as_table();
    CHECK(!stream->contains("takion_version"));
    const toml::table* versions = (*stream)["takion_versions"].as_table();
    CHECK(versions != nullptr);
    for (const char* target : {"ps4", "ps5", "ps5_cloud", "ps_now"})
        CHECK_EQ(tint(versions, target), int64_t(15));

    toml::table untouched = toml::parse("version = 9\n[stream]\npacket_loss_max = 0.05\n");
    chiaki_migrations::buildSettingsMigrator().migrate(untouched);
    CHECK(!untouched["stream"].as_table()->contains("takion_versions"));
}
