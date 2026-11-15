find_program(GLSLANG_VALIDATOR glslangValidator)
if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator is required to compile the Vulkan shaders")
endif ()

set(AKIRA_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan)
set(AKIRA_SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated/shaders)
file(MAKE_DIRECTORY ${AKIRA_SHADER_OUTPUT_DIR})
set(AKIRA_SHADER_HEADERS)

function(akira_vulkan_shader name source)
    set(output ${AKIRA_SHADER_OUTPUT_DIR}/${name}.h)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 ${ARGN} --vn ${name} -o ${output} ${AKIRA_SHADER_SOURCE_DIR}/${source}
        DEPENDS ${AKIRA_SHADER_SOURCE_DIR}/${source} ${AKIRA_SHADER_SOURCE_DIR}/nv12.glsl
        COMMENT "Compiling Vulkan shader ${name}"
        VERBATIM)
    set(AKIRA_SHADER_HEADERS ${AKIRA_SHADER_HEADERS} ${output} PARENT_SCOPE)
endfunction()

akira_vulkan_shader(fullscreen_vert fullscreen.vert)
akira_vulkan_shader(overlay_vert overlay.vert)
akira_vulkan_shader(video_image_frag video.frag)
akira_vulkan_shader(video_texel_frag video.frag -DTEXEL_BUFFER)
akira_vulkan_shader(easu_image_frag easu.frag)
akira_vulkan_shader(easu_texel_frag easu.frag -DTEXEL_BUFFER)
akira_vulkan_shader(rcas_frag rcas.frag)
akira_vulkan_shader(pass_frag pass.frag)
akira_vulkan_shader(overlay_frag overlay.frag)
akira_vulkan_shader(border_frag border.frag)

target_sources(${PROJECT_NAME} PRIVATE ${AKIRA_SHADER_HEADERS})
target_include_directories(${PROJECT_NAME} PRIVATE ${AKIRA_SHADER_OUTPUT_DIR})
