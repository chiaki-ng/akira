#include "test_util.hpp"

#include "consoles/console_address.hpp"

using akira::consoles::ConsoleAddress;

TEST(console_address_accepts_ipv4_and_names)
{
    CHECK(ConsoleAddress::parse("192.168.1.20").has_value());
    CHECK(ConsoleAddress::parse("0.0.0.0").has_value());
    CHECK(ConsoleAddress::parse("ps5.home.example").has_value());
    CHECK(ConsoleAddress::parse("my-ps5.lan").has_value());
    CHECK_EQ(ConsoleAddress::parse("10.0.0.7")->text(), std::string("10.0.0.7"));
}

TEST(console_address_rejects_malformed)
{
    CHECK(!ConsoleAddress::parse("").has_value());
    CHECK(!ConsoleAddress::parse("192.168.50.266").has_value());
    CHECK(!ConsoleAddress::parse("192.168.1").has_value());
    CHECK(!ConsoleAddress::parse("localhost").has_value());
    CHECK(!ConsoleAddress::parse(".example.com").has_value());
    CHECK(!ConsoleAddress::parse("example.com.").has_value());
    CHECK(!ConsoleAddress::parse("-ps5.lan").has_value());
    CHECK(!ConsoleAddress::parse("ps5 lan.com").has_value());
    CHECK(!ConsoleAddress::parse(std::string(254, 'a') + ".com").has_value());
}
