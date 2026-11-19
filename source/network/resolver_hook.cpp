#include "network/resolver_hook.hpp"

#include "network/custom_dns.hpp"

namespace akira::network {

ResolverHook::ResolverHook(CustomDns& dns)
{
    installed.store(&dns, std::memory_order_release);
}

ResolverHook::~ResolverHook()
{
    installed.store(nullptr, std::memory_order_release);
}

std::optional<int> ResolverHook::getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                                             addrinfo** result)
{
    CustomDns* dns = installed.load(std::memory_order_acquire);
    if (!dns)
        return std::nullopt;
    return dns->getaddrinfo(node, service, hints, result);
}

bool ResolverHook::freeaddrinfo(addrinfo* list)
{
    CustomDns* dns = installed.load(std::memory_order_acquire);
    return dns && dns->freeaddrinfo(list);
}

}
