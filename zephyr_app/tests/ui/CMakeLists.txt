# Host renderer of the user interface (docs/18-interface-telas.md).
#
# Builds the LVGL of the NCS and the interface in zephyr_app/src/ui with the
# PC's GCC (MinGW on Windows, GCC on Linux) and draws every screen to PPM with
# checks on the panel colours and on labels out of their boxes. The same C
# code goes into the firmware.
#
#   cmake -S zephyr_app/tests/ui -B build/ui -G Ninja
#   cmake --build build/ui
#   build/ui/ui_render build/ui/out
#
# tools/ui/render_screens.py runs these steps and turns the PPM files into
# the PNG sheets of docs/img/telas-lvgl.

cmake_minimum_required(VERSION 3.20)
project(gnss_ui_render LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED LVGL_DIR)
    if(DEFINED ENV{NCS_LVGL})
        set(LVGL_DIR $ENV{NCS_LVGL})
    else()
        set(LVGL_DIR C:/ncs/v3.3.0/modules/lib/gui/lvgl)
    endif()
endif()
if(NOT EXISTS ${LVGL_DIR}/lvgl.h)
    message(FATAL_ERROR "LVGL not found in ${LVGL_DIR}: pass -DLVGL_DIR=<NCS>/modules/lib/gui/lvgl")
endif()

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# LVGL with the configuration of this folder
file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE=1)
target_compile_options(lvgl PRIVATE -w)

# The interface, as the firmware builds it
file(GLOB UI_SOURCES ${APP_DIR}/src/ui/*.c ${APP_DIR}/src/ui/fonts/*.c)
add_library(ui STATIC ${UI_SOURCES})
target_include_directories(ui PUBLIC ${APP_DIR}/include ${APP_DIR}/src/ui)
target_link_libraries(ui PUBLIC lvgl)
target_compile_options(ui PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Werror
    -Werror=implicit-function-declaration
    -Werror=return-type
)

add_executable(ui_render ui_render.c ui_samples.c)
target_link_libraries(ui_render PRIVATE ui m)
target_compile_options(ui_render PRIVATE -Wall -Wextra -Wno-unused-parameter -Werror)
