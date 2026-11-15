###########################################
# Find dependencies for Switch
###########################################
if (PLATFORM_SWITCH)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil libswscale libswresample)

    if (PLATFORM_SWITCH2)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c)
        set(SDL2_LIB PkgConfig::SDL2)
        set(JSONC_LIB PkgConfig::JSONC)
    else()
        # Find SDL2
        find_library(SDL2_LIB SDL2)
        if(NOT SDL2_LIB)
            message(FATAL_ERROR "SDL2 not found")
        endif()

        # Find json-c for PSN API
        find_library(JSONC_LIB json-c PATHS ${PORTLIBS}/lib)
        if(JSONC_LIB)
            list(APPEND APP_PLATFORM_INCLUDE ${PORTLIBS}/include/json-c)
        endif()

        # Find dav1d (AV1 decoder)
        find_library(DAV1D_LIB dav1d)
    endif()
endif()
