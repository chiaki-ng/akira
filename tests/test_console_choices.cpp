#include "test_util.hpp"

#include "consoles/console_pin.hpp"
#include "consoles/psn_identity.hpp"
#include "consoles/takion_pin_choice.hpp"

#include <array>
#include <cstdint>

using akira::consoles::ConsolePin;
using akira::consoles::PsnIdentity;
using akira::consoles::TakionPinChoice;

TEST(console_pin_takes_four_digits_or_nothing)
{
    CHECK(ConsolePin::parse("1234").has_value());
    CHECK(ConsolePin::parse("0000").has_value());
    CHECK(ConsolePin::parse("").has_value());
    CHECK(!ConsolePin::parse("123").has_value());
    CHECK(!ConsolePin::parse("12345").has_value());
    CHECK(!ConsolePin::parse("12a4").has_value());
    CHECK_EQ(ConsolePin::parse("0420")->text(), std::string("0420"));
}

TEST(takion_pin_choice_selects_the_pinned_version)
{
    const std::array<std::uint32_t, 3> versions{12, 14, 20};
    CHECK_EQ(TakionPinChoice(versions, std::nullopt, 0).selected(), 0);
    CHECK_EQ(TakionPinChoice(versions, 14u, 0).selected(), 2);
    CHECK_EQ(TakionPinChoice(versions, 9u, 0).selected(), 0);
}

TEST(takion_pin_choice_maps_rows_to_versions)
{
    const std::array<std::uint32_t, 2> versions{9, 12};
    TakionPinChoice choice(versions, std::nullopt, 0);
    CHECK(choice.contains(0));
    CHECK(choice.contains(2));
    CHECK(!choice.contains(3));
    CHECK(!choice.contains(-1));
    CHECK(!choice.versionAt(0).has_value());
    CHECK(choice.versionAt(1) == std::optional<std::uint32_t>(9));
    CHECK(choice.versionAt(2) == std::optional<std::uint32_t>(12));
    CHECK(!choice.versionAt(3).has_value());
}

TEST(takion_pin_choice_reports_the_configured_fallback)
{
    const std::array<std::uint32_t, 1> versions{20};
    CHECK(!TakionPinChoice(versions, std::nullopt, 0).configuredVersion().has_value());
    CHECK(!TakionPinChoice(versions, std::nullopt, -1).configuredVersion().has_value());
    CHECK(TakionPinChoice(versions, std::nullopt, 15).configuredVersion() == std::optional<std::uint32_t>(15));
}

TEST(psn_identity_names_what_a_console_lacks)
{
    CHECK(PsnIdentity("", "player").missingFor(true) == PsnIdentity::Missing::AccountId);
    CHECK(!PsnIdentity("abc=", "").missingFor(true).has_value());
    CHECK(PsnIdentity("abc=", "").missingFor(false) == PsnIdentity::Missing::OnlineId);
    CHECK(!PsnIdentity("", "player").missingFor(false).has_value());
}
