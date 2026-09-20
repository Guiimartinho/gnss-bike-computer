# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)

add_subdirectory_ifdef(CONFIG_MEMLCD drivers/display)
add_subdirectory_ifdef(CONFIG_GNSS_FUEL_GAUGE_MAX17262 drivers/fuel_gauge)
add_subdirectory_ifdef(CONFIG_GNSS_CHARGER_AEM10900 drivers/charger)
add_subdirectory_ifdef(CONFIG_GNSS_UBLOX_M10 drivers/gnss)
