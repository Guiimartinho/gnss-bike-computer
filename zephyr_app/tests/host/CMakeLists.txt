# Testes de host dos módulos de lógica do zephyr_app.
#
# Compila os .c de zephyr_app/src com o GCC do PC (MinGW no Windows, GCC no
# Linux) contra shims mínimos dos headers do Zephyr (tests/host/shim) e roda
# cada conjunto com Unity + CTest. Nada de hardware nem de kernel.
#
#   cmake --preset host-tests
#   cmake --build --preset host-tests
#   ctest --preset host-tests

cmake_minimum_required(VERSION 3.20)
project(gnss_host_tests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(CTest)
include(FetchContent)

FetchContent_Declare(unity
    URL      https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.1.tar.gz
    URL_HASH SHA256=b41a66d45a6b99758fb3202ace6178177014d52fc524bf1f72687d93e9867292
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(UNITY_EXTENSION_FIXTURE OFF CACHE BOOL "" FORCE)
set(UNITY_EXTENSION_MEMORY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(unity)
# Unity also compares doubles; the firmware only uses float.
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Zephyr of the NCS, for the libraries that run unchanged on the host (SMF).
# Without it, the suites that need it are skipped.
if(NOT DEFINED ZEPHYR_BASE)
    if(DEFINED ENV{ZEPHYR_BASE})
        set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})
    else()
        set(ZEPHYR_BASE C:/ncs/v3.3.0/zephyr)
    endif()
endif()
set(ZEPHYR_SMF ${ZEPHYR_BASE}/lib/smf/smf.c)

# Include paths and flags shared by the code under test and the tests.
add_library(host_env INTERFACE)
target_include_directories(host_env INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${APP_DIR}/include
    ${APP_DIR}/modules/gnss_drivers/include
)
target_compile_definitions(host_env INTERFACE GNSS_HOST_TEST=1)
# Same warning set as zephyr_app/CMakeLists.txt, plus -Werror.
target_compile_options(host_env INTERFACE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Werror
    -Werror=implicit-function-declaration
    -Werror=return-type
)

# gnss_add_test(<name> SOURCES <files under zephyr_app/src> [SUPPORT <files under support/>]
#               [ZEPHYR <absolute paths of Zephyr sources>])
function(gnss_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;SUPPORT;ZEPHYR" ${ARGN})
    set(code)
    foreach(src IN LISTS T_SOURCES)
        list(APPEND code ${APP_DIR}/src/${src})
    endforeach()
    foreach(src IN LISTS T_SUPPORT)
        list(APPEND code ${CMAKE_CURRENT_SOURCE_DIR}/support/${src})
    endforeach()
    add_executable(${name} ${name}.c ${code} ${T_ZEPHYR})
    target_link_libraries(${name} PRIVATE unity host_env m)
    if(T_ZEPHYR)
        # The Zephyr headers these sources need, searched after the shims
        target_compile_options(${name} PRIVATE -idirafter ${ZEPHYR_BASE}/include)
        target_compile_definitions(${name} PRIVATE
            CONFIG_SMF_ANCESTOR_SUPPORT=1
            CONFIG_SMF_INITIAL_TRANSITION=1)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

gnss_add_test(test_vecteur SOURCES model/vecteur.c)
gnss_add_test(test_liste_points SOURCES model/liste_points.c model/vecteur.c)
gnss_add_test(test_segment
    SOURCES model/segment.c model/liste_points.c model/vecteur.c model/segment_file.c
    SUPPORT host_kernel.c host_fs.c)
gnss_add_test(test_cmd_parser SOURCES model/cmd_parser.c)
gnss_add_test(test_file_policy SOURCES model/file_policy.c model/segment_file.c)
gnss_add_test(test_e2e_route
    SOURCES model/parcours.c model/vecteur.c model/file_policy.c model/segment_file.c
            model/map_project.c model/route_profile.c model/route_file.c model/gpx_scan.c
    SUPPORT host_kernel.c host_fs.c)
gnss_add_test(test_dfu_state SOURCES model/dfu_state.c)
gnss_add_test(test_map_project SOURCES model/map_project.c model/vecteur.c)
gnss_add_test(test_route_profile SOURCES model/route_profile.c model/map_project.c model/vecteur.c)
gnss_add_test(test_route_file SOURCES model/route_file.c)
gnss_add_test(test_gpx_scan SOURCES model/gpx_scan.c)
gnss_add_test(test_fit_encode SOURCES model/fit_encode.c)
gnss_add_test(test_activity SOURCES model/activity.c)
gnss_add_test(test_climb SOURCES model/climb.c)
gnss_add_test(test_radar SOURCES model/radar.c model/radar_wire.c)
gnss_add_test(test_incident SOURCES model/incident.c)
gnss_add_test(test_loc_arbiter SOURCES model/loc_arbiter.c)
gnss_add_test(test_csc_calc SOURCES model/csc_calc.c)
gnss_add_test(test_ftms_parse SOURCES model/ftms_parse.c)
gnss_add_test(test_e2e_activity SOURCES model/activity.c model/fit_encode.c)
gnss_add_test(test_parcours
    SOURCES model/parcours.c model/vecteur.c model/route_file.c model/gpx_scan.c
    SUPPORT host_kernel.c host_fs.c)
target_compile_definitions(test_parcours PRIVATE
    PARCOURS_DB_DIR="${APP_DIR}/../tools/TDD/DB")
gnss_add_test(test_segment_file SOURCES model/segment_file.c)
target_compile_definitions(test_segment_file PRIVATE
    SEGMENT_DB_DIR="${APP_DIR}/../tools/TDD/DB")
gnss_add_test(test_distance SOURCES model/distance.c model/vecteur.c)
gnss_add_test(test_udmatrix SOURCES model/udmatrix.c)
gnss_add_test(test_kalman_altitude SOURCES model/kalman_altitude.c model/udmatrix.c)
gnss_add_test(test_crash_recovery SOURCES model/crash_recovery.c SUPPORT host_kernel.c)
gnss_add_test(test_attitude
    SOURCES model/attitude.c model/distance.c model/vecteur.c model/power_estimate.c
            model/kalman_altitude.c model/udmatrix.c model/crash_recovery.c
    SUPPORT host_kernel.c)
gnss_add_test(test_power_estimate SOURCES model/power_estimate.c)
gnss_add_test(test_alerts SOURCES model/alerts.c)
gnss_add_test(test_cps_parse SOURCES model/cps_parse.c)
gnss_add_test(test_power_metrics SOURCES model/power_metrics.c)
gnss_add_test(test_rr_zone SOURCES model/rr_zone.c)
gnss_add_test(test_power_zone SOURCES model/power_zone.c)
gnss_add_test(test_suffer_score SOURCES model/suffer_score.c)
gnss_add_test(test_sd_logger SOURCES model/sd_logger.c SUPPORT host_kernel.c host_fs.c)
gnss_add_test(test_ui_fmt SOURCES ui/ui_fmt.c)
gnss_add_test(test_tilt SOURCES svc/sensors/tilt.c)
gnss_add_test(test_backlight SOURCES svc/ui/backlight.c)
# The display driver lives in a Zephyr module next to src/
gnss_add_test(test_memlcd SOURCES ../modules/gnss_drivers/drivers/display/memlcd_frame.c)
gnss_add_test(test_max17262)
gnss_add_test(test_aem10900)
gnss_add_test(test_ubx_m10 SOURCES ../modules/gnss_drivers/drivers/gnss/ubx_m10.c)
gnss_add_test(test_gnss_power SOURCES svc/gnss/gnss_power.c)
gnss_add_test(test_battery SOURCES svc/power/battery.c)
gnss_add_test(test_charge SOURCES svc/power/charge.c)
gnss_add_test(test_power_scheduler
    SOURCES model/power_scheduler.c
    SUPPORT host_kernel.c)
if(EXISTS ${ZEPHYR_SMF})
    gnss_add_test(test_sys_fsm
        SOURCES svc/power/sys_fsm.c model/power_scheduler.c
        SUPPORT host_kernel.c
        ZEPHYR ${ZEPHYR_SMF})
    gnss_add_test(test_mode_fsm
        SOURCES model/mode_fsm.c
        ZEPHYR ${ZEPHYR_SMF})
else()
    message(WARNING "Zephyr SMF not found in ${ZEPHYR_BASE}: test_sys_fsm and test_mode_fsm skipped (pass -DZEPHYR_BASE=...)")
endif()
