if (PLATFORM_SWITCH2)
    set(AKIRA_PLATFORM_OUNCE ON)
    set(AKIRA_PLATFORM_NX OFF)
else()
    set(AKIRA_PLATFORM_OUNCE OFF)
    set(AKIRA_PLATFORM_NX ON)
endif()

set(AKIRA_PLATFORM_NX_SOURCES
    core/update_manager
    input/extended_input_manager
    input/pad_output_state
    input/pad_path_mc
    input/rumble_profile
    stream/ipc_service
    views/battery_pips
    views/controller_picker_view
    views/lightbar_picker
    views/players_panel_view
    views/settings_updates_view
    views/update_flow)

if (NOT AKIRA_PLATFORM_NX)
    foreach (source IN LISTS AKIRA_PLATFORM_NX_SOURCES)
        list(REMOVE_ITEM MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/${source}.cpp)
    endforeach()
endif()

if (AKIRA_PLATFORM_NX)
    list(APPEND MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/stream/nxext_ipc_server.c)
endif()

set(AKIRA_PLATFORM_NX_RESOURCES
    img/pads
    xml/settings/updates.xml
    help/updates.json
    help/zh-Hans/updates.json)
set(AKIRA_PLATFORM_OUNCE_RESOURCES
    help/ounce.json
    help/zh-Hans/ounce.json)

if (AKIRA_PLATFORM_NX)
    list(APPEND AKIRA_FOREIGN_RESOURCES ${AKIRA_PLATFORM_OUNCE_RESOURCES})
else()
    list(APPEND AKIRA_FOREIGN_RESOURCES ${AKIRA_PLATFORM_NX_RESOURCES})
endif()

message(STATUS "Platform: nx=${AKIRA_PLATFORM_NX} ounce=${AKIRA_PLATFORM_OUNCE}")
