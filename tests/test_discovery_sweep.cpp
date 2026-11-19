#include "test_util.hpp"

#include "consoles/discovery_sweep.hpp"

#include <arpa/inet.h>
#include <cstdint>
#include <string>

using namespace akira::discovery;

namespace {

uint32_t ipNet(const char* s)
{
    struct in_addr ina = {};
    inet_pton(AF_INET, s, &ina);
    return ina.s_addr;
}

uint32_t ipHost(const char* s)
{
    return ntohl(ipNet(s));
}

std::string netToStr(uint32_t sAddrNet)
{
    struct in_addr ina = {};
    ina.s_addr = sAddrNet;
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &ina, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace

TEST(sweep_parse_accepts_24_to_32)
{
    uint32_t base = 0;
    int prefix = 0;

    CHECK(parseSweepCidr("192.168.50.0/24", base, prefix));
    CHECK_EQ(base, ipHost("192.168.50.0"));
    CHECK_EQ(prefix, 24);

    CHECK(parseSweepCidr("192.168.50.99/24", base, prefix));
    CHECK_EQ(base, ipHost("192.168.50.0"));

    CHECK(parseSweepCidr("192.168.50.99/27", base, prefix));
    CHECK_EQ(base, ipHost("192.168.50.96"));
    CHECK_EQ(prefix, 27);

    CHECK(parseSweepCidr("192.168.50.99/32", base, prefix));
    CHECK_EQ(base, ipHost("192.168.50.99"));
    CHECK_EQ(prefix, 32);

    CHECK(parseSweepCidr("10.0.0.200/25", base, prefix));
    CHECK_EQ(base, ipHost("10.0.0.128"));
}

TEST(sweep_parse_rejects_bigger_and_malformed)
{
    uint32_t base = 0;
    int prefix = 0;
    CHECK(!parseSweepCidr("192.168.50.0/23", base, prefix));
    CHECK(!parseSweepCidr("192.168.0.0/16", base, prefix));
    CHECK(!parseSweepCidr("192.168.50.0/0", base, prefix));
    CHECK(!parseSweepCidr("192.168.50.0/33", base, prefix));
    CHECK(!parseSweepCidr("192.168.50.99", base, prefix));
    CHECK(!parseSweepCidr("192.168.50.0/", base, prefix));
    CHECK(!parseSweepCidr("192.168.50.0/2x", base, prefix));
    CHECK(!parseSweepCidr("not.an.ip/24", base, prefix));
    CHECK(!parseSweepCidr("", base, prefix));
}

TEST(sweep_is_valid_matches_parse)
{
    CHECK(isValidSweepCidr("192.168.1.0/24"));
    CHECK(isValidSweepCidr("192.168.50.96/27"));
    CHECK(isValidSweepCidr("192.168.50.99/32"));
    CHECK(!isValidSweepCidr("192.168.1.5"));
    CHECK(!isValidSweepCidr("10.0.0.0/8"));
    CHECK(!isValidSweepCidr("192.168.1.0/23"));
}

TEST(sweep_normalize)
{
    CHECK_EQ(normalizeSweepCidr("192.168.50.99/24"), std::string("192.168.50.0/24"));
    CHECK_EQ(normalizeSweepCidr("192.168.50.99/27"), std::string("192.168.50.96/27"));
    CHECK_EQ(normalizeSweepCidr("192.168.50.0/24"), std::string("192.168.50.0/24"));
    CHECK_EQ(normalizeSweepCidr("10.20.30.254/25"), std::string("10.20.30.128/25"));
    CHECK_EQ(normalizeSweepCidr("bogus"), std::string("bogus"));
}

TEST(sweep_prefix_mask_and_count)
{
    CHECK_EQ(prefixMask(24), 0xFFFFFF00u);
    CHECK_EQ(prefixMask(27), 0xFFFFFFE0u);
    CHECK_EQ(prefixMask(32), 0xFFFFFFFFu);

    CHECK_EQ(subnetAddrCount(24), 256u);
    CHECK_EQ(subnetAddrCount(27), 32u);
    CHECK_EQ(subnetAddrCount(25), 128u);
    CHECK_EQ(subnetAddrCount(32), 1u);
}

TEST(sweep_host_offsets_and_count)
{
    uint32_t first = 0, last = 0;

    sweepHostOffsets(24, first, last);
    CHECK_EQ(first, 1u);
    CHECK_EQ(last, 254u);
    CHECK_EQ(sweepHostCount(24), 254u);

    sweepHostOffsets(27, first, last);
    CHECK_EQ(first, 1u);
    CHECK_EQ(last, 30u);
    CHECK_EQ(sweepHostCount(27), 30u);

    sweepHostOffsets(31, first, last);
    CHECK_EQ(first, 0u);
    CHECK_EQ(last, 1u);
    CHECK_EQ(sweepHostCount(31), 2u);

    sweepHostOffsets(32, first, last);
    CHECK_EQ(first, 0u);
    CHECK_EQ(last, 0u);
    CHECK_EQ(sweepHostCount(32), 1u);
}

TEST(sweep_contains_local)
{
    uint32_t local = ipNet("192.168.20.116");

    CHECK(subnetContainsLocal(local, ipHost("192.168.20.0"), 24));
    CHECK(!subnetContainsLocal(local, ipHost("192.168.50.0"), 24));

    CHECK(subnetContainsLocal(local, ipHost("192.168.20.96"), 27));
    CHECK(!subnetContainsLocal(local, ipHost("192.168.20.0"), 27));
}

TEST(sweep_chunk_count)
{
    CHECK_EQ(sweepChunkCount(254, 64), 4);
    CHECK_EQ(sweepChunkCount(30, 64), 1);
    CHECK_EQ(sweepChunkCount(128, 64), 2);
    CHECK_EQ(sweepChunkCount(0, 64), 0);
    CHECK_EQ(sweepChunkCount(64, 64), 1);
    CHECK_EQ(sweepChunkCount(65, 64), 2);
}

TEST(sweep_host_addr)
{
    uint32_t base = 0;
    int prefix = 0;
    CHECK(parseSweepCidr("192.168.50.0/24", base, prefix));

    CHECK_EQ(sweepAddrNet(base, 99), ipNet("192.168.50.99"));
    CHECK_EQ(sweepAddrNet(base, 1), ipNet("192.168.50.1"));
    CHECK_EQ(sweepAddrNet(base, 254), ipNet("192.168.50.254"));
    CHECK_EQ(netToStr(sweepAddrNet(base, 99)), std::string("192.168.50.99"));

    CHECK(parseSweepCidr("192.168.50.96/27", base, prefix));
    CHECK_EQ(sweepAddrNet(base, 1), ipNet("192.168.50.97"));
    CHECK_EQ(sweepAddrNet(base, 30), ipNet("192.168.50.126"));
}
