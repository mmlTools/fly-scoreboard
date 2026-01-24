cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options (these are mainly here so your CI flags are "used")
# ---------------------------------------------------------------------------
option(ENABLE_FRONTEND_API "Use obs-frontend-api for dock, hotkeys, browser auto-setup" ON)
option(ENABLE_QT           "Use Qt for dock UI and dialogs"                             ON)
option(EMBED_DEFAULT_ASSETS "Embed data/overlay + locale into binary"                   ON)

# This plugin *requires* Qt and frontend API; don't allow disabling them.
if(NOT ENABLE_QT)
  message(FATAL_ERROR "ENABLE_QT=OFF is not supported for fly-scoreboard; Qt is required.")
endif()

if(NOT ENABLE_FRONTEND_API)
  message(FATAL_ERROR "ENABLE_FRONTEND_API=OFF is not supported for fly-scoreboard; obs-frontend-api is required.")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

if(MSVC)
  add_compile_options(/WX- /utf-8)
endif()

# OBS 32+: use dock-by-id API
add_compile_definitions(HAVE_OBS_DOCK_BY_ID=1)

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

# config.hpp (PLUGIN_NAME / PLUGIN_VERSION)
configure_file(
  "${FS_INC_DIR}/config.hpp.in"
  "${FS_INC_DIR}/config.hpp"
  @ONLY
)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_library(${CMAKE_PROJECT_NAME} MODULE)

# libobs
find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# ---------------------------------------------------------------------------
# Frontend API (REQUIRED)
# ---------------------------------------------------------------------------
find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
# Define macro for your #ifdefs
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_FRONTEND_API=1)

# ---------------------------------------------------------------------------
# Qt (dock & dialogs) – REQUIRED
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

# Define macro for your #ifdefs (even if you don't currently use it)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_QT=1)

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
  ${FS_SRC_DIR}/fly_score_state.cpp
  ${FS_SRC_DIR}/fly_score_qt_helpers.cpp
  ${FS_SRC_DIR}/fly_score_obs_helpers.cpp
  ${FS_SRC_DIR}/fly_score_logo_helpers.cpp
  ${FS_SRC_DIR}/fly_score_paths.cpp
)

list(APPEND OBS_FLY_SCORE_SRC
  ${FS_SRC_DIR}/fly_score_dock.cpp
  ${FS_INC_DIR}/fly_score_dock.hpp
  ${FS_SRC_DIR}/widget.cpp
  ${FS_INC_DIR}/widget.hpp
  ${FS_SRC_DIR}/fly_score_teams_dialog.cpp
  ${FS_INC_DIR}/fly_score_teams_dialog.hpp
  ${FS_SRC_DIR}/fly_score_fields_dialog.cpp
  ${FS_INC_DIR}/fly_score_fields_dialog.hpp
  ${FS_SRC_DIR}/fly_score_timers_dialog.cpp
  ${FS_INC_DIR}/fly_score_timers_dialog.hpp
  ${FS_SRC_DIR}/fly_score_hotkeys_dialog.cpp
  ${FS_INC_DIR}/fly_score_hotkeys_dialog.hpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${OBS_FLY_SCORE_SRC})

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  PLUGIN_NAME_STR="${_name}"
  PLUGIN_VERSION_STR="${_version}"
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED YES
)

# ---------------------------------------------------------------------------
# Embedded overlay/locale assets
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
# Let obs-plugintemplate handle bundles & install layout
# ---------------------------------------------------------------------------
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES
  OUTPUT_NAME ${_name}
)
