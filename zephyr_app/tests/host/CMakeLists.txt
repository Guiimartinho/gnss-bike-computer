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

# Include paths and flags shared by the code under test and the tests.
add_library(host_env INTERFACE)
target_include_directories(host_env INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${APP_DIR}/include
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

# gnss_add_test(<name> SOURCES <files under zephyr_app/src> [SUPPORT <files under support/>])
function(gnss_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;SUPPORT" ${ARGN})
    set(code)
    foreach(src IN LISTS T_SOURCES)
        list(APPEND code ${APP_DIR}/src/${src})
    endforeach()
    foreach(src IN LISTS T_SUPPORT)
        list(APPEND code ${CMAKE_CURRENT_SOURCE_DIR}/support/${src})
    endforeach()
    add_executable(${name} ${name}.c ${code})
    target_link_libraries(${name} PRIVATE unity host_env m)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

gnss_add_test(test_vecteur SOURCES model/vecteur.c)
gnss_add_test(test_power_zone SOURCES model/power_zone.c)
gnss_add_test(test_suffer_score SOURCES model/suffer_score.c)
gnss_add_test(test_nmea_parser SOURCES drivers/gps/nmea_parser.c)
gnss_add_test(test_gps_mgmt
    SOURCES drivers/gps/gps_mgmt.c drivers/gps/nmea_parser.c
    SUPPORT host_kernel.c fake_gps_hal.c)
