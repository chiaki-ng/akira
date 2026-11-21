#pragma once

#include <optional>
#include <string>

#include "network/sony_dns_checker.hpp"

namespace akira::network {

class SonyDnsFailure {
public:
    static auto first(const SonyDnsReport& report) -> std::optional<SonyDnsFailure>;

    auto domain() const -> const std::string& { return failedDomain; }
    auto server() const -> const std::string& { return failedServer; }

private:
    SonyDnsFailure(std::string domain, std::string server);

    static auto firstWith(const SonyDnsReport& report, DnsVerdict verdict) -> std::optional<SonyDnsFailure>;

    std::string failedDomain;
    std::string failedServer;
};

}
