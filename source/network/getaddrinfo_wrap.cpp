#include <netdb.h>

#include "network/resolver_hook.hpp"

extern "C" int __real_getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result);
extern "C" void __real_freeaddrinfo(addrinfo* list);

extern "C" int __wrap_getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result)
{
    if (const auto handled = akira::network::ResolverHook::getaddrinfo(node, service, hints, result))
        return *handled;
    return __real_getaddrinfo(node, service, hints, result);
}

extern "C" void __wrap_freeaddrinfo(addrinfo* list)
{
    if (!akira::network::ResolverHook::freeaddrinfo(list))
        __real_freeaddrinfo(list);
}
