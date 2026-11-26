cmake_minimum_required(VERSION 3.13)

set(AKIRA_PLATFORM "nx" CACHE STRING "Target console: nx or ounce")
set_property(CACHE AKIRA_PLATFORM PROPERTY STRINGS nx ounce)
if (NOT AKIRA_PLATFORM MATCHES "^(nx|ounce)$")
    message(FATAL_ERROR "AKIRA_PLATFORM must be nx or ounce, not ${AKIRA_PLATFORM}")
endif()

set(AKIRA_SHELL "borealis" CACHE STRING "UI shell: borealis or arisa, both ship as akira")
set_property(CACHE AKIRA_SHELL PROPERTY STRINGS borealis arisa)
if (NOT AKIRA_SHELL MATCHES "^(borealis|arisa)$")
    message(FATAL_ERROR "AKIRA_SHELL must be borealis or arisa, not ${AKIRA_SHELL}")
endif()
set(AKIRA_TARGET akira)
set(AKIRA_ARISA_GPU "drako3d" CACHE STRING "GPU API of the arisa Switch 2 package: drako3d or vulkan")
set_property(CACHE AKIRA_ARISA_GPU PROPERTY STRINGS drako3d vulkan)
if (NOT AKIRA_ARISA_GPU MATCHES "^(drako3d|vulkan)$")
    message(FATAL_ERROR "AKIRA_ARISA_GPU must be drako3d or vulkan, not ${AKIRA_ARISA_GPU}")
endif()

# Force Switch platform
set(PLATFORM_SWITCH ON CACHE BOOL "Build for Nintendo Switch" FORCE)
if (AKIRA_PLATFORM STREQUAL "ounce")
    set(PLATFORM_SWITCH2 ON CACHE BOOL "Build for Switch 2 with libounce" FORCE)
else()
    set(PLATFORM_SWITCH2 OFF CACHE BOOL "Build for Switch 2 with libounce" FORCE)
endif()

# Borealis paths
if (PLATFORM_SWITCH2)
    set(AKIRA_LIBRARY_SUFFIX "-ounce")
else()
    set(AKIRA_LIBRARY_SUFFIX "")
endif()

set(BOREALIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis${AKIRA_LIBRARY_SUFFIX} CACHE PATH "Borealis source tree")
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)

if (PLATFORM_SWITCH2)
    set(USE_VULKAN ON CACHE BOOL "Use Vulkan for rendering" FORCE)
    set(USE_DRAKO3D ON CACHE BOOL "Use drako3d with a Vulkan fallback" FORCE)
    set(USE_DEKO3D OFF CACHE BOOL "Use deko3d for video rendering" FORCE)
else()
    # Use deko3d for video rendering (must be set BEFORE commonOption.cmake)
    set(USE_DEKO3D ON CACHE BOOL "Use deko3d for video rendering" FORCE)
    set(USE_VULKAN OFF CACHE BOOL "Use Vulkan for rendering" FORCE)
endif()

set(AKIRA_DRAKO3D OFF)
set(AKIRA_VULKAN OFF)
if (PLATFORM_SWITCH2 AND AKIRA_SHELL STREQUAL "borealis")
    set(AKIRA_DRAKO3D ON)
    set(AKIRA_VULKAN ON)
elseif (PLATFORM_SWITCH2 AND AKIRA_ARISA_GPU STREQUAL "drako3d")
    set(AKIRA_DRAKO3D ON)
elseif (PLATFORM_SWITCH2)
    set(AKIRA_VULKAN ON)
endif()

if (PLATFORM_SWITCH2)
    set(AKIRA_WORKER_CORE_MASK "0x3F")
    set(AKIRA_MAIN_CORE "4")
    set(BOREALIS_SWITCH_CORE_MASK "0x20" CACHE STRING "" FORCE)
    set(LIBCHIAKING_HORIZON_CORE_MASK ${AKIRA_WORKER_CORE_MASK} CACHE STRING "" FORCE)
else()
    set(AKIRA_WORKER_CORE_MASK "0x7")
    set(AKIRA_MAIN_CORE "-1")
endif()

option(DEBUG_DEKO3D_RENDERING "Enable debug squares in video shader" OFF)

# Platform common options + toolchain from borealis
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(akira)

add_compile_options(-fno-omit-frame-pointer)
add_compile_definitions(TOML_EXCEPTIONS=0)
if (AKIRA_SHELL STREQUAL "arisa")
    add_compile_options("SHELL:-include ${AKIRA_ARISA_PREFIX}/include/arisa/provider/diagnostics/trap.h")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Project metadata + git-derived version
include(version)

# Platform options
list(APPEND APP_PLATFORM_OPTION
    -DBUILD_PACKAGE_NAME=${PACKAGE_NAME}
)

# Threads
find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# Resources are embedded via --romfsdir in elf2nro
set(BRLS_RESOURCES_DIR "romfs:/")

include(dependencies)
include(core)
include(core_includes)
find_package(CURL REQUIRED)

# Vendored C libraries (wireguard target + lwIP sources)
include(vendored_libs)

###########################################
# Source files
###########################################
file(GLOB_RECURSE MAIN_SRC CONFIGURE_DEPENDS source/*.cpp)
if (NOT AKIRA_CLOUD)
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/cloud/(service|library_listing|library_status|catalog_rows|cloud_shortcuts|game_badges|game_actions|game_facts|release_notice|title_refresh_cooldown|launch_checks|fetch_progress|start_steps|cloud_connect_task)\\.cpp$")
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/cloud/views/(library_view|game_detail_view|launch_screen|cloud_shortcuts_rail|cloud_card|cloud_game_card|cloud_game_row_cell|cloud_catalog_data_source)\\.cpp$")
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/(cloud/ui/[a-z_]+|app/arisa/psn_cloud_desk)\\.cpp$")
endif()
include(platform)

if (AKIRA_SHELL STREQUAL "arisa")
    list(FILTER MAIN_SRC EXCLUDE REGEX "/views/")
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/app/exception_hook\\.cpp$")
    if (AKIRA_VULKAN)
        list(FILTER MAIN_SRC EXCLUDE REGEX "/source/app/arisa/deko3d_(host|presenter)\\.cpp$")
    else()
        list(FILTER MAIN_SRC EXCLUDE REGEX "/source/app/arisa/vulkan_(host|presenter)\\.cpp$")
    endif()
    set(AKIRA_SHELL_SRC ${MAIN_SRC})
    list(FILTER AKIRA_SHELL_SRC INCLUDE REGEX "/source/(app/arisa|[a-z]+/ui)/")
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/(app/arisa|[a-z]+/ui)/")
    if (NOT AKIRA_VULKAN)
        list(APPEND MAIN_SRC
            ${BOREALIS_LIBRARY}/lib/extern/nanovg/deko3d/framework/CMemPool.cpp
            ${BOREALIS_LIBRARY}/lib/extern/nanovg/deko3d/framework/CShader.cpp
            ${BOREALIS_LIBRARY}/lib/extern/nanovg/deko3d/framework/CIntrusiveTree.cpp)
    endif()
else()
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/(app/arisa|[a-z]+/ui)/")

    # Switch wrapper for libnx initialization
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)

    add_subdirectory(${BOREALIS_DIR}/library borealis)
endif()
add_subdirectory(library/tomlplusplus EXCLUDE_FROM_ALL)

###########################################
# Main target
###########################################
program_target(${AKIRA_TARGET} "${MAIN_SRC}")
set_target_properties(${AKIRA_TARGET} PROPERTIES CXX_STANDARD 23)
if (AKIRA_VIDEO_PROVIDER STREQUAL "vulkan")
    include(vulkan_shaders)
endif()

if (AKIRA_SHELL STREQUAL "borealis")
# Borealis uses DEBUG only to attach nxlinkStdio() in its Switch wrapper.
# Keep that capture path in optimized builds without compiling the whole app
# (or its libraries) with debug-only behavior. Runtime log levels and file
# logging are configured independently in source/app/views/main.cpp.
set_source_files_properties(
    ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c
    PROPERTIES COMPILE_DEFINITIONS DEBUG
)
endif()

target_sources(${AKIRA_TARGET} PRIVATE ${LWIP_SRCS} ${LWIP_RELAY_SRCS})
target_sources(${AKIRA_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library/qrcodegen/qrcodegen.cpp)

target_include_directories(${AKIRA_TARGET} PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/library/qrcodegen
    ${CMAKE_CURRENT_SOURCE_DIR}/library/stb
    # Generated version header (configure_file in cmake/version.cmake)
    ${CMAKE_BINARY_DIR}/generated
    # Shared IPC definitions
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/common/include
    # Vendored nanovg deko3d framework and nlohmann json
    ${BOREALIS_LIBRARY}/include/borealis/extern
    # WireGuard
    ${WIREGUARD_DIR}/include
    ${WIREGUARD_DIR}/src
    ${WIREGUARD_DIR}/lwip-relay/include
    # lwIP includes
    ${LWIP_DIR}/src/include
    ${APP_PLATFORM_INCLUDE}
)

target_compile_options(${AKIRA_TARGET} PRIVATE
    -ffunction-sections
    -fdata-sections
    -Wunused-variable
    ${APP_PLATFORM_OPTION}
)

if (USE_DEKO3D AND AKIRA_SHELL STREQUAL "borealis")
    target_compile_definitions(${AKIRA_TARGET} PRIVATE BOREALIS_USE_DEKO3D)
endif()
if (AKIRA_PLATFORM_NX AND AKIRA_SHELL STREQUAL "arisa")
    target_compile_definitions(${AKIRA_TARGET} PRIVATE __SWITCH__)
endif()
if (AKIRA_SHELL STREQUAL "borealis")
    target_include_directories(${AKIRA_TARGET} PRIVATE ${BOREALIS_LIBRARY}/include)
    target_link_libraries(${AKIRA_TARGET} PRIVATE borealis)
endif()

option(MUTE_CHIAKI_LOGS "Drop every core log line" OFF)
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_WORKER_CORE_MASK=${AKIRA_WORKER_CORE_MASK}
    AKIRA_MAIN_CORE=${AKIRA_MAIN_CORE})
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_MUTE_CORE_LOGS=$<BOOL:${MUTE_CHIAKI_LOGS}>)
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_CLOUD=$<BOOL:${AKIRA_CLOUD}>)
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_PLATFORM_OUNCE=$<BOOL:${AKIRA_PLATFORM_OUNCE}>)
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_STATS_OVERLAY=$<STREQUAL:${AKIRA_SHELL},borealis>)
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_PS5_CLOUD_LAUNCH=$<BOOL:${AKIRA_PS5_CLOUD_LAUNCH}>)
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_PS4_CLOUD_LAUNCH=$<BOOL:${AKIRA_PS4_CLOUD_LAUNCH}>)


set(AKIRA_SERVICE_LIBRARIES
    libchiaking_chiaki_ng
    libchiaking_curl
    libchiaking_crypto_libnx
    libchiaking_media_opus
    libchiaking_simdutf
    CURL::libcurl
    tomlplusplus::tomlplusplus
    ${SDL2_LIB}
    PkgConfig::FFMPEG
    ${DAV1D_LIB}
    ${JSONC_LIB}
    wireguard
    ${APP_PLATFORM_LIB}
)
target_link_libraries(${AKIRA_TARGET} PRIVATE ${AKIRA_SERVICE_LIBRARIES})

if (AKIRA_SHELL STREQUAL "arisa")
    include(arisa_shell)
endif()

# deko3d shader compilation + uam runtime link
if (AKIRA_VIDEO_PROVIDER STREQUAL "deko3d")
    include(deko3d_shaders)
endif()
if (AKIRA_DRAKO3D)
    include(drako3d_shaders)
endif()
target_compile_definitions(${AKIRA_TARGET} PRIVATE AKIRA_DRAKO3D=$<BOOL:${AKIRA_DRAKO3D}> AKIRA_VULKAN=$<BOOL:${AKIRA_VULKAN}>)

# SDL2 depends on EGL/OpenGL libraries on Switch
if (AKIRA_PLATFORM_NX)
    target_link_libraries(${AKIRA_TARGET} PRIVATE EGL glapi drm_nouveau)
endif()

target_link_options(${AKIRA_TARGET} PRIVATE ${APP_PLATFORM_LINK_OPTION})
if (PLATFORM_SWITCH)
    target_link_options(${AKIRA_TARGET} PRIVATE
        -Wl,--wrap=svcSetThreadCoreMask
        -Wl,--wrap=getaddrinfo
        -Wl,--wrap=freeaddrinfo)
endif()
if (PLATFORM_SWITCH AND AKIRA_SHELL STREQUAL "borealis")
    target_link_options(${AKIRA_TARGET} PRIVATE
        -Wl,--wrap=abort
        -Wl,--wrap=__assert_func
        -Wl,--wrap=fatalThrow
        -Wl,--wrap=fatalThrowWithPolicy)
endif()

# NRO packaging
include(packaging)
