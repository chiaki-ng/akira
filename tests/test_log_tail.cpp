#include "test_util.hpp"

#include "log/log_tail.hpp"

#include <string>
#include <vector>

using akira::log::LogTail;

TEST(log_tail_keeps_the_newest_lines)
{
    LogTail tail(2, LogTail::Stamp::None);
    tail.push("one");
    tail.push("two");
    tail.push("three");
    CHECK(tail.lines() == std::vector<std::string>({"two", "three"}));
}

TEST(log_tail_masks_and_stamps)
{
    LogTail tail(5, LogTail::Stamp::Clock);
    tail.mask("player_one", "pl***");
    tail.mask("", "x");
    tail.push("signed in as player_one, player_one again");
    const auto lines = tail.lines();
    REQUIRE(lines.size() == 1u);
    CHECK(lines[0].ends_with(" signed in as pl***, pl*** again"));
    CHECK_EQ(lines[0].size(), std::string("00:00:00.000 signed in as pl***, pl*** again").size());
    CHECK_EQ(lines[0][2], ':');
}
