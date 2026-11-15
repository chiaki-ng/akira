#include <netdb.h>

#include "util/custom_dns.hpp"

extern "C" int __real_getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result);
extern "C" void __real_freeaddrinfo(addrinfo* list);

extern "C" int __wrap_getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result)
{
    if (const auto handled = akira::net::CustomDns::instance().getaddrinfo(node, service, hints, result))
        return *handled;
    return __real_getaddrinfo(node, service, hints, result);
}

extern "C" void __wrap_freeaddrinfo(addrinfo* list)
{
    if (!akira::net::CustomDns::instance().freeaddrinfo(list))
        __real_freeaddrinfo(list);
}
