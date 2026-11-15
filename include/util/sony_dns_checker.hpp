#ifndef AKIRA_SONY_DNS_CHECKER_HPP
#define AKIRA_SONY_DNS_CHECKER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "util/dns_client.hpp"
#include "util/ipv4_address.hpp"

namespace akira::net {

inline constexpr std::array<const char*, 3> SonyDnsDomains = {
    "auth.api.sonyentertainmentnetwork.com",
    "m.np.playstation.com",
    "psnow.playstation.com",
};

enum class DnsVerdict { Inconclusive, Reachable, Blocked };

struct DnsServerStatus {
    std::string address;
    std::array<DnsVerdict, 3> domains{};

    int failedCount() const {
        int count = 0;
        for (DnsVerdict verdict : domains)
            count += verdict != DnsVerdict::Reachable;
        return count;
    }

    bool hasSonyFailure() const { return failedCount() > 0; }
};

struct SonyDnsReport {
    std::vector<DnsServerStatus> servers;

    bool failed() const {
        for (const DnsServerStatus& server : servers)
            if (server.hasSonyFailure())
                return true;
        return false;
    }
};

inline bool isBlockedSonyAddress(Ipv4Address address)
{
    const uint32_t hostOrder = ntohl(address.networkOrder());
    return (hostOrder >> 24) == 127 || hostOrder == 0;
}

inline DnsVerdict classifySonyAnswer(std::span<const DnsClient::Record> records)
{
    bool sawBlocked = false;
    bool sawReachable = false;
    for (const DnsClient::Record& record : records)
        (isBlockedSonyAddress(record.address) ? sawBlocked : sawReachable) = true;
    if (sawReachable)
        return DnsVerdict::Reachable;
    if (sawBlocked)
        return DnsVerdict::Blocked;
    return DnsVerdict::Inconclusive;
}

inline DnsVerdict classifyDnsAResponse(const uint8_t* packet, size_t length, uint16_t expectedId)
{
    if (!packet)
        return DnsVerdict::Inconclusive;
    const auto records = DnsClient::parseResponse({packet, length}, expectedId);
    return records ? classifySonyAnswer(*records) : DnsVerdict::Inconclusive;
}

inline int secondsUntilDnsWarningOk(int64_t elapsedMilliseconds)
{
    if (elapsedMilliseconds >= 5000)
        return 0;
    if (elapsedMilliseconds < 0)
        elapsedMilliseconds = 0;
    return static_cast<int>((5000 - elapsedMilliseconds + 999) / 1000);
}

// Runs on a worker thread; only numeric DNS servers configured on the Switch
// are queried. It makes no HTTP requests and sends no account data.
SonyDnsReport checkSonyDnsAtStartup();

} // namespace akira::net

#endif // AKIRA_SONY_DNS_CHECKER_HPP
