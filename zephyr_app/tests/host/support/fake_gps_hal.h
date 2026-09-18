/**
 * @file fake_gps_hal.h
 * @brief Fake UART/GPIO/EPO layer to test drivers/gps/gps_mgmt.c on the host.
 *
 * Lines queued with fake_uart_push_line() are delivered to the registered
 * line callback when gps_mgmt_process() calls hal_uart_process(), which is
 * how the firmware runs since NMEA parsing left the UART ISR.
 */

#ifndef FAKE_GPS_HAL_H
#define FAKE_GPS_HAL_H

#include <stdbool.h>

#include "hal/hal_gpio.h"

/** Clears queued lines and GPIO states (keeps the registered line callback). */
void fake_gps_hal_reset(void);

/** Queues one UART line ("$GPRMC,...*CS", without CR LF). */
void fake_uart_push_line(const char *line);

/** Last level written to a GPIO with hal_gpio_set() (logical, as in the DT). */
bool fake_gpio_level(hal_gpio_pin_t pin);

/** Level returned by hal_gpio_get() for a pin (e.g. the GPS FIX pin). */
void fake_gpio_set_input(hal_gpio_pin_t pin, bool level);

#endif /* FAKE_GPS_HAL_H */
