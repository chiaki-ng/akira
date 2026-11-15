if (PLATFORM_SWITCH2)
    set(AKIRA_VIDEO_VULKAN_DEFAULT ON)
    set(AKIRA_VIDEO_DEKO3D_DEFAULT OFF)
else()
    set(AKIRA_VIDEO_VULKAN_DEFAULT OFF)
    set(AKIRA_VIDEO_DEKO3D_DEFAULT ON)
endif()

option(AKIRA_VIDEO_VULKAN "Build the Vulkan video provider" ${AKIRA_VIDEO_VULKAN_DEFAULT})
option(AKIRA_VIDEO_DEKO3D "Build the deko3d video provider" ${AKIRA_VIDEO_DEKO3D_DEFAULT})

if (AKIRA_VIDEO_VULKAN AND AKIRA_VIDEO_DEKO3D)
    message(FATAL_ERROR "Enable only one of AKIRA_VIDEO_VULKAN and AKIRA_VIDEO_DEKO3D")
endif()
if (NOT AKIRA_VIDEO_VULKAN AND NOT AKIRA_VIDEO_DEKO3D)
    message(FATAL_ERROR "Enable one of AKIRA_VIDEO_VULKAN and AKIRA_VIDEO_DEKO3D")
endif()

set(AKIRA_VIDEO_VULKAN_SOURCES
    stream/stream_decoder
    stream/hardware_decoder
    stream/decoder_output
    stream/h264_sps
    stream/vulkan_renderer
    video/vulkan_provider)
set(AKIRA_VIDEO_DEKO3D_SOURCES
    stream/video_decoder
    stream/deko3d_renderer
    video/deko3d_provider)

foreach (source IN LISTS AKIRA_VIDEO_VULKAN_SOURCES AKIRA_VIDEO_DEKO3D_SOURCES)
    list(REMOVE_ITEM MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/${source}.cpp)
endforeach()

if (AKIRA_VIDEO_VULKAN)
    set(AKIRA_VIDEO_SOURCES ${AKIRA_VIDEO_VULKAN_SOURCES})
else()
    set(AKIRA_VIDEO_SOURCES ${AKIRA_VIDEO_DEKO3D_SOURCES})
endif()
foreach (source IN LISTS AKIRA_VIDEO_SOURCES)
    list(APPEND MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/${source}.cpp)
endforeach()

message(STATUS "Video provider: ${AKIRA_VIDEO_SOURCES}")
