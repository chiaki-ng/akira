set(VERSION_MAJOR "0")
set(VERSION_MINOR "7")
set(VERSION_PATCH "0")

if (NOT DEFINED BUILD_CHANNEL)
    set(BUILD_CHANNEL "dev")
endif()

if (NOT DEFINED VERSION_PRERELEASE)
    set(VERSION_PRERELEASE "")
endif()

find_package(Git QUIET)

set(GIT_COMMIT "")
set(GIT_DIRTY "")
if (Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=no
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

string(TIMESTAMP BUILD_DATE "%Y-%m-%d" UTC)

set(BUILD_METADATA "")
if (NOT GIT_COMMIT STREQUAL "")
    set(BUILD_METADATA "${GIT_COMMIT}")
    if (NOT GIT_DIRTY STREQUAL "")
        set(BUILD_METADATA "${BUILD_METADATA}.dirty")
    endif()
endif()

set(APP_VERSION_CORE "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(APP_VERSION_NACP "${APP_VERSION_CORE}")
if (NOT VERSION_PRERELEASE STREQUAL "")
    set(APP_VERSION_NACP "${APP_VERSION_NACP}-${VERSION_PRERELEASE}")
endif()

set(APP_VERSION "${APP_VERSION_NACP}")
if (NOT BUILD_METADATA STREQUAL "")
    set(APP_VERSION "${APP_VERSION}+${BUILD_METADATA}")
endif()

set(APP_TITLE "Akira")
set(PROJECT_AUTHOR "xlanor")
set(PACKAGE_NAME "io.github.akira")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/akira_version.h.in
    ${CMAKE_BINARY_DIR}/generated/akira_version.h
    @ONLY
)

message(STATUS "Akira version: ${APP_VERSION} (channel: ${BUILD_CHANNEL})")
