# Build NRO
set(BUILD_FONT_DIR ${CMAKE_BINARY_DIR}/resources/font)
set(AKIRA_OVERLAY_COMMANDS)
foreach (overlay IN ITEMS platform/${AKIRA_PLATFORM_DIR} video/${AKIRA_VIDEO_PROVIDER})
    if (EXISTS ${PROJECT_RESOURCES}/${overlay})
        list(APPEND AKIRA_OVERLAY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES}/${overlay} ${CMAKE_BINARY_DIR}/resources)
    endif()
endforeach()
if (AKIRA_SHELL STREQUAL "arisa")
    set(AKIRA_ROMFS_DROPPED ${CMAKE_BINARY_DIR}/resources/xml ${CMAKE_BINARY_DIR}/resources/i18n)
    list(APPEND AKIRA_OVERLAY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${arisa_ASSETS_DIR} ${CMAKE_BINARY_DIR}/resources)
else()
    set(AKIRA_ROMFS_DROPPED ${CMAKE_BINARY_DIR}/resources/strings)
endif()
add_custom_target(${AKIRA_TARGET}.nro
    DEPENDS ${AKIRA_TARGET}
    COMMAND ${NX_NACPTOOL_EXE} --create "${APP_TITLE}" "${PROJECT_AUTHOR}" "${APP_VERSION_NACP}" ${AKIRA_TARGET}.nacp
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/resources/platform ${CMAKE_BINARY_DIR}/resources/video ${AKIRA_ROMFS_DROPPED}
    ${AKIRA_OVERLAY_COMMANDS}
    COMMAND ${NX_ELF2NRO_EXE} ${AKIRA_TARGET}.elf ${AKIRA_TARGET}.nro
        --icon=${PROJECT_ICON}
        --nacp=${AKIRA_TARGET}.nacp
        --romfsdir=${CMAKE_BINARY_DIR}/resources
    ALL
)
