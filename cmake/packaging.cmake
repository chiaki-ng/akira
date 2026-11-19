# Build NRO
set(BUILD_FONT_DIR ${CMAKE_BINARY_DIR}/resources/font)
set(AKIRA_OVERLAY_COMMANDS)
foreach (overlay IN ITEMS platform/${AKIRA_PLATFORM_DIR} video/${AKIRA_VIDEO_PROVIDER})
    if (EXISTS ${PROJECT_RESOURCES}/${overlay})
        list(APPEND AKIRA_OVERLAY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES}/${overlay} ${CMAKE_BINARY_DIR}/resources)
    endif()
endforeach()
add_custom_target(${PROJECT_NAME}.nro
    DEPENDS ${PROJECT_NAME}
    COMMAND ${NX_NACPTOOL_EXE} --create "${APP_TITLE}" "${PROJECT_AUTHOR}" "${APP_VERSION_NACP}" ${PROJECT_NAME}.nacp
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/resources/platform ${CMAKE_BINARY_DIR}/resources/video ${CMAKE_BINARY_DIR}/resources/strings
    ${AKIRA_OVERLAY_COMMANDS}
    COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
        --icon=${PROJECT_ICON}
        --nacp=${PROJECT_NAME}.nacp
        --romfsdir=${CMAKE_BINARY_DIR}/resources
    ALL
)
