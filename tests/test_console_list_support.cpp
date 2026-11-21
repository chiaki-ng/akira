#include "test_util.hpp"

#include "accounts/account_settings.hpp"
#include "accounts/profile_switch.hpp"
#include "consoles/console_list.hpp"
#include "consoles/debug_consoles.hpp"
#include "consoles/discovery_gate.hpp"
#include "consoles/remote_lookup.hpp"
#include "settings/settings_store.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using akira::accounts::SessionState;
using akira::consoles::ConsoleList;
using akira::consoles::DebugConsoles;
using akira::consoles::DiscoveryGate;
using akira::consoles::HostType;
using akira::consoles::RemoteLookup;

TEST(console_list_profile_follows_profiles_and_legacy)
{
    CHECK(ConsoleList::profileFor(false, false) == ConsoleList::Profile::None);
    CHECK(ConsoleList::profileFor(false, true) == ConsoleList::Profile::None);
    CHECK(ConsoleList::profileFor(true, true) == ConsoleList::Profile::Legacy);
    CHECK(ConsoleList::profileFor(true, false) == ConsoleList::Profile::Signed);
}

TEST(discovery_gate_updates_only_while_shown_and_idle)
{
    DiscoveryGate gate;
    CHECK(!gate.open());
    CHECK(gate.discovered(false, true) == DiscoveryGate::Discovered::NoHost);
    CHECK(gate.discovered(true, true) == DiscoveryGate::Discovered::Suspended);
    CHECK(!gate.hostsChanged(true));

    gate.appeared();
    CHECK(gate.shown());
    CHECK(gate.discovered(true, true) == DiscoveryGate::Discovered::Update);
    CHECK(gate.discovered(true, false) == DiscoveryGate::Discovered::NoList);
    CHECK(gate.hostsChanged(true));
    CHECK(!gate.hostsChanged(false));

    gate.connectionStarted();
    CHECK(gate.shown());
    CHECK(gate.discovered(true, true) == DiscoveryGate::Discovered::Suspended);
    CHECK(!gate.hostsChanged(true));

    gate.connectionEnded();
    CHECK(gate.discovered(true, true) == DiscoveryGate::Discovered::Update);
    gate.disappeared();
    CHECK(gate.discovered(true, true) == DiscoveryGate::Discovered::Suspended);
}

TEST(remote_lookup_needs_a_linked_session)
{
    CHECK(!RemoteLookup(std::nullopt).allowed());
    CHECK(RemoteLookup(std::nullopt).notice() == RemoteLookup::Notice::NoToken);
    CHECK(!RemoteLookup(SessionState::NotLinked).allowed());
    CHECK(RemoteLookup(SessionState::Valid).allowed());
    CHECK(RemoteLookup(SessionState::Valid).notice() == RemoteLookup::Notice::Finding);
    CHECK(RemoteLookup(SessionState::Expired).notice() == RemoteLookup::Notice::RefreshingToken);
    CHECK(RemoteLookup(SessionState::Refreshing).notice() == RemoteLookup::Notice::RefreshingToken);
}

TEST(debug_consoles_alternate_type_and_rest_every_fourth)
{
    const auto consoles = DebugConsoles::list();
    REQUIRE(consoles.size() == 20u);
    CHECK_EQ(consoles.front().nickname, std::string("PS5-Debug-1"));
    CHECK_EQ(consoles.back().nickname, std::string("PS5-Debug-20"));
    CHECK(consoles[0].type == HostType::Manual);
    CHECK(consoles[1].type == HostType::Auto);
    CHECK(!consoles[2].resting);
    CHECK(consoles[3].resting);
    CHECK(consoles[19].resting);
}

TEST(debug_consoles_list_as_paired_ps5_cards)
{
    const auto facts = DebugConsoles::facts();
    REQUIRE(facts.size() == static_cast<std::size_t>(DebugConsoles::Count));
    CHECK_EQ(facts.front().catalogName, std::string("PS5-Debug-1"));
    CHECK(facts.front().paired);
    CHECK(facts.front().ps5);
    CHECK(facts.front().ready);
    CHECK(facts[3].standby);
    CHECK(!facts[3].ready);
    CHECK(facts[1].type == HostType::Auto);
    const ConsoleList list(facts, ConsoleList::Profile::Signed);
    CHECK_EQ(list.shown().size(), facts.size());
}

TEST(debug_consoles_own_one_host_per_console_in_order)
{
    std::vector<std::string> made;
    const DebugConsoles consoles([&made](const DebugConsoles::Console& console) {
        made.push_back(console.nickname);
        return std::shared_ptr<akira::consoles::Host>();
    });
    CHECK_EQ(consoles.hosts().size(), static_cast<std::size_t>(DebugConsoles::Count));
    REQUIRE(made.size() == static_cast<std::size_t>(DebugConsoles::Count));
    CHECK_EQ(made.front(), std::string("PS5-Debug-1"));
    CHECK_EQ(made.back(), std::string("PS5-Debug-20"));
}

TEST(profile_switch_activates_saves_and_announces)
{
    const auto directory = std::filesystem::temp_directory_path() / "akira-profile-switch";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    akira::settings::SettingsStore store(directory);
    akira::accounts::AccountSettings accounts(store, [](bool) {});
    akira::accounts::Profile profile;
    const int64_t id = accounts.addProfile(profile);

    std::vector<int64_t> seen;
    akira::accounts::ProfileSwitch(accounts, store, [&] { seen.push_back(accounts.activeProfileId()); }).to(id);

    CHECK_EQ(accounts.activeProfileId(), id);
    REQUIRE(seen.size() == 1u);
    CHECK_EQ(seen.front(), id);
    std::filesystem::remove_all(directory);
}
