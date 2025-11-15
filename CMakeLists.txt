cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

# ---------------------------------------------------------------------------
# Fallbacks if not provided by buildspec/bootstrap
# ---------------------------------------------------------------------------
if(NOT DEFINED _name)
  set(_name "fly-scoreboard")
endif()

if(NOT DEFINED _version)
  set(_version "1.0.0")
endif()

project(${_name} VERSION ${_version} LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ENABLE_FRONTEND_API   "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_HTTP_SERVER    "Serve overlay on http://127.0.0.1:<port>"  ON)
option(EMBED_DEFAULT_ASSETS  "Embed data/overlay + locale into binary"   ON)

include(compilerconfig)
include(defaults)
include(helpers)

# ---------------------------------------------------------------------------
# Global compile flags / behavior
# ---------------------------------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

if(MSVC)
  # Don't treat warnings as errors for this plugin, force UTF-8
  add_compile_options(/WX- /utf-8)
endif()

if(WIN32 OR APPLE)
  add_compile_definitions(HAVE_OBS_DOCK_BY_ID=1)
endif()

if(APPLE)
  add_compile_options(
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=newline-eof>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=deprecated-this-capture>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=deprecated-declarations>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=range-loop-construct>
  )
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(FS_INC_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/src/include")
set(FS_3P_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/src/thirdparty")
set(FS_SRC_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(FS_DATA_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(FS_OVERLAY_DIR "${FS_DATA_DIR}/overlay")
set(FS_LOCALE_DIR  "${FS_DATA_DIR}/locale")
set(FS_GEN_DIR     "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${FS_GEN_DIR}")

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_library(${CMAKE_PROJECT_NAME} MODULE)

# libobs
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# ---------------------------------------------------------------------------
# Qt (dock + dialogs + hotkeys, now required)
# ---------------------------------------------------------------------------
find_package(Qt6 COMPONENTS Core Widgets QUIET)
if(Qt6_FOUND)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
else()
  find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt5::Core Qt5::Widgets)
endif()

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  AUTORCC ON
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_QT=1)

# ---------------------------------------------------------------------------
# Optional: frontend API
# ---------------------------------------------------------------------------
if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_FRONTEND_API=1)
endif()

# ---------------------------------------------------------------------------
# Optional: HTTP server
# ---------------------------------------------------------------------------
if(ENABLE_HTTP_SERVER)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_HTTP_SERVER=1)
  if(WIN32)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ws2_32)
  endif()
endif()

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${FS_INC_DIR}
  ${FS_SRC_DIR}
  ${FS_GEN_DIR}
  ${FS_3P_DIR}
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(OBS_FLY_SCORE_SRC
  ${FS_SRC_DIR}/fly_score_plugin.cpp
  ${FS_SRC_DIR}/fly_score_hotkeys.cpp
  ${FS_SRC_DIR}/fly_score_state.cpp
  ${FS_SRC_DIR}/seed_defaults.cpp
)

if(ENABLE_HTTP_SERVER)
  list(APPEND OBS_FLY_SCORE_SRC
    ${FS_SRC_DIR}/fly_score_server.cpp
  )
endif()

# Qt-related sources (dock + settings dialog) are always included now
list(APPEND OBS_FLY_SCORE_SRC
  ${FS_SRC_DIR}/fly_score_dock.cpp
  ${FS_INC_DIR}/fly_score_dock.hpp
  ${FS_SRC_DIR}/fly_score_settings_dialog.cpp
  ${FS_INC_DIR}/fly_score_settings_dialog.hpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${OBS_FLY_SCORE_SRC})

# ---------------------------------------------------------------------------
# Macros used in code
# ---------------------------------------------------------------------------
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  PLUGIN_NAME_STR="${_name}"
  PLUGIN_VERSION_STR="${_version}"
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED YES
)

# ---------------------------------------------------------------------------
# Embedded assets (overlay + locale)
# ---------------------------------------------------------------------------
if(EMBED_DEFAULT_ASSETS)
  set(EMBED_TEXT_FILES
    "${FS_OVERLAY_DIR}/index.html"
    "${FS_OVERLAY_DIR}/style.css"
    "${FS_OVERLAY_DIR}/script.js"
    "${FS_LOCALE_DIR}/en-US.ini"
    "${FS_LOCALE_DIR}/ro-RO.ini"
  )

  set(_asset_entries "")
  foreach(_f IN LISTS EMBED_TEXT_FILES)
    if(EXISTS "${_f}")
      get_filename_component(_fname "${_f}" NAME)
      string(REPLACE "." "_" _id "${_fname}")
      string(REPLACE "-" "_" _id "${_id}")
      file(READ "${_f}" _contents)
      if(NOT _contents MATCHES "\n$")
        set(_contents "${_contents}\n")
      endif()
      string(APPEND _asset_entries
"#define HAVE_${_id} 1
inline constexpr const char* ${_id} = R\"obsplg(${_contents})obsplg\";\n")
    else()
      message(WARNING "Embedded asset missing: ${_f}")
    endif()
  endforeach()

  set(_embedded_header "${FS_GEN_DIR}/embedded_assets.hpp")
  file(WRITE "${_embedded_header}"
"/* Auto-generated: do not edit. */\n#pragma once\nnamespace fly_score_embedded {\n${_asset_entries}}\n")

  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_EMBEDDED_DEFAULTS=1)
endif()

# ---------------------------------------------------------------------------
# Plugin packaging / install (let template helpers handle it)
# ---------------------------------------------------------------------------
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES
  OUTPUT_NAME ${_name}
)
