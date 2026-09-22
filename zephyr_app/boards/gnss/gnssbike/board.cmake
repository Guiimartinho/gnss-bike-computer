# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=nRF54LM20A_M33" "--speed=4000")
board_runner_args(nrfutil "--nrf-family=NRF54L")

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
