# SPDX-License-Identifier: Apache-2.0
# GNSS bike computer: the Zephyr port of stravaV10 (docs/16-arquitetura-firmware.md)

cmake_minimum_required(VERSION 3.20.0)

# Board: chosen with west build -b (tools/fw/fw.sh and build.bat default to
# nrf52840dk/nrf52840). Zephyr adds boards/<board>.overlay by name, so each
# target keeps its pins in its own overlay. BOARD_ROOT lets a board of our
# own live in boards/.
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

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
    src/svc/power/power_svc.c
    src/svc/power/sys_fsm.c
    src/svc/gnss/gnss_svc.c
    src/svc/sensors/sensors_svc.c
    src/svc/sensors/tilt.c
    src/svc/radio/radio_svc.c
    src/svc/storage/storage_svc.c
    src/svc/model/model_svc.c
    src/svc/model/model_ui.c
    src/svc/ui/ui_svc.c
)

# =============================================================================
# Model: the legacy algorithms (docs/06)
# =============================================================================
target_sources(app PRIVATE
    src/model/attitude.c
    src/model/crash_recovery.c
    src/model/kalman.c
    src/model/kalman_altitude.c
    src/model/liste_points.c
    src/model/loc_source.c
    src/model/locator.c
    src/model/parcours.c
    src/model/power_scheduler.c
    src/model/power_zone.c
    src/model/rr_zone.c
    src/model/sd_logger.c
    src/model/segment.c
    src/model/suffer_score.c
    src/model/udmatrix.c
    src/model/user_settings.c
    src/model/vecteur.c
)

# =============================================================================
# Radio: BLE central and peripheral, sensor clients (docs/07)
# =============================================================================
target_sources(app PRIVATE
    src/rf/ble/ble_manager.c
    src/rf/ble/ble_nus.c
    src/rf/ble/ble_lns.c
    src/rf/ble_hrs_client.c
    src/rf/ble_bsc_client.c
    src/rf/ble_fec_client.c
    src/rf/ble_komoot_client.c
)

# ANT on the sdk-ant add-on, only in ANT builds (ANT=1 in tools/fw/fw.sh)
target_sources_ifdef(CONFIG_ANT app PRIVATE
    src/rf/ant/ant.c
)
