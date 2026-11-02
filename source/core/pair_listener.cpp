#include "core/pair_listener.hpp"

#include "core/chiaki_core.hpp"
#include "core/pair_crypto.hpp"
#include "util/net_wrappers.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include <switch.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <json-c/json.h>

namespace akira::pair {

namespace {

constexpr std::size_t kSealedHeaderLen = 5 + kPubKeyLen + kIvLen + 4;
constexpr std::size_t kMaxCiphertext = 65536;

bool recvExact(int fd, uint8_t* buf, std::size_t n) {
    std::size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        if (r <= 0)
            return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

bool sendExact(int fd, const uint8_t* buf, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
        ssize_t s = send(fd, buf + sent, n - sent, 0);
        if (s <= 0)
            return false;
        sent += static_cast<std::size_t>(s);
    }
    return true;
}

std::string jsonStr(json_object* obj, const char* key) {
    json_object* v = nullptr;
    if (json_object_object_get_ex(obj, key, &v) && v)
        return json_object_get_string(v);
    return "";
}

int64_t jsonInt(json_object* obj, const char* key) {
    json_object* v = nullptr;
    if (json_object_object_get_ex(obj, key, &v) && v)
        return json_object_get_int64(v);
    return 0;
}

bool jsonBool(json_object* obj, const char* key) {
    json_object* v = nullptr;
    if (json_object_object_get_ex(obj, key, &v) && v)
        return json_object_get_boolean(v);
    return false;
}

bool parsePayload(const std::vector<uint8_t>& plaintext, PairedCredentials& out) {
    std::string text(plaintext.begin(), plaintext.end());
    json_object* root = json_tokener_parse(text.c_str());
    if (!root)
        return false;

    out.duid = jsonStr(root, "duid");
    out.accountId = jsonStr(root, "account_id");
    out.onlineId = jsonStr(root, "online_id");
    out.accessToken = jsonStr(root, "access_token");
    out.refreshToken = jsonStr(root, "refresh_token");
    out.expiresIn = static_cast<int>(jsonInt(root, "expires_in"));
    out.expiresAt = jsonInt(root, "expires_at");
    out.isExpired = jsonBool(root, "is_expired");
    out.npsso = jsonStr(root, "npsso");

    out.mobileAccessToken = jsonStr(root, "psn_mobile_sso_access_token");
    out.mobileRefreshToken = jsonStr(root, "psn_mobile_sso_refresh_token");
    out.mobileExpiresAt = jsonInt(root, "psn_mobile_sso_expires_at");
    out.hasMobile = !out.mobileAccessToken.empty();

    json_object_put(root);

    return !out.accessToken.empty() || !out.duid.empty();
}

}  // namespace

PairListener::~PairListener() {
    stop();
}

std::string PairListener::generateCode() {
    std::array<std::byte, 2> raw{};
    if (!ChiakiCore::current().random().fill_bytes(raw))
        raw = {};
    unsigned value = ((std::to_integer<unsigned>(raw[0]) << 8) | std::to_integer<unsigned>(raw[1])) % 10000u;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04u", value);
    return std::string(buf);
}

std::string PairListener::localIpv4() {
    uint32_t addr = 0;
    uint32_t mask = 0;
    Result rc = nifmInitialize(NifmServiceType_User);
    if (R_SUCCEEDED(rc)) {
        nifmGetCurrentIpConfigInfo(&addr, &mask, nullptr, nullptr, nullptr);
        nifmExit();
    } else {
        return "";
    }
    if (addr == 0)
        return "";
    struct in_addr a = {};
    a.s_addr = addr;
    char buf[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &a, buf, sizeof(buf)))
        return "";
    return std::string(buf);
}

void PairListener::start(int port, const std::string& code, EventCallback onEvent, ImportCallback onImport) {
    stop();
    onEvent_ = std::move(onEvent);
    onImport_ = std::move(onImport);
    auto started = libchiaking::platform::Thread::start(akira::ChiakiCore::current().threading(), "pair-listener",
        libchiaking::platform::ThreadRole::unspecified,
        [this, port, code](libchiaking::platform::StopToken stop) { run(port, code, stop); });
    if (!started) {
        if (onEvent_) onEvent_(ListenerEvent::Error);
        return;
    }
    thread_ = std::move(started).value();
}

void PairListener::stop() {
    static_cast<void>(thread_.request_stop());
    thread_ = libchiaking::platform::Thread{};
}

void PairListener::run(int port, const std::string& code, const libchiaking::platform::StopToken& stop) {
    SocketGuard listenSock(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (listenSock < 0) {
        if (onEvent_) onEvent_(ListenerEvent::Error);
        return;
    }

    int one = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(listenSock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        if (onEvent_) onEvent_(ListenerEvent::Error);
        return;
    }
    if (listen(listenSock, 1) < 0) {
        if (onEvent_) onEvent_(ListenerEvent::Error);
        return;
    }

    int listenFlags = fcntl(listenSock, F_GETFL, 0);
    fcntl(listenSock, F_SETFL, listenFlags | O_NONBLOCK);

    if (onEvent_) onEvent_(ListenerEvent::Listening);

    auto& core = ChiakiCore::current();
    KeyExchange exchange{core.crypto(), core.random()};
    auto startTime = std::chrono::steady_clock::now();
    int tries = 0;

    while (!stop.stop_requested()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - startTime)
                           .count();
        if (elapsed >= kWindowSeconds) {
            if (onEvent_) onEvent_(ListenerEvent::TimedOut);
            break;
        }

        struct pollfd pfd = {};
        pfd.fd = listenSock;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 500);
        if (pr <= 0 || !(pfd.revents & POLLIN))
            continue;

        struct sockaddr_in clientAddr = {};
        socklen_t clientLen = sizeof(clientAddr);
        int client = accept(listenSock, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (client < 0)
            continue;

        SocketGuard clientSock(client);
        int clientFlags = fcntl(clientSock, F_GETFL, 0);
        fcntl(clientSock, F_SETFL, clientFlags & ~O_NONBLOCK);
        struct timeval ctv = {5, 0};
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &ctv, sizeof(ctv));

        if (onEvent_) onEvent_(ListenerEvent::ClientConnected);

        Hello hello;
        auto switchKey = exchange.makeHello(hello);
        if (!switchKey) {
            if (onEvent_) onEvent_(ListenerEvent::Error);
            continue;
        }

        std::vector<uint8_t> helloBytes = hello.encode();
        if (!sendExact(clientSock, helloBytes.data(), helloBytes.size())) {
            if (onEvent_) onEvent_(ListenerEvent::Error);
            continue;
        }

        std::vector<uint8_t> buf(kSealedHeaderLen);
        if (!recvExact(clientSock, buf.data(), kSealedHeaderLen)) {
            if (onEvent_) onEvent_(ListenerEvent::Error);
            continue;
        }

        std::size_t lenOff = 5 + kPubKeyLen + kIvLen;
        std::size_t ctLen = (static_cast<std::size_t>(buf[lenOff]) << 24) |
                            (static_cast<std::size_t>(buf[lenOff + 1]) << 16) |
                            (static_cast<std::size_t>(buf[lenOff + 2]) << 8) |
                            static_cast<std::size_t>(buf[lenOff + 3]);
        if (ctLen > kMaxCiphertext) {
            if (onEvent_) onEvent_(ListenerEvent::Error);
            continue;
        }

        buf.resize(kSealedHeaderLen + ctLen + kMacLen);
        if (!recvExact(clientSock, buf.data() + kSealedHeaderLen, ctLen + kMacLen)) {
            if (onEvent_) onEvent_(ListenerEvent::Error);
            continue;
        }

        std::vector<uint8_t> plaintext;
        OpenResult result = exchange.openSealed(hello, *switchKey, buf.data(), buf.size(), code, plaintext);

        PairedCredentials creds;
        bool parsed = (result == OpenResult::Ok) && parsePayload(plaintext, creds);

        bool imported = parsed;

        uint8_t ack = 0x02;
        if (imported)
            ack = 0x01;
        else if (result == OpenResult::BadMac)
            ack = 0x00;
        sendExact(clientSock, &ack, 1);

        if (imported) {
            if (onImport_) onImport_(creds);
            if (onEvent_) onEvent_(ListenerEvent::Imported);
            break;
        }

        if (result == OpenResult::BadMac) {
            tries++;
            if (tries >= kMaxTries) {
                if (onEvent_) onEvent_(ListenerEvent::LockedOut);
                break;
            }
            if (onEvent_) onEvent_(ListenerEvent::BadCode);
            continue;
        }

        if (onEvent_) onEvent_(ListenerEvent::Error);
        continue;
    }
}

}
