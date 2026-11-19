#include "core/stun_client.hpp"
#include "log/log.hpp"

#include <cstring>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <switch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <curl/curl.h>
#include <format>
#include "util/curl_wrappers.hpp"
#include "util/http.hpp"

#include <sys/stat.h>
#include "util/net_wrappers.hpp"

static std::optional<akira::log::Log::Redirect> natLogStart() {
    mkdir("sdmc:/switch/akira/logs", 0755);

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    auto path = std::format("sdmc:/switch/akira/logs/{:02}{:02}{:02}_{:02}{:02}{:02}_nat.log",
        t->tm_mday, t->tm_mon + 1, t->tm_year % 100, t->tm_hour, t->tm_min, t->tm_sec);

    auto redirect = akira::log::Log::process().redirect(path);
    if (!redirect)
        akira::log::warning("NAT: could not open {}, keeping the current log output", path);
    return redirect;
}

static const char* STUN_SERVER_1 = "stun.l.google.com";
static const char* STUN_SERVER_2 = "stun1.l.google.com";
static const uint16_t STUN_PORT = 19302;
static const int STUN_MAX_RETRIES = 3;

static const uint16_t STUN_BINDING_REQUEST = 0x0001;
static const uint16_t STUN_BINDING_RESPONSE = 0x0101;
static const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;
static const uint16_t STUN_ATTR_MAPPED_ADDRESS = 0x0001;
static const uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020;
static const uint16_t STUN_ATTR_CHANGE_REQUEST = 0x0003;
static const uint16_t STUN_ATTR_OTHER_ADDRESS = 0x802C;

static const char* RFC5780_SERVER_LIST_URL = "https://raw.githubusercontent.com/pradt2/always-online-stun/master/valid_nat_testing_hosts.txt";

std::string StunClient::natTypeToString(NATType type) {
    switch (type) {
        case NATType::OpenInternet: return "Open Internet";
        case NATType::FullCone: return "Cone NAT (Type 2)";
        case NATType::RestrictedCone: return "Cone NAT (Type 2)";
        case NATType::PortRestrictedCone: return "Cone NAT (Type 2)";
        case NATType::Symmetric: return "Symmetric NAT (Type 3)";
        case NATType::SymmetricPortOnly: return "Symmetric NAT (Type 3)";
        case NATType::UDPBlocked: return "UDP Blocked";
        default: return "Unknown";
    }
}

std::string StunClient::natTypeDescription(NATType type) {
    switch (type) {
        case NATType::OpenInternet:
            return "Open internet, no NAT";
        case NATType::FullCone:
            return "Full Cone, holepunch supported";
        case NATType::RestrictedCone:
            return "Restricted Cone, holepunch supported";
        case NATType::PortRestrictedCone:
            return "Port-Restricted Cone, holepunch supported";
        case NATType::Symmetric:
            return "Symmetric, holepunch may work depending on network";
        case NATType::SymmetricPortOnly:
            return "Symmetric (port-dependent), holepunch may work depending on network";
        case NATType::UDPBlocked:
            return "UDP traffic is blocked";
        default:
            return "Could not determine NAT type";
    }
}

std::string StunClient::filteringTypeToString(FilteringType type) {
    switch (type) {
        case FilteringType::EndpointIndependent: return "Open";
        case FilteringType::AddressDependent: return "Restricted";
        case FilteringType::AddressPortDependent: return "Port-Restricted";
        default: return "Unknown";
    }
}

std::vector<std::string> StunClient::fetchRFC5780Servers() {
    std::vector<std::string> servers;

    HttpResponse result = httpGet(RFC5780_SERVER_LIST_URL, "", 5);
    if (!result.ok()) {
        akira::log::warning("STUN: could not fetch RFC5780 server list: {}",
            result.transportFailed() ? result.error : std::format("HTTP {}", result.status));
        return servers;
    }

    std::istringstream stream(result.body);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line[0] != '#') {
            size_t start = line.find_first_not_of(" \t\r\n");
            size_t end = line.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                servers.push_back(line.substr(start, end - start + 1));
            }
        }
    }

    return servers;
}

FilteringType StunClient::detectFiltering() {
    akira::log::info("=== Filtering Detection ===");
    std::vector<std::string> servers = fetchRFC5780Servers();
    akira::log::info("Fetched {} RFC5780 servers", servers.size());

    for (const auto& server : servers) {
        akira::log::info("Trying filtering server: {}", server);
        FilteringType filtering = testFilteringWithServer(server);
        if (filtering != FilteringType::Unknown) {
            akira::log::info("Filtering result: {}", filteringTypeToString(filtering));
            return filtering;
        }
        akira::log::info("Server {} returned Unknown, trying next", server);
    }

    akira::log::info("Filtering result: Unknown (no servers worked)");
    return FilteringType::Unknown;
}

FilteringType StunClient::testFilteringWithServer(const std::string& server) {
    size_t colonPos = server.find(':');
    if (colonPos == std::string::npos) {
        return FilteringType::Unknown;
    }

    std::string host = server.substr(0, colonPos);
    uint16_t port = static_cast<uint16_t>(std::stoi(server.substr(colonPos + 1)));

    SocketGuard sock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (sock < 0) {
        return FilteringType::Unknown;
    }

    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = 0;

    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        return FilteringType::Unknown;
    }

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    auto portStr = std::to_string(port);

    AddrInfoGuard serverInfo;
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, serverInfo.ptr()) != 0) {
        return FilteringType::Unknown;
    }

    uint8_t request[20];
    memset(request, 0, sizeof(request));
    request[0] = (STUN_BINDING_REQUEST >> 8) & 0xFF;
    request[1] = STUN_BINDING_REQUEST & 0xFF;
    request[4] = (STUN_MAGIC_COOKIE >> 24) & 0xFF;
    request[5] = (STUN_MAGIC_COOKIE >> 16) & 0xFF;
    request[6] = (STUN_MAGIC_COOKIE >> 8) & 0xFF;
    request[7] = STUN_MAGIC_COOKIE & 0xFF;

    srand(time(nullptr));
    for (int i = 8; i < 20; i++) {
        request[i] = rand() & 0xFF;
    }

    ssize_t sent = sendto(sock, request, sizeof(request), 0, serverInfo.info->ai_addr, serverInfo.info->ai_addrlen);
    if (sent != sizeof(request)) {
        return FilteringType::Unknown;
    }

    uint8_t response[512];
    ssize_t received = recv(sock, response, sizeof(response), 0);

    if (received < 20) {
        return FilteringType::Unknown;
    }

    MappedAddress mapped;
    OtherAddress other;
    other.valid = false;

    if (!parseBindingResponse(response, received, mapped, &other)) {
        akira::log::info("  Filtering: failed to parse binding response");
        return FilteringType::Unknown;
    }

    akira::log::info("  Filtering: mapped={}:{}", mapped.ip, mapped.port);

    if (!other.valid) {
        akira::log::info("  Filtering: server did not provide OTHER-ADDRESS");
        return FilteringType::Unknown;
    }

    akira::log::info("  Filtering: OTHER-ADDRESS={}:{}", other.ip, other.port);

    uint8_t changeRequest[28];
    memset(changeRequest, 0, sizeof(changeRequest));
    changeRequest[0] = (STUN_BINDING_REQUEST >> 8) & 0xFF;
    changeRequest[1] = STUN_BINDING_REQUEST & 0xFF;
    changeRequest[2] = 0;
    changeRequest[3] = 8;
    changeRequest[4] = (STUN_MAGIC_COOKIE >> 24) & 0xFF;
    changeRequest[5] = (STUN_MAGIC_COOKIE >> 16) & 0xFF;
    changeRequest[6] = (STUN_MAGIC_COOKIE >> 8) & 0xFF;
    changeRequest[7] = STUN_MAGIC_COOKIE & 0xFF;

    for (int i = 8; i < 20; i++) {
        changeRequest[i] = rand() & 0xFF;
    }

    changeRequest[20] = (STUN_ATTR_CHANGE_REQUEST >> 8) & 0xFF;
    changeRequest[21] = STUN_ATTR_CHANGE_REQUEST & 0xFF;
    changeRequest[22] = 0;
    changeRequest[23] = 4;
    changeRequest[24] = 0;
    changeRequest[25] = 0;
    changeRequest[26] = 0;
    changeRequest[27] = 0x06;

    AddrInfoGuard origInfo;
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, origInfo.ptr()) != 0) {
        return FilteringType::Unknown;
    }

    sent = sendto(sock, changeRequest, sizeof(changeRequest), 0, origInfo.info->ai_addr, origInfo.info->ai_addrlen);

    if (sent != sizeof(changeRequest)) {
        return FilteringType::Unknown;
    }

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    received = recvfrom(sock, response, sizeof(response), 0, (struct sockaddr*)&fromAddr, &fromLen);

    if (received >= 20) {
        char fromIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &fromAddr.sin_addr, fromIP, sizeof(fromIP));
        uint16_t fromPort = ntohs(fromAddr.sin_port);
        akira::log::info("  Filtering Test II (change IP+port): got response from {}:{} (server was {}:{})", fromIP, fromPort, host, port);
        if (std::string(fromIP) != host || fromPort != port) {
            akira::log::info("  Filtering: EndpointIndependent (response from alt addr)");
            return FilteringType::EndpointIndependent;
        }
        akira::log::info("  Filtering Test II: response from same addr, server may not support CHANGE-REQUEST");
    } else {
        akira::log::info("  Filtering Test II (change IP+port): no response (timeout)");
    }

    changeRequest[27] = 0x02;
    for (int i = 8; i < 20; i++) {
        changeRequest[i] = rand() & 0xFF;
    }

    AddrInfoGuard origInfo2;
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, origInfo2.ptr()) != 0) {
        return FilteringType::Unknown;
    }

    sent = sendto(sock, changeRequest, sizeof(changeRequest), 0, origInfo2.info->ai_addr, origInfo2.info->ai_addrlen);

    if (sent != sizeof(changeRequest)) {
        return FilteringType::Unknown;
    }

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    fromLen = sizeof(fromAddr);
    received = recvfrom(sock, response, sizeof(response), 0, (struct sockaddr*)&fromAddr, &fromLen);

    if (received >= 20) {
        char fromIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &fromAddr.sin_addr, fromIP, sizeof(fromIP));
        uint16_t fromPort = ntohs(fromAddr.sin_port);
        akira::log::info("  Filtering Test III (change port): got response from {}:{}", fromIP, fromPort);
        if (std::string(fromIP) == host && fromPort != port) {
            akira::log::info("  Filtering: AddressDependent");
            return FilteringType::AddressDependent;
        }
    } else {
        akira::log::info("  Filtering Test III (change port): no response (timeout)");
    }

    akira::log::info("  Filtering: AddressPortDependent");
    return FilteringType::AddressPortDependent;
}

StunResult StunClient::detectNATType() {
    StunResult result;
    result.type = NATType::Unknown;
    result.filtering = FilteringType::Unknown;
    result.externalPort = 0;

    SocketGuard sock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (sock < 0) {
        result.error = "Failed to create socket";
        return result;
    }

    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = 0;

    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        result.error = "Failed to bind socket";
        return result;
    }

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    auto natLog = natLogStart();
    akira::log::info("=== NAT Type Detection ===");

    MappedAddress addr1 = sendBindingRequest(STUN_SERVER_1, STUN_PORT, sock);
    akira::log::info("Test 1: {}:{} -> {} (ip={} port={})",
        STUN_SERVER_1, STUN_PORT, addr1.valid ? "OK" : "FAIL",
        addr1.valid ? addr1.ip : "n/a", addr1.port);

    if (!addr1.valid) {
        result.type = NATType::UDPBlocked;
        result.error = "No response from STUN server";
        akira::log::info("Result: UDP Blocked");
        return result;
    }

    result.externalIP = addr1.ip;
    result.externalPort = addr1.port;

    MappedAddress addr2 = sendBindingRequest(STUN_SERVER_2, STUN_PORT, sock);
    akira::log::info("Test 2: {}:{} -> {} (ip={} port={})",
        STUN_SERVER_2, STUN_PORT, addr2.valid ? "OK" : "FAIL",
        addr2.valid ? addr2.ip : "n/a", addr2.port);

    if (!addr2.valid) {
        result.type = NATType::Unknown;
        result.error = "Second STUN server did not respond";
        result.filtering = detectFiltering();
        akira::log::info("Result: Unknown (second server failed)");
        return result;
    } else if (addr1.port != addr2.port) {
        result.type = NATType::Symmetric;
        akira::log::info("addr1.port={} != addr2.port={} -> Symmetric (address-dependent)", addr1.port, addr2.port);
    } else {
        std::vector<std::string> rfc5780servers = fetchRFC5780Servers();
        MappedAddress addr3;
        addr3.valid = false;
        addr3.port = 0;
        for (const auto& srv : rfc5780servers) {
            size_t colonPos = srv.find(':');
            if (colonPos == std::string::npos) continue;
            std::string srvHost = srv.substr(0, colonPos);
            uint16_t srvPort = static_cast<uint16_t>(std::stoi(srv.substr(colonPos + 1)));
            addr3 = sendBindingRequest(srvHost.c_str(), srvPort, sock);
            akira::log::info("Test 3: {}:{} -> {} (ip={} port={})",
                srvHost, srvPort, addr3.valid ? "OK" : "FAIL",
                addr3.valid ? addr3.ip : "n/a", addr3.port);
            if (addr3.valid) break;
        }

        if (addr3.valid && addr3.port != addr1.port) {
            result.type = NATType::SymmetricPortOnly;
            akira::log::info("addr1.port={} != addr3.port={} -> Symmetric (port-dependent)", addr1.port, addr3.port);
        } else {
            result.type = NATType::FullCone;
            akira::log::info("addr1.port={} == addr3.port={} -> Cone NAT", addr1.port, addr3.valid ? addr3.port : 0);
        }
    }

    result.filtering = detectFiltering();
    akira::log::info("Filtering: {}", filteringTypeToString(result.filtering));

    if (result.type == NATType::FullCone && result.filtering != FilteringType::Unknown) {
        if (result.filtering == FilteringType::AddressDependent) {
            result.type = NATType::RestrictedCone;
        } else if (result.filtering == FilteringType::AddressPortDependent) {
            result.type = NATType::PortRestrictedCone;
        }
    }

    akira::log::info("Final: {} - {}", natTypeToString(result.type), natTypeDescription(result.type));
    return result;
}

StunClient::MappedAddress StunClient::sendBindingRequest(const char* stunServer, uint16_t stunPort, int localSocket) {
    MappedAddress result;
    result.valid = false;
    result.port = 0;

    struct addrinfo hints, *serverInfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    auto portStr2 = std::to_string(stunPort);

    if (getaddrinfo(stunServer, portStr2.c_str(), &hints, &serverInfo) != 0) {
        akira::log::info("  sendBindingRequest: DNS failed for {}:{}", stunServer, stunPort);
        return result;
    }

    for (int attempt = 0; attempt < STUN_MAX_RETRIES; attempt++) {
        uint8_t request[20];
        memset(request, 0, sizeof(request));

        request[0] = (STUN_BINDING_REQUEST >> 8) & 0xFF;
        request[1] = STUN_BINDING_REQUEST & 0xFF;

        request[4] = (STUN_MAGIC_COOKIE >> 24) & 0xFF;
        request[5] = (STUN_MAGIC_COOKIE >> 16) & 0xFF;
        request[6] = (STUN_MAGIC_COOKIE >> 8) & 0xFF;
        request[7] = STUN_MAGIC_COOKIE & 0xFF;

        for (int i = 8; i < 20; i++) {
            request[i] = rand() & 0xFF;
        }

        ssize_t sent = sendto(localSocket, request, sizeof(request), 0,
                              serverInfo->ai_addr, serverInfo->ai_addrlen);

        if (sent != sizeof(request)) {
            akira::log::info("  sendBindingRequest: send failed attempt {}/{} to {}:{}", attempt + 1, STUN_MAX_RETRIES, stunServer, stunPort);
            continue;
        }

        uint8_t response[512];
        ssize_t received = recv(localSocket, response, sizeof(response), 0);

        if (received < 20) {
            akira::log::info("  sendBindingRequest: timeout attempt {}/{} to {}:{}", attempt + 1, STUN_MAX_RETRIES, stunServer, stunPort);
            continue;
        }

        if (parseBindingResponse(response, received, result)) {
            freeaddrinfo(serverInfo);
            return result;
        }
    }

    freeaddrinfo(serverInfo);
    return result;
}

bool StunClient::parseBindingResponse(const uint8_t* buffer, size_t length, MappedAddress& result, OtherAddress* otherAddr) {
    if (length < 20) {
        return false;
    }

    uint16_t msgType = (buffer[0] << 8) | buffer[1];
    if (msgType != STUN_BINDING_RESPONSE) {
        return false;
    }

    uint16_t msgLength = (buffer[2] << 8) | buffer[3];
    if (length < 20 + msgLength) {
        return false;
    }

    size_t offset = 20;
    while (offset + 4 <= 20 + msgLength) {
        uint16_t attrType = (buffer[offset] << 8) | buffer[offset + 1];
        uint16_t attrLength = (buffer[offset + 2] << 8) | buffer[offset + 3];

        if (offset + 4 + attrLength > length) {
            break;
        }

        if (attrType == STUN_ATTR_XOR_MAPPED_ADDRESS && attrLength >= 8) {
            uint8_t family = buffer[offset + 5];
            if (family == 0x01) {
                uint16_t xorPort = (buffer[offset + 6] << 8) | buffer[offset + 7];
                result.port = xorPort ^ (STUN_MAGIC_COOKIE >> 16);

                uint32_t xorAddr = (buffer[offset + 8] << 24) | (buffer[offset + 9] << 16) |
                                   (buffer[offset + 10] << 8) | buffer[offset + 11];
                uint32_t addr = xorAddr ^ STUN_MAGIC_COOKIE;

                char ipStr[INET_ADDRSTRLEN];
                struct in_addr inAddr;
                inAddr.s_addr = htonl(addr);
                inet_ntop(AF_INET, &inAddr, ipStr, sizeof(ipStr));
                result.ip = ipStr;
                result.valid = true;
            }
        } else if (attrType == STUN_ATTR_MAPPED_ADDRESS && attrLength >= 8 && !result.valid) {
            uint8_t family = buffer[offset + 5];
            if (family == 0x01) {
                result.port = (buffer[offset + 6] << 8) | buffer[offset + 7];

                uint32_t addr = (buffer[offset + 8] << 24) | (buffer[offset + 9] << 16) |
                                (buffer[offset + 10] << 8) | buffer[offset + 11];

                char ipStr[INET_ADDRSTRLEN];
                struct in_addr inAddr;
                inAddr.s_addr = htonl(addr);
                inet_ntop(AF_INET, &inAddr, ipStr, sizeof(ipStr));
                result.ip = ipStr;
                result.valid = true;
            }
        } else if (attrType == STUN_ATTR_OTHER_ADDRESS && attrLength >= 8 && otherAddr) {
            uint8_t family = buffer[offset + 5];
            if (family == 0x01) {
                otherAddr->port = (buffer[offset + 6] << 8) | buffer[offset + 7];

                uint32_t addr = (buffer[offset + 8] << 24) | (buffer[offset + 9] << 16) |
                                (buffer[offset + 10] << 8) | buffer[offset + 11];

                char ipStr[INET_ADDRSTRLEN];
                struct in_addr inAddr;
                inAddr.s_addr = htonl(addr);
                inet_ntop(AF_INET, &inAddr, ipStr, sizeof(ipStr));
                otherAddr->ip = ipStr;
                otherAddr->valid = true;
            }
        }

        offset += 4 + attrLength;
        if (attrLength % 4 != 0) {
            offset += 4 - (attrLength % 4);
        }
    }

    return result.valid;
}
