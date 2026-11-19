#ifndef AKIRA_STUN_CLIENT_HPP
#define AKIRA_STUN_CLIENT_HPP

#include <string>
#include <vector>
#include <cstdint>

enum class NATType {
    Unknown,
    OpenInternet,
    FullCone,
    RestrictedCone,
    PortRestrictedCone,
    Symmetric,
    SymmetricPortOnly,
    UDPBlocked
};

enum class FilteringType {
    Unknown,
    EndpointIndependent,
    AddressDependent,
    AddressPortDependent
};

struct StunResult {
    NATType type;
    FilteringType filtering;
    std::string externalIP;
    uint16_t externalPort;
    std::string error;
};

class StunClient {
public:
    static StunResult detectNATType();
    static std::string natTypeToString(NATType type);
    static std::string natTypeDescription(NATType type);
    static std::string filteringTypeToString(FilteringType type);

private:
    struct MappedAddress {
        std::string ip;
        uint16_t port;
        bool valid;
    };

    struct OtherAddress {
        std::string ip;
        uint16_t port;
        bool valid;
    };

    static MappedAddress sendBindingRequest(const char* stunServer, uint16_t stunPort, int localSocket);
    static bool parseBindingResponse(const uint8_t* buffer, size_t length, MappedAddress& result, OtherAddress* otherAddr = nullptr);
    static std::vector<std::string> fetchRFC5780Servers();
    static FilteringType detectFiltering();
    static FilteringType testFilteringWithServer(const std::string& server);
};

#endif
