#ifndef AKIRA_NET_WRAPPERS_HPP
#define AKIRA_NET_WRAPPERS_HPP

#include <netdb.h>
#include <unistd.h>

struct AddrInfoGuard {
    addrinfo* info = nullptr;
    AddrInfoGuard() = default;
    ~AddrInfoGuard() { if (info) freeaddrinfo(info); }
    AddrInfoGuard(const AddrInfoGuard&) = delete;
    AddrInfoGuard& operator=(const AddrInfoGuard&) = delete;
    addrinfo** ptr() { return &info; }
    operator addrinfo*() { return info; }
};

struct SocketGuard {
    int fd;
    explicit SocketGuard(int f) : fd(f) {}
    ~SocketGuard() { if (fd >= 0) close(fd); }
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    operator int() { return fd; }
    int release() { int f = fd; fd = -1; return f; }
};

#endif // AKIRA_NET_WRAPPERS_HPP
