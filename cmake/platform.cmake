if (PLATFORM_SWITCH2)
    set(AKIRA_PLATFORM_OUNCE ON)
    set(AKIRA_PLATFORM_NX OFF)
else()
    set(AKIRA_PLATFORM_OUNCE OFF)
    set(AKIRA_PLATFORM_NX ON)
endif()

if (AKIRA_PLATFORM_NX)
    set(AKIRA_PLATFORM_DIR nx)
    set(AKIRA_VIDEO_PROVIDER deko3d)
    set(AKIRA_PLATFORM_BACKENDS
        factories/nx
        video/deko3d
        video/nvtegra
        video/uam
        display/fixed
        updates/github
        stats/nxext
        pads/extended
        haptics/extended
        feedback/extended
        pad_settings/extended
        stream/couch)
else()
    set(AKIRA_PLATFORM_DIR ounce)
    set(AKIRA_VIDEO_PROVIDER vulkan)
    set(AKIRA_PLATFORM_BACKENDS
        factories/ounce
        video/viddec
        video/vulkan
        display/per_mode
        updates/none
        stats/none
        pads/hid
        haptics/hid
        feedback/hid
        pad_settings/hid
        stream/single)
endif()

list(FILTER MAIN_SRC EXCLUDE REGEX "/source/platform/(factories|video|display|updates|stats|pads|haptics|feedback|pad_settings|stream)/")
foreach (backend IN LISTS AKIRA_PLATFORM_BACKENDS)
    file(GLOB_RECURSE AKIRA_BACKEND_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/source/platform/${backend}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/platform/${backend}/*.c)
    list(APPEND MAIN_SRC ${AKIRA_BACKEND_SOURCES})
endforeach()

message(STATUS "Platform: ${AKIRA_PLATFORM_DIR} (${AKIRA_PLATFORM_BACKENDS})")
