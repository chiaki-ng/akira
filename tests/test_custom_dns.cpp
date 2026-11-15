#include "test_util.hpp"
#include "util/custom_dns.hpp"

using akira::net::CustomDns;

TEST(custom_dns_matches_sony_domains_and_subdomains)
{
    CHECK(CustomDns::isSonyHost("auth.api.sonyentertainmentnetwork.com"));
    CHECK(CustomDns::isSonyHost("m.np.playstation.com"));
    CHECK(CustomDns::isSonyHost("remoteplay.dl.playstation.net"));
    CHECK(CustomDns::isSonyHost("ca.account.sony.com"));
    CHECK(CustomDns::isSonyHost("playstation.com"));
    CHECK(CustomDns::isSonyHost("psnow.playstation.com."));
}

TEST(custom_dns_leaves_other_names_to_the_system)
{
    CHECK(!CustomDns::isSonyHost("nintendo.net"));
    CHECK(!CustomDns::isSonyHost("api.github.com"));
    CHECK(!CustomDns::isSonyHost("notplaystation.com"));
    CHECK(!CustomDns::isSonyHost("playstation.com.evil.example"));
    CHECK(!CustomDns::isSonyHost("192.168.1.10"));
    CHECK(!CustomDns::isSonyHost("localhost"));
    CHECK(!CustomDns::isSonyHost(""));
}

TEST(custom_dns_has_built_in_servers)
{
    CHECK(CustomDns::DefaultServers[0].toString() == "1.1.1.1");
    CHECK(CustomDns::DefaultServers[1].toString() == "8.8.8.8");
}
