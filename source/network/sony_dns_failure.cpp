#include "network/sony_dns_failure.hpp"

#include <utility>

namespace akira::network {

SonyDnsFailure::SonyDnsFailure(std::string domain, std::string server)
    : failedDomain(std::move(domain))
    , failedServer(std::move(server))
{
}

auto SonyDnsFailure::first(const SonyDnsReport& report) -> std::optional<SonyDnsFailure>
{
    if (std::optional<SonyDnsFailure> blocked = firstWith(report, DnsVerdict::Blocked))
        return blocked;
    return firstWith(report, DnsVerdict::Inconclusive);
}

auto SonyDnsFailure::firstWith(const SonyDnsReport& report, DnsVerdict verdict) -> std::optional<SonyDnsFailure>
{
    for (const DnsServerStatus& server : report.servers)
    {
        for (std::size_t index = 0; index < server.domains.size(); ++index)
        {
            if (server.domains[index] == verdict)
                return SonyDnsFailure(SonyDnsDomains[index], server.address);
        }
    }
    return std::nullopt;
}

}
