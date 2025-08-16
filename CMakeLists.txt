cmake_minimum_required(VERSION 3.16)
project(desktop_audio_visualizer)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

# Expect OBS installed with dev headers / cmake config.
# Example on Windows:
#   -DOBS_PATH="C:/Program Files/obs-studio"
# or set OBS_PATH env var.
if(DEFINED ENV{OBS_PATH} AND NOT OBS_PATH)
  set(OBS_PATH $ENV{OBS_PATH})
endif()
if(NOT OBS_PATH)
  set(OBS_PATH "C:/Program Files/obs-studio")
endif()

list(APPEND CMAKE_PREFIX_PATH "${OBS_PATH}/cmake")
find_package(libobs REQUIRED)

add_library(desktop_audio_visualizer MODULE
  src/plugin.c
  src/visualizer_source.c
  src/dsp.c
  src/dsp.h
)

target_include_directories(desktop_audio_visualizer PRIVATE ${LIBOBS_INCLUDE_DIRS})
target_link_libraries(desktop_audio_visualizer PRIVATE libobs)

# Install to OBS plugin directory layout
if(WIN32)
  set(OBS_PLUGIN_DIR "${OBS_PATH}/obs-plugins/64bit")
  set(OBS_DATA_DIR "${OBS_PATH}/data/obs-plugins/desktop_audio_visualizer")
elseif(APPLE)
  set(OBS_PLUGIN_DIR "${CMAKE_BINARY_DIR}")
  set(OBS_DATA_DIR "${CMAKE_BINARY_DIR}/data")
else()
  set(OBS_PLUGIN_DIR "${CMAKE_BINARY_DIR}")
  set(OBS_DATA_DIR "${CMAKE_BINARY_DIR}/data")
endif()

install(TARGETS desktop_audio_visualizer DESTINATION "${OBS_PLUGIN_DIR}")
install(DIRECTORY data/ DESTINATION "${OBS_DATA_DIR}")
