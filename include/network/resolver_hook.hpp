#pragma once

#include <netdb.h>

#include <atomic>
#include <optional>

namespace akira::network {

class CustomDns;

class ResolverHook {
public:
    explicit ResolverHook(CustomDns& dns);
    ~ResolverHook();

    ResolverHook(const ResolverHook&) = delete;
    ResolverHook& operator=(const ResolverHook&) = delete;

    static std::optional<int> getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                                          addrinfo** result);
    static bool freeaddrinfo(addrinfo* list);

private:
    static inline std::atomic<CustomDns*> installed{nullptr};
};

}
