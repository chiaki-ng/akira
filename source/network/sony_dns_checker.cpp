#include "network/sony_dns_checker.hpp"
#include "network/custom_dns.hpp"
#include "log/log.hpp"

#include <switch.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>

namespace akira::network {
namespace {

DnsVerdict queryServer(Ipv4Address server, const char* domain)
{
    const auto answer = DnsClient(std::chrono::milliseconds(500)).queryA(server, domain);
    if (!answer) {
        log::error(
            "[NET] dns_probe_fail domain={} server={} step={} os_errno={} os_error=\"{}\"",
            domain, server.toString(), DnsClient::stepName(answer.error().step), answer.error().osErrno,
            answer.error().osErrno ? std::strerror(answer.error().osErrno) : "-");
        return DnsVerdict::Inconclusive;
    }
    const DnsVerdict verdict = classifySonyAnswer(*answer);
    if (verdict == DnsVerdict::Blocked)
        log::error("[NET] dns_probe_fail domain={} server={} step=blocked_answer",
            domain, server.toString());
    else if (verdict == DnsVerdict::Inconclusive)
        log::error("[NET] dns_probe_fail domain={} server={} step=invalid_or_empty_answer os_errno=0 os_error=\"-\"",
            domain, server.toString());
    return verdict;
}

const char* verdictName(DnsVerdict verdict)
{
    switch (verdict) {
        case DnsVerdict::Blocked: return "blocked";
        case DnsVerdict::Reachable: return "reachable";
        default: return "inconclusive";
    }
}

} // namespace

NetworkFacts readNetworkFacts(const CustomDns& dns)
{
    NetworkFacts facts;
    for (const Ipv4Address& server : dns.resolvers())
        facts.akiraDns.push_back(server.toString());
    uint32_t ip = 0;
    uint32_t mask = 0;
    uint32_t gateway = 0;
    uint32_t dns1 = 0;
    uint32_t dns2 = 0;
    if (R_FAILED(nifmGetCurrentIpConfigInfo(&ip, &mask, &gateway, &dns1, &dns2)) || ip == 0)
        return facts;
    facts.address = Ipv4Address::fromNetworkOrder(ip).toString();
    for (const uint32_t server : { dns1, dns2 })
        if (server != 0)
            facts.systemDns.push_back(Ipv4Address::fromNetworkOrder(server).toString());
    return facts;
}

SonyDnsReport checkSonyDnsAtStartup(const CustomDns& dns)
{
    SonyDnsReport report;
    uint32_t ip = 0;
    uint32_t mask = 0;
    uint32_t gateway = 0;
    uint32_t dns1 = 0;
    uint32_t dns2 = 0;
    const Result configResult = nifmGetCurrentIpConfigInfo(&ip, &mask, &gateway, &dns1, &dns2);
    if (R_FAILED(configResult) || ip == 0) {
        log::info("[NET] dns_probe skipped: no connected IP configuration (rc=0x{:08x})",
            static_cast<unsigned int>(configResult));
        return report;
    }

    const std::vector<Ipv4Address> dnsServers = dns.resolvers();

    for (const Ipv4Address& serverIp : dnsServers) {
        DnsServerStatus server;
        server.address = serverIp.toString();
        for (size_t i = 0; i < SonyDnsDomains.size(); ++i)
            server.domains[i] = queryServer(serverIp, SonyDnsDomains[i]);

        log::info(
            "[NET] dns_probe server={} auth={} profile={} cloud={} failed_domains={}",
            server.address, verdictName(server.domains[0]), verdictName(server.domains[1]),
            verdictName(server.domains[2]), server.failedCount());
        report.servers.push_back(std::move(server));
    }
    return report;
}

} // namespace akira::network
