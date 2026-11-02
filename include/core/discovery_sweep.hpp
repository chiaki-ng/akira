#ifndef AKIRA_DISCOVERY_SWEEP_HPP
#define AKIRA_DISCOVERY_SWEEP_HPP

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>

namespace akira::discovery {

inline uint32_t prefixMask(int prefix)
{
    if (prefix <= 0)
        return 0u;
    if (prefix >= 32)
        return 0xFFFFFFFFu;
    return 0xFFFFFFFFu << (32 - prefix);
}

inline uint32_t subnetAddrCount(int prefix)
{
    if (prefix <= 0)
        return 0u;
    if (prefix >= 32)
        return 1u;
    return 1u << (32 - prefix);
}

inline bool parseSweepCidr(const std::string& token, uint32_t& baseHost, int& prefix)
{
    size_t slash = token.find('/');
    if (slash == std::string::npos)
        return false;

    std::string ipPart = token.substr(0, slash);
    std::string prefixPart = token.substr(slash + 1);
    if (ipPart.empty() || prefixPart.empty())
        return false;

    for (char c : prefixPart) {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    }
    int p = std::atoi(prefixPart.c_str());
    if (p < 24 || p > 32)
        return false;

    struct in_addr ina = {};
    if (inet_pton(AF_INET, ipPart.c_str(), &ina) != 1)
        return false;

    prefix = p;
    baseHost = ntohl(ina.s_addr) & prefixMask(p);
    return true;
}

inline bool isValidSweepCidr(const std::string& token)
{
    uint32_t base = 0;
    int prefix = 0;
    return parseSweepCidr(token, base, prefix);
}

inline std::string normalizeSweepCidr(const std::string& token)
{
    uint32_t base = 0;
    int prefix = 0;
    if (!parseSweepCidr(token, base, prefix))
        return token;

    struct in_addr netAddr = {};
    netAddr.s_addr = htonl(base);
    char buf[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &netAddr, buf, sizeof(buf)))
        return token;

    return std::string(buf) + "/" + std::to_string(prefix);
}

inline void sweepHostOffsets(int prefix, uint32_t& firstOffset, uint32_t& lastOffset)
{
    uint32_t count = subnetAddrCount(prefix);
    if (prefix >= 32) {
        firstOffset = 0;
        lastOffset = 0;
    } else if (prefix == 31) {
        firstOffset = 0;
        lastOffset = 1;
    } else {
        firstOffset = 1;
        lastOffset = count - 2;
    }
}

inline uint32_t sweepHostCount(int prefix)
{
    uint32_t first = 0;
    uint32_t last = 0;
    sweepHostOffsets(prefix, first, last);
    return last - first + 1;
}

inline uint32_t sweepAddrNet(uint32_t baseHost, uint32_t offset)
{
    return htonl(baseHost + offset);
}

inline bool subnetContainsLocal(uint32_t localAddrNet, uint32_t baseHost, int prefix)
{
    uint32_t mask = prefixMask(prefix);
    return (ntohl(localAddrNet) & mask) == (baseHost & mask);
}

inline int sweepChunkCount(int totalTargets, int chunkSize)
{
    if (chunkSize <= 0 || totalTargets <= 0)
        return 0;
    return (totalTargets + chunkSize - 1) / chunkSize;
}

} // namespace akira::discovery

#endif // AKIRA_DISCOVERY_SWEEP_HPP
