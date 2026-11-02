#pragma once

#include <tomlmigrate/tomlmigrate.hpp>

#include "m000_baseline.hpp"
#include "m001_unify_psn_auth.hpp"
#include "m002_multi_profile.hpp"
#include "m003_drop_hardened_nat.hpp"
#include "m004_cloud_datacenter_tables.hpp"
#include "m005_group_settings_tables.hpp"
#include "m006_split_cloud_video_by_service.hpp"
#include "m007_split_psn_sign_ins.hpp"
#include "m008_nest_account_data_in_profiles.hpp"
#include "m009_split_takion_version_by_target.hpp"

namespace chiaki_migrations {

inline tomlmigrate::Migrator buildSettingsMigrator() {
    tomlmigrate::Options opts;
    opts.version_key = "version";
    opts.assume_missing_version = 0;

    tomlmigrate::Migrator m(opts);
    register_m000_baseline(m);
    register_m001_unify_psn_auth(m);
    register_m002_multi_profile(m);
    register_m003_drop_hardened_nat(m);
    register_m004_cloud_datacenter_tables(m);
    register_m005_group_settings_tables(m);
    register_m006_split_cloud_video_by_service(m);
    register_m007_split_psn_sign_ins(m);
    register_m008_nest_account_data_in_profiles(m);
    register_m009_split_takion_version_by_target(m);
    return m;
}

}
