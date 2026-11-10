cmake_minimum_required(VERSION 3.13)

# Force Switch platform
set(PLATFORM_SWITCH ON CACHE BOOL "Build for Nintendo Switch" FORCE)

# Borealis paths
set(BOREALIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis CACHE PATH "Borealis source tree")
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)

# Use deko3d for video rendering (must be set BEFORE commonOption.cmake)
set(USE_DEKO3D ON CACHE BOOL "Use deko3d for video rendering" FORCE)

option(DEBUG_DEKO3D_RENDERING "Enable debug squares in video shader" OFF)

# Platform common options + toolchain from borealis
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(akira)

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
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/cloud/(service|library_listing|library_view|game_detail_view|game_actions|launch_checks|launch_screen|fetch_progress|start_steps|cloud_connect_task)\\.cpp$")
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/views/cloud_shortcuts_rail\\.cpp$")
endif()
list(APPEND MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/stream/nxext_ipc_server.c)

# Switch wrapper for libnx initialization
list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)

# Sub-libraries
add_subdirectory(${BOREALIS_DIR}/library borealis)
add_subdirectory(library/tomlplusplus EXCLUDE_FROM_ALL)

###########################################
# Main target
###########################################
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

# Borealis uses DEBUG only to attach nxlinkStdio() in its Switch wrapper.
# Keep that capture path in optimized builds without compiling the whole app
# (or its libraries) with debug-only behavior. Runtime log levels and file
# logging are configured independently in source/main.cpp.
set_source_files_properties(
    ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c
    PROPERTIES COMPILE_DEFINITIONS DEBUG
)

target_sources(${PROJECT_NAME} PRIVATE ${LWIP_SRCS} ${LWIP_RELAY_SRCS})
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library/qrcodegen/qrcodegen.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/library/qrcodegen
    # Generated version header (configure_file in cmake/version.cmake)
    ${CMAKE_BINARY_DIR}/generated
    # Shared IPC definitions
    ${CMAKE_CURRENT_SOURCE_DIR}/overlay/common/include
    # Borealis includes
    ${BOREALIS_LIBRARY}/include
    # WireGuard
    ${WIREGUARD_DIR}/include
    ${WIREGUARD_DIR}/src
    ${WIREGUARD_DIR}/lwip-relay/include
    # lwIP includes
    ${LWIP_DIR}/src/include
    ${APP_PLATFORM_INCLUDE}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -ffunction-sections
    -fdata-sections
    -Wunused-variable
    ${APP_PLATFORM_OPTION}
)

if (USE_DEKO3D)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BOREALIS_USE_DEKO3D)
endif()

option(MUTE_CHIAKI_LOGS "Drop every core log line" OFF)
target_compile_definitions(${PROJECT_NAME} PRIVATE AKIRA_MUTE_CORE_LOGS=$<BOOL:${MUTE_CHIAKI_LOGS}>)
target_compile_definitions(${PROJECT_NAME} PRIVATE AKIRA_CLOUD=$<BOOL:${AKIRA_CLOUD}>)
target_compile_definitions(${PROJECT_NAME} PRIVATE AKIRA_PS5_CLOUD_LAUNCH=$<BOOL:${AKIRA_PS5_CLOUD_LAUNCH}>)
target_compile_definitions(${PROJECT_NAME} PRIVATE AKIRA_PS4_CLOUD_LAUNCH=$<BOOL:${AKIRA_PS4_CLOUD_LAUNCH}>)


target_link_libraries(${PROJECT_NAME} PRIVATE
    borealis
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

# deko3d shader compilation + uam runtime link
include(deko3d_shaders)

# SDL2 depends on EGL/OpenGL libraries on Switch
if (PLATFORM_SWITCH)
    target_link_libraries(${PROJECT_NAME} PRIVATE EGL glapi drm_nouveau)
endif()

target_link_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_LINK_OPTION})
if (PLATFORM_SWITCH)
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,--wrap=svcSetThreadCoreMask)
endif()

# NRO packaging
include(packaging)
