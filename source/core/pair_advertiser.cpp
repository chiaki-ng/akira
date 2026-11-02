#include "core/pair_advertiser.hpp"

#include "core/chiaki_core.hpp"
#include "util/net_wrappers.hpp"

#include <cstring>
#include <vector>

#include <switch.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

namespace akira::pair {

namespace {

const char kInstance[] = "Akira Switch";
const char kHost[] = "akira";
const char kServiceA[] = "_akira-pair";
const char kServiceB[] = "_tcp";
const char kLocal[] = "local";

uint32_t localAddr() {
    uint32_t addr = 0;
    uint32_t mask = 0;
    if (R_SUCCEEDED(nifmInitialize(NifmServiceType_User))) {
        nifmGetCurrentIpConfigInfo(&addr, &mask, nullptr, nullptr, nullptr);
        nifmExit();
    }
    return addr;
}

void putU16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v & 0xff));
}

void putU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v >> 24));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    b.push_back(static_cast<uint8_t>(v & 0xff));
}

void putLabel(std::vector<uint8_t>& b, const char* s) {
    std::size_t n = std::strlen(s);
    b.push_back(static_cast<uint8_t>(n));
    b.insert(b.end(), s, s + n);
}

std::vector<uint8_t> serviceName() {
    std::vector<uint8_t> n;
    putLabel(n, kServiceA);
    putLabel(n, kServiceB);
    putLabel(n, kLocal);
    n.push_back(0);
    return n;
}

std::vector<uint8_t> instanceName() {
    std::vector<uint8_t> n;
    putLabel(n, kInstance);
    putLabel(n, kServiceA);
    putLabel(n, kServiceB);
    putLabel(n, kLocal);
    n.push_back(0);
    return n;
}

std::vector<uint8_t> hostName() {
    std::vector<uint8_t> n;
    putLabel(n, kHost);
    putLabel(n, kLocal);
    n.push_back(0);
    return n;
}

std::vector<uint8_t> buildAnnouncement(int port, uint32_t addr) {
    std::vector<uint8_t> svc = serviceName();
    std::vector<uint8_t> inst = instanceName();
    std::vector<uint8_t> host = hostName();

    std::vector<uint8_t> pkt;
    putU16(pkt, 0);
    putU16(pkt, 0x8400);
    putU16(pkt, 0);
    putU16(pkt, 4);
    putU16(pkt, 0);
    putU16(pkt, 0);

    pkt.insert(pkt.end(), svc.begin(), svc.end());
    putU16(pkt, 12);
    putU16(pkt, 0x0001);
    putU32(pkt, 120);
    putU16(pkt, static_cast<uint16_t>(inst.size()));
    pkt.insert(pkt.end(), inst.begin(), inst.end());

    pkt.insert(pkt.end(), inst.begin(), inst.end());
    putU16(pkt, 33);
    putU16(pkt, 0x8001);
    putU32(pkt, 120);
    std::vector<uint8_t> srv;
    putU16(srv, 0);
    putU16(srv, 0);
    putU16(srv, static_cast<uint16_t>(port));
    srv.insert(srv.end(), host.begin(), host.end());
    putU16(pkt, static_cast<uint16_t>(srv.size()));
    pkt.insert(pkt.end(), srv.begin(), srv.end());

    pkt.insert(pkt.end(), inst.begin(), inst.end());
    putU16(pkt, 16);
    putU16(pkt, 0x8001);
    putU32(pkt, 120);
    const char* txt = "txtvers=1";
    uint8_t tlen = static_cast<uint8_t>(std::strlen(txt));
    putU16(pkt, static_cast<uint16_t>(1 + tlen));
    pkt.push_back(tlen);
    pkt.insert(pkt.end(), txt, txt + tlen);

    pkt.insert(pkt.end(), host.begin(), host.end());
    putU16(pkt, 1);
    putU16(pkt, 0x8001);
    putU32(pkt, 120);
    putU16(pkt, 4);
    const uint8_t* ip = reinterpret_cast<const uint8_t*>(&addr);
    pkt.push_back(ip[0]);
    pkt.push_back(ip[1]);
    pkt.push_back(ip[2]);
    pkt.push_back(ip[3]);

    return pkt;
}

}

PairAdvertiser::~PairAdvertiser() {
    stop();
}

void PairAdvertiser::start(int port) {
    stop();
    auto started = libchiaking::platform::Thread::start(akira::ChiakiCore::current().threading(), "pair-advertiser",
        libchiaking::platform::ThreadRole::discovery_service,
        [this, port](libchiaking::platform::StopToken stop) { run(port, stop); });
    if (started)
        thread_ = std::move(started).value();
}

void PairAdvertiser::stop() {
    static_cast<void>(thread_.request_stop());
    thread_ = libchiaking::platform::Thread{};
}

void PairAdvertiser::run(int port, const libchiaking::platform::StopToken& stop) {
    uint32_t addr = localAddr();
    if (addr == 0)
        return;

    SocketGuard sock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (sock < 0)
        return;

    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif

    struct sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = htons(5353);
    bind(sock, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr));

    unsigned char ttl = 255;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    struct sockaddr_in mcast = {};
    mcast.sin_family = AF_INET;
    mcast.sin_addr.s_addr = inet_addr("224.0.0.251");
    mcast.sin_port = htons(5353);

    std::vector<uint8_t> pkt = buildAnnouncement(port, addr);

    while (!stop.stop_requested()) {
        sendto(sock, pkt.data(), pkt.size(), 0,
               reinterpret_cast<struct sockaddr*>(&mcast), sizeof(mcast));

        struct pollfd pfd = {};
        pfd.fd = sock;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 1000);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            uint8_t drain[1500];
            recv(sock, drain, sizeof(drain), 0);
        }
    }
}

}
