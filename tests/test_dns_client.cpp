#include "test_util.hpp"
#include "util/dns_client.hpp"

#include <array>
#include <span>
#include <string>
#include <utility>
#include <vector>

using akira::net::DnsClient;
using akira::net::Ipv4Address;

static std::vector<uint8_t> answerPacket(uint16_t flags, std::vector<std::pair<std::array<uint8_t, 4>, uint32_t>> answers)
{
    std::vector<uint8_t> packet{
        0x12, 0x34, static_cast<uint8_t>(flags >> 8), static_cast<uint8_t>(flags), 0x00, 0x01,
        0x00, static_cast<uint8_t>(answers.size()), 0, 0, 0, 0,
        3, 'm', 'n', 'p', 3, 'c', 'o', 'm', 0, 0, 1, 0, 1,
    };
    for (const auto& [address, ttl] : answers) {
        const uint8_t record[] = {
            0xc0, 0x0c, 0, 1, 0, 1,
            static_cast<uint8_t>(ttl >> 24), static_cast<uint8_t>(ttl >> 16), static_cast<uint8_t>(ttl >> 8), static_cast<uint8_t>(ttl),
            0, 4, address[0], address[1], address[2], address[3],
        };
        packet.insert(packet.end(), std::begin(record), std::end(record));
    }
    return packet;
}

TEST(dns_client_encodes_a_question_with_recursion)
{
    const auto query = DnsClient::encodeQuery(0xbeef, "m.np.playstation.com.");
    REQUIRE(query.has_value());
    const std::vector<uint8_t> expected{
        0xbe, 0xef, 0x01, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0,
        1, 'm', 2, 'n', 'p', 11, 'p', 'l', 'a', 'y', 's', 't', 'a', 't', 'i', 'o', 'n', 3, 'c', 'o', 'm', 0,
        0, 1, 0, 1,
    };
    CHECK(*query == expected);
}

TEST(dns_client_rejects_bad_names)
{
    CHECK(!DnsClient::encodeQuery(1, "").has_value());
    CHECK(!DnsClient::encodeQuery(1, "a..b").has_value());
    CHECK(!DnsClient::encodeQuery(1, std::string(64, 'a') + ".com").has_value());
}

TEST(dns_client_reads_a_records_and_ttls)
{
    const auto packet = answerPacket(0x8180, {{{104, 18, 32, 7}, 300}, {{104, 18, 33, 7}, 60}});
    const auto records = DnsClient::parseResponse(packet, 0x1234);
    REQUIRE(records.has_value());
    REQUIRE(records->size() == 2);
    CHECK((*records)[0].address == *Ipv4Address::parse("104.18.32.7"));
    CHECK((*records)[1].address == *Ipv4Address::parse("104.18.33.7"));
    CHECK((*records)[0].ttl.count() == 300);
    CHECK((*records)[1].ttl.count() == 60);
}

TEST(dns_client_maps_failures)
{
    CHECK(DnsClient::parseResponse(answerPacket(0x8183, {}), 0x1234).error() == DnsClient::Step::NoSuchName);
    CHECK(DnsClient::parseResponse(answerPacket(0x8182, {}), 0x1234).error() == DnsClient::Step::ServerFailure);
    CHECK(DnsClient::parseResponse(answerPacket(0x8380, {}), 0x1234).error() == DnsClient::Step::Truncated);
    CHECK(DnsClient::parseResponse(answerPacket(0x8180, {}), 0x4321).error() == DnsClient::Step::Malformed);
    const auto cut = answerPacket(0x8180, {{{1, 2, 3, 4}, 30}});
    CHECK(DnsClient::parseResponse(std::span(cut).first(cut.size() - 1), 0x1234).error() == DnsClient::Step::Malformed);
}

TEST(dns_client_returns_no_records_for_an_empty_answer)
{
    const auto records = DnsClient::parseResponse(answerPacket(0x8180, {}), 0x1234);
    REQUIRE(records.has_value());
    CHECK(records->empty());
}
