#include "test_util.hpp"
#include "network/sony_dns_checker.hpp"

#include <vector>

using akira::net::DnsVerdict;

static std::vector<uint8_t> dnsAnswer(const std::vector<uint8_t>& addresses)
{
    const uint16_t answers = static_cast<uint16_t>(addresses.size() / 4);
    std::vector<uint8_t> packet{
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01,
        static_cast<uint8_t>(answers >> 8), static_cast<uint8_t>(answers),
        0, 0, 0, 0,
        1, 'x', 0, 0, 1, 0, 1,
    };
    for (size_t i = 0; i < addresses.size(); i += 4) {
        const uint8_t answer[] = {
            0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4,
            addresses[i], addresses[i + 1], addresses[i + 2], addresses[i + 3],
        };
        packet.insert(packet.end(), std::begin(answer), std::end(answer));
    }
    return packet;
}

TEST(sony_dns_classifies_loopback_and_null_answers_as_blocked)
{
    auto loopback = dnsAnswer({127, 0, 0, 1});
    auto nullAddress = dnsAnswer({0, 0, 0, 0});
    CHECK(akira::net::classifyDnsAResponse(loopback.data(), loopback.size(), 0x1234)
        == DnsVerdict::Blocked);
    CHECK(akira::net::classifyDnsAResponse(nullAddress.data(), nullAddress.size(), 0x1234)
        == DnsVerdict::Blocked);
}

TEST(sony_dns_prefers_reachable_answer_over_mixed_poisoned_answer)
{
    auto reachable = dnsAnswer({1, 2, 3, 4});
    auto mixed = dnsAnswer({127, 0, 0, 1, 1, 2, 3, 4});
    CHECK(akira::net::classifyDnsAResponse(reachable.data(), reachable.size(), 0x1234)
        == DnsVerdict::Reachable);
    CHECK(akira::net::classifyDnsAResponse(mixed.data(), mixed.size(), 0x1234)
        == DnsVerdict::Reachable);
}

TEST(sony_dns_rejects_malformed_or_unrelated_responses)
{
    auto packet = dnsAnswer({127, 0, 0, 1});
    CHECK(akira::net::classifyDnsAResponse(packet.data(), packet.size(), 0x1235)
        == DnsVerdict::Inconclusive);
    CHECK(akira::net::classifyDnsAResponse(packet.data(), packet.size() - 1, 0x1234)
        == DnsVerdict::Inconclusive);
    packet[2] = 0x83; // Truncated response
    CHECK(akira::net::classifyDnsAResponse(packet.data(), packet.size(), 0x1234)
        == DnsVerdict::Inconclusive);
}

TEST(sony_dns_warns_if_any_domain_fails_on_either_server)
{
    akira::net::DnsServerStatus status{"207.246.121.77",
        {DnsVerdict::Blocked, DnsVerdict::Inconclusive, DnsVerdict::Reachable}};
    CHECK(status.hasSonyFailure());
    CHECK_EQ(status.failedCount(), 2);
    akira::net::SonyDnsReport report{{status}};
    CHECK(report.failed());
    status.domains = {DnsVerdict::Reachable, DnsVerdict::Reachable, DnsVerdict::Reachable};
    CHECK(!status.hasSonyFailure());
    CHECK(!akira::net::SonyDnsReport{{status}}.failed());
    status.domains[1] = DnsVerdict::Inconclusive;
    CHECK(status.hasSonyFailure());
}

TEST(sony_dns_warning_ok_unlocks_only_after_five_seconds)
{
    CHECK_EQ(akira::net::secondsUntilDnsWarningOk(0), 5);
    CHECK_EQ(akira::net::secondsUntilDnsWarningOk(999), 5);
    CHECK_EQ(akira::net::secondsUntilDnsWarningOk(1000), 4);
    CHECK_EQ(akira::net::secondsUntilDnsWarningOk(4999), 1);
    CHECK_EQ(akira::net::secondsUntilDnsWarningOk(5000), 0);
    CHECK_EQ(akira::net::secondsUntilDnsWarningOk(5001), 0);
}
