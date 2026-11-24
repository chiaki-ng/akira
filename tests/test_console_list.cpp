#include "test_util.hpp"

#include "consoles/console_list.hpp"

#include <string>
#include <vector>

using akira::consoles::ConsoleEntry;
using akira::consoles::ConsoleFacts;
using akira::consoles::ConsoleList;
using akira::consoles::HostType;

namespace {

ConsoleFacts console(std::string name, HostType type)
{
    ConsoleFacts facts;
    facts.catalogName = name;
    facts.name = std::move(name);
    facts.type = type;
    facts.ps5 = true;
    facts.presence = "Unknown";
    return facts;
}

ConsoleFacts paired(std::string name, HostType type)
{
    ConsoleFacts facts = console(std::move(name), type);
    facts.paired = true;
    return facts;
}

std::vector<std::string> names(const std::vector<ConsoleEntry>& entries)
{
    std::vector<std::string> out;
    for (const auto& entry : entries)
        out.push_back(entry.facts().catalogName);
    return out;
}

}

TEST(console_list_shows_nothing_without_a_profile)
{
    ConsoleList list({paired("A", HostType::Discovered)}, ConsoleList::Profile::None);
    CHECK(list.shown().empty());
    CHECK(!list.hasProfile());
}

TEST(console_list_hides_unknown_consoles_and_keeps_order)
{
    ConsoleFacts discovered = console("B", HostType::Discovered);
    discovered.discovered = true;
    ConsoleList list({paired("A", HostType::Auto), discovered, console("C", HostType::Discovered),
                      console("D", HostType::Manual)},
                     ConsoleList::Profile::Signed);
    CHECK(names(list.shown()) == std::vector<std::string>({"A", "B", "D"}));
}

TEST(console_list_hides_remote_consoles_on_a_legacy_profile)
{
    std::vector<ConsoleFacts> consoles{paired("Home", HostType::Discovered), paired("Home (Remote)", HostType::Remote)};
    CHECK(names(ConsoleList(consoles, ConsoleList::Profile::Legacy).shown()) == std::vector<std::string>({"Home"}));
    CHECK(ConsoleList(consoles, ConsoleList::Profile::Signed).shown().front().routes().size() == 2u);
}

TEST(console_list_links_only_to_paired_local_consoles)
{
    ConsoleList list({paired("Home", HostType::Discovered), paired("Far (Remote)", HostType::Remote),
                      console("New", HostType::Discovered)},
                     ConsoleList::Profile::Signed);
    CHECK(list.linkTargets() == std::vector<std::string>({"Home"}));
}

TEST(console_entry_strips_remote_suffix)
{
    CHECK_EQ(ConsoleEntry(paired("Living Room (Remote)", HostType::Remote), false).displayName(),
             std::string("Living Room"));
    CHECK_EQ(ConsoleEntry(paired("Living Room (Remote)", HostType::Manual), false).displayName(),
             std::string("Living Room (Remote)"));
    CHECK_EQ(ConsoleEntry(paired(" (Remote)", HostType::Remote), false).displayName(), std::string(" (Remote)"));
}

TEST(console_entry_badges)
{
    CHECK(ConsoleEntry(console("a", HostType::Remote), false).badge() == ConsoleEntry::Badge::Remote);
    CHECK(ConsoleEntry(console("a", HostType::Manual), false).badge() == ConsoleEntry::Badge::Manual);
    CHECK(ConsoleEntry(console("a", HostType::Auto), false).badge() == ConsoleEntry::Badge::Auto);
    CHECK(ConsoleEntry(console("a", HostType::Discovered), false).badge() == ConsoleEntry::Badge::Local);
}

TEST(console_entry_remote_needing_a_link_offers_link)
{
    ConsoleFacts remote = console("Far (Remote)", HostType::Remote);
    remote.needsLink = true;
    remote.canPairOverInternet = true;
    ConsoleEntry withTargets(remote, true);
    CHECK(withTargets.status() == ConsoleEntry::Status::Link);
    CHECK(withTargets.primaryAction() == ConsoleEntry::Action::Link);
    CHECK(withTargets.dimmed());

    ConsoleEntry noTargets(remote, false);
    CHECK(noTargets.primaryAction() != ConsoleEntry::Action::Link);

    remote.canPairOverInternet = false;
    CHECK(ConsoleEntry(remote, false).primaryAction() == ConsoleEntry::Action::Link);
}

TEST(console_entry_remote_ps4_connects_over_internet_pairing)
{
    ConsoleFacts ps4 = console("Far (Remote)", HostType::Remote);
    ps4.ps5 = false;
    ConsoleEntry entry(ps4, false);
    CHECK(entry.status() == ConsoleEntry::Status::Ready);
    CHECK(!entry.dimmed());
    CHECK(entry.primaryAction() == ConsoleEntry::Action::Connect);
}

TEST(console_entry_unpaired_discovered_offers_register)
{
    ConsoleFacts found = console("New", HostType::Discovered);
    found.discovered = true;
    ConsoleEntry entry(found, false);
    CHECK(entry.status() == ConsoleEntry::Status::Register);
    CHECK(entry.primaryAction() == ConsoleEntry::Action::Register);

    ConsoleFacts unseen = console("Gone", HostType::Discovered);
    CHECK(ConsoleEntry(unseen, false).status() == ConsoleEntry::Status::Presence);
    CHECK(!ConsoleEntry(unseen, false).primaryAction().has_value());
    unseen.canPairOverInternet = true;
    CHECK(ConsoleEntry(unseen, false).primaryAction() == ConsoleEntry::Action::Register);
}

TEST(console_entry_standby_wakes_only_at_home)
{
    ConsoleFacts sleeping = paired("Home", HostType::Discovered);
    sleeping.standby = true;
    CHECK(ConsoleEntry(sleeping, false).status() == ConsoleEntry::Status::Standby);
    CHECK(ConsoleEntry(sleeping, false).primaryAction() == ConsoleEntry::Action::Wake);

    ConsoleFacts remote = paired("Far (Remote)", HostType::Remote);
    remote.standby = true;
    CHECK(ConsoleEntry(remote, false).status() == ConsoleEntry::Status::Standby);
    CHECK(ConsoleEntry(remote, false).primaryAction() == ConsoleEntry::Action::Connect);
}

TEST(console_entry_paired_and_ready_is_ready)
{
    ConsoleFacts ready = paired("Home", HostType::Discovered);
    ready.ready = true;
    CHECK(ConsoleEntry(ready, false).status() == ConsoleEntry::Status::Ready);
    CHECK(ConsoleEntry(ready, false).primaryAction() == ConsoleEntry::Action::Connect);

    ConsoleFacts unknown = paired("Home", HostType::Discovered);
    CHECK(ConsoleEntry(unknown, false).status() == ConsoleEntry::Status::Presence);
    CHECK(ConsoleEntry(unknown, false).primaryAction() == ConsoleEntry::Action::Connect);
    CHECK(ConsoleEntry(paired("Far (Remote)", HostType::Remote), false).status() == ConsoleEntry::Status::Ready);
}

TEST(console_entry_options_follow_pairing_and_route)
{
    using Option = ConsoleEntry::Option;
    ConsoleFacts home = paired("Home", HostType::Discovered);
    home.hasAddress = true;
    CHECK(ConsoleEntry(home, false).options()
          == std::vector<Option>({Option::ConsolePin, Option::SetAddress, Option::TakionVersion, Option::Delete}));

    CHECK(ConsoleEntry(console("New", HostType::Discovered), false).options()
          == std::vector<Option>({Option::AddAddress}));

    CHECK(ConsoleEntry(paired("Far (Remote)", HostType::Remote), false).options()
          == std::vector<Option>({Option::AddAddress, Option::TakionVersion, Option::Delete}));

    ConsoleFacts cloud = paired("Cloud", HostType::Discovered);
    cloud.cloud = true;
    CHECK(ConsoleEntry(cloud, false).options() == std::vector<Option>({Option::ConsolePin, Option::Delete}));
}

TEST(console_list_folds_the_internet_half_into_its_home_console)
{
    ConsoleFacts home = paired("PS5", HostType::Discovered);
    home.discovered = true;
    ConsoleFacts internet = paired("PS5 (Remote)", HostType::Remote);
    internet.discovered = true;
    ConsoleList list({home, internet}, ConsoleList::Profile::Signed);
    const auto shown = list.shown();
    CHECK(names(shown) == std::vector<std::string>({"PS5"}));
    CHECK(shown.front().routes()
        == std::vector<ConsoleEntry::Route>({ConsoleEntry::Route::Home, ConsoleEntry::Route::Internet}));
    CHECK(shown.front().preferredRoute() == ConsoleEntry::Route::Home);
    CHECK(shown.front().over(ConsoleEntry::Route::Internet).facts().catalogName == "PS5 (Remote)");
}

TEST(console_list_prefers_the_internet_when_the_console_is_not_home)
{
    ConsoleList list({paired("PS5", HostType::Discovered), paired("PS5 (Remote)", HostType::Remote)},
                     ConsoleList::Profile::Signed);
    const auto shown = list.shown();
    CHECK(names(shown) == std::vector<std::string>({"PS5"}));
    CHECK(shown.front().preferredRoute() == ConsoleEntry::Route::Internet);
}

TEST(console_list_keeps_an_unpaired_internet_console_apart)
{
    ConsoleFacts internet = console("PS5 (Remote)", HostType::Remote);
    internet.discovered = true;
    internet.needsLink = true;
    ConsoleList list({paired("Other", HostType::Discovered), internet}, ConsoleList::Profile::Signed);
    CHECK(names(list.shown()) == std::vector<std::string>({"Other", "PS5 (Remote)"}));
    CHECK(list.shown().front().routes() == std::vector<ConsoleEntry::Route>({ConsoleEntry::Route::Home}));
}

TEST(console_list_offers_only_home_on_a_legacy_profile)
{
    ConsoleList list({paired("PS5", HostType::Discovered), paired("PS5 (Remote)", HostType::Remote)},
                     ConsoleList::Profile::Legacy);
    const auto shown = list.shown();
    CHECK(names(shown) == std::vector<std::string>({"PS5"}));
    CHECK(shown.front().routes() == std::vector<ConsoleEntry::Route>({ConsoleEntry::Route::Home}));
}
