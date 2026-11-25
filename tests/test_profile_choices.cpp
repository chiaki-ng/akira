#include "test_util.hpp"

#include "accounts/profile_choices.hpp"
#include "app/app_settings.hpp"
#include "settings/settings_store.hpp"

#include <filesystem>

using akira::accounts::Profile;
using akira::accounts::ProfileChoices;

namespace {

Profile profile(int64_t id, std::string onlineId, std::string accountId)
{
    Profile entry;
    entry.id = id;
    entry.onlineId = std::move(onlineId);
    entry.accountId = std::move(accountId);
    return entry;
}

}

TEST(profile_choices_label_and_select_the_active_profile)
{
    akira::settings::SettingsStore store(std::filesystem::temp_directory_path() / "akira-profile-choices");
    akira::app::AppSettings app(store);
    const std::vector<Profile> profiles{profile(4, "player_one", ""), profile(7, "", "QUJD"), profile(9, "", "")};

    ProfileChoices plain(profiles, 7, app);
    REQUIRE(plain.choices().size() == 3u);
    CHECK(plain.choices()[0].label == std::optional<std::string>("player_one"));
    CHECK(plain.choices()[1].label == std::optional<std::string>("QUJD"));
    CHECK(!plain.choices()[2].label.has_value());
    CHECK_EQ(plain.selected(), 1);
    CHECK(plain.idAt(2) == std::optional<int64_t>(9));
    CHECK(!plain.idAt(3).has_value());
    CHECK(!plain.idAt(-1).has_value());

    app.setHideAccountName(true);
    ProfileChoices hidden(profiles, 99, app);
    CHECK(hidden.choices()[0].label == std::optional<std::string>("pl***"));
    CHECK_EQ(hidden.selected(), 0);
}
