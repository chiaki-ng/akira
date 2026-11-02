file(GLOB_RECURSE akira_core_include_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

set(akira_core_allowed_header
    "^libchiaking/((chiaki_ng|(users|consoles|catalog|trophies|play|kakusan)(_types)?)\\.hpp|platform/(platform|defaults|stock_providers)\\.hpp)$")

set(akira_core_include_violations "")
foreach(source IN LISTS akira_core_include_sources)
  file(STRINGS ${source} lines REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]libchiaking/")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"].*$" "\\1" header "${line}")
    if(NOT header MATCHES "${akira_core_allowed_header}")
      file(RELATIVE_PATH relative ${CMAKE_CURRENT_SOURCE_DIR} ${source})
      string(APPEND akira_core_include_violations "\n  ${relative}: ${header}")
    endif()
  endforeach()
endforeach()

if(akira_core_include_violations)
  message(FATAL_ERROR "These includes reach past the core's public faces:${akira_core_include_violations}")
endif()
