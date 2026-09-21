# SPDX-License-Identifier: Apache-2.0
# GNSS bike computer: the Zephyr port of stravaV10 (docs/16-arquitetura-firmware.md)

cmake_minimum_required(VERSION 3.20.0)

# Board: chosen with west build -b (tools/fw/fw.sh and build.bat default to
# nrf52840dk/nrf52840). Zephyr adds boards/<board>.overlay by name, so each
# target keeps its pins in its own overlay. BOARD_ROOT lets a board of our
# own live in boards/.
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Drivers of our own, as a Zephyr module inside the application: the
# memory-in-pixel display in portrait (modules/gnss_drivers, docs/05)
list(APPEND EXTRA_ZEPHYR_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/gnss_drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(gnss_bike_computer VERSION 2.0.0 LANGUAGES C)

# Compile options following MISRA and 0.memory standards
target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Werror=implicit-function-declaration
    -Werror=return-type
    -Wno-unused-parameter
    -ffunction-sections
    -fdata-sections
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/model
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rf
)

# =============================================================================
# Application: boot, zbus channels, service framework (docs/16)
# =============================================================================
target_sources(app PRIVATE
    src/app/main.c
    src/app/app_channels.c
    src/app/app_svc.c
)

# =============================================================================
# Services, one thread each (docs/16, Threads)
# =============================================================================
target_sources(app PRIVATE
    src/svc/power/battery.c
    src/svc/power/charge.c
    src/svc/power/power_svc.c
    src/svc/power/sys_fsm.c
    src/svc/gnss/gnss_power.c
    src/svc/gnss/gnss_svc.c
    src/svc/sensors/sensors_svc.c
    src/svc/sensors/tilt.c
    src/svc/radio/radio_svc.c
    src/svc/storage/storage_svc.c
    src/svc/model/model_svc.c
    src/svc/model/model_ui.c
    src/svc/ui/backlight.c
    src/svc/ui/ui_input.c
    src/svc/ui/ui_svc.c
)

# =============================================================================
# Model: the legacy algorithms (docs/06)
# =============================================================================
target_sources(app PRIVATE
    src/model/attitude.c
    src/model/cmd_parser.c
    src/model/crash_recovery.c
    src/model/file_policy.c
    src/model/dfu_state.c
    src/model/kalman.c
    src/model/kalman_altitude.c
    src/model/liste_points.c
    src/model/map_project.c
    src/model/gpx_scan.c
    src/model/route_file.c
    src/model/route_profile.c
    src/model/loc_source.c
    src/model/locator.c
    src/model/activity.c
    src/model/climb.c
    src/model/radar.c
    src/model/radar_wire.c
    src/model/fit_encode.c
    src/model/parcours.c
    src/model/power_scheduler.c
    src/model/distance.c
    src/model/power_estimate.c
    src/model/power_zone.c
    src/model/rr_zone.c
    src/model/sd_logger.c
    src/model/segment.c
    src/model/segment_file.c
    src/model/suffer_score.c
    src/model/udmatrix.c
    src/model/user_settings.c
    src/model/vecteur.c
)

# =============================================================================
# Radio: BLE central and peripheral, sensor clients (docs/07)
# =============================================================================
target_sources(app PRIVATE
    src/app/app_cmd.c
    src/rf/dfu.c
    src/rf/file_xfer.c
    src/rf/ble/ble_manager.c
    src/rf/ble/ble_nus.c
    src/rf/ble/ble_lns.c
    src/rf/ble_radar_client.c
    src/rf/ble_hrs_client.c
    src/rf/ble_bsc_client.c
    src/rf/ble_fec_client.c
    src/rf/ble_komoot_client.c
)

# =============================================================================
# Interface: the LVGL screens (docs/18), the same code as the host renderer
# of tests/ui
# =============================================================================
target_sources_ifdef(CONFIG_LVGL app PRIVATE
    src/ui/ui_core.c
    src/ui/ui_fields.c
    src/ui/ui_fmt.c
    src/ui/ui_scr_crs.c
    src/ui/ui_scr_menus.c
    src/ui/ui_scr_modes.c
    src/ui/ui_scr_system.c
    src/ui/ui_text.c
    src/ui/ui_theme.c
    src/ui/ui_widgets.c
    src/ui/fonts/ui_font_b12.c
    src/ui/fonts/ui_font_b14.c
    src/ui/fonts/ui_font_b16.c
    src/ui/fonts/ui_font_b18.c
    src/ui/fonts/ui_font_b22.c
    src/ui/fonts/ui_font_b28.c
    src/ui/fonts/ui_font_b32.c
    src/ui/fonts/ui_font_b44.c
    src/ui/fonts/ui_font_b64.c
    src/ui/fonts/ui_font_r10.c
    src/ui/fonts/ui_font_r12.c
)

# ANT on the sdk-ant add-on, only in ANT builds (ANT=1 in tools/fw/fw.sh)
target_sources_ifdef(CONFIG_ANT app PRIVATE
    src/rf/ant/ant.c
    src/rf/ant/radar_ant.c
)

# The page decoding of the ANT+ Bike Radar is not in this repository
# (zephyr_app/include/rf/radar_ant.h): when the owner puts it there, it is
# compiled and its strong definition takes over from the weak one.
if(CONFIG_ANT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/rf/ant/radar_pages.c)
    target_sources(app PRIVATE src/rf/ant/radar_pages.c)
    message(STATUS "ANT+ radar: local profile found")
endif()

# =============================================================================
# Update over the air: src/rf/dfu.c includes img_mgmt.h, which pulls
# bootutil/image.h from the MCUboot side library
# (bootloader/mcuboot/boot/bootutil/zephyr/CMakeLists.txt).
# =============================================================================
if(CONFIG_MCUBOOT_BOOTUTIL_LIB)
    zephyr_link_libraries(MCUBOOT_BOOTUTIL)
endif()

# =============================================================================
# USB service: only where the USB device stack is on (the nRF54LM20 target)
# =============================================================================
if(CONFIG_USB_DEVICE_STACK_NEXT)
    target_sources(app PRIVATE src/svc/usb/usb_svc.c)
endif()
