/**
 * @file memlcd.h
 * @brief What the memory-in-pixel display driver adds to the display API
 */

#ifndef MEMLCD_H
#define MEMLCD_H

#include <stdint.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set how often the COM inverts
 *
 * Rising edges of EXTCOMIN per second, or serial VCOM inversions per second
 * without the pin. The datasheets allow 1 to 140 (JDI) and ask for the COM
 * near 60 Hz with the light on: 120 edges, since the COM is half of it.
 *
 * @return 0, or -EINVAL out of 1 to 140
 */
int memlcd_set_com_hz(const struct device *dev, uint32_t hz);

/**
 * @brief Clear the panel memory (all clear mode) and the frame buffer
 */
int memlcd_clear(const struct device *dev);

/**
 * @brief Clear, turn the picture off and stop the COM, before the power goes
 *
 * The panel must not keep a picture without the COM inverting (datasheet
 * power-off sequence: clear, DISP low, then the supply).
 */
int memlcd_power_off(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* MEMLCD_H */
