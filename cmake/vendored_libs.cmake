# WireGuard library
set(WIREGUARD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/switch-wireguard CACHE PATH "switch-wireguard source tree")

file(GLOB WIREGUARD_SRCS CONFIGURE_DEPENDS
    ${WIREGUARD_DIR}/src/*.c
    ${WIREGUARD_DIR}/library/crypto/*.c
)
add_library(wireguard STATIC ${WIREGUARD_SRCS})
target_include_directories(wireguard PRIVATE
    ${WIREGUARD_DIR}/include
    ${WIREGUARD_DIR}/library/crypto
    ${WIREGUARD_DIR}/src
)
target_compile_options(wireguard PRIVATE
    -march=armv8-a+crc+crypto
    -mtune=cortex-a57
    -mtp=soft
    -fPIC
    -O2
    -Wall
    -ffunction-sections
)

###########################################
# lwIP library (for WireGuard relay)
###########################################
set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/lwip CACHE PATH "lwIP source tree")

set(LWIP_SRCS
    ${LWIP_DIR}/src/core/init.c
    ${LWIP_DIR}/src/core/def.c
    ${LWIP_DIR}/src/core/inet_chksum.c
    ${LWIP_DIR}/src/core/ip.c
    ${LWIP_DIR}/src/core/mem.c
    ${LWIP_DIR}/src/core/memp.c
    ${LWIP_DIR}/src/core/netif.c
    ${LWIP_DIR}/src/core/pbuf.c
    ${LWIP_DIR}/src/core/tcp.c
    ${LWIP_DIR}/src/core/tcp_in.c
    ${LWIP_DIR}/src/core/tcp_out.c
    ${LWIP_DIR}/src/core/timeouts.c
    ${LWIP_DIR}/src/core/udp.c
    ${LWIP_DIR}/src/core/ipv4/ip4.c
    ${LWIP_DIR}/src/core/ipv4/ip4_addr.c
    ${LWIP_DIR}/src/core/ipv4/ip4_frag.c
    ${LWIP_DIR}/src/core/ipv4/icmp.c
)

set(LWIP_RELAY_SRCS
    ${WIREGUARD_DIR}/lwip-relay/src/wg_netif.c
    ${WIREGUARD_DIR}/lwip-relay/src/sys_arch.c
)
