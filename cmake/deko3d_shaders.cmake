# Compile deko3d shaders and link uam runtime library
# uam must be linked BEFORE EGL because both bundle mesa GLSL symbols;
# link order determines which symbols win with -z,muldefs
if (USE_DEKO3D)
    find_program(NX_UAM_EXE NAMES uam HINTS "${DEVKITPRO}/tools/bin")
    if (NOT NX_UAM_EXE)
        message(FATAL_ERROR "Could not find uam: try installing uam")
    endif()

    find_library(UAM_LIB uam HINTS "${DEVKITPRO}/portlibs/switch/lib")
    if (NOT UAM_LIB)
        message(FATAL_ERROR "Could not find libuam: build library/uam with -Dbuild_as_library=true")
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${UAM_LIB})
    target_include_directories(${PROJECT_NAME} PRIVATE "${DEVKITPRO}/portlibs/switch/include")
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,muldefs)

    set(SHADER_DIR "${PROJECT_RESOURCES}/video/deko3d/shaders")
    set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/resources/shaders")

    # Compile borealis/nanovg shaders
    gen_dksh("${SHADER_OUT_DIR}")

    # Compile text shaders (video shaders are compiled at runtime via uam library)
    file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

    execute_process(COMMAND ${NX_UAM_EXE} -s vert -o "${SHADER_OUT_DIR}/text_vsh.dksh" "${SHADER_DIR}/text_vsh.glsl" TIMEOUT 5)
    execute_process(COMMAND ${NX_UAM_EXE} -s frag -o "${SHADER_OUT_DIR}/text_fsh.dksh" "${SHADER_DIR}/text_fsh.glsl" TIMEOUT 5)
    message(STATUS "Compiled text shaders to ${SHADER_OUT_DIR}")
    message(STATUS "Video shaders will be compiled at runtime via uam library")
endif()
