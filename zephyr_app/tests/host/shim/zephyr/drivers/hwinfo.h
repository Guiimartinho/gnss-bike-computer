/**
 * @file hwinfo.h
 * @brief Host shim of the Zephyr hardware information API
 *
 * The host tests do not have a reset cause: the reader answers that it does
 * not know, and the tests exercise the rest of the crash recovery.
 */

#ifndef SHIM_ZEPHYR_DRIVERS_HWINFO_H
#define SHIM_ZEPHYR_DRIVERS_HWINFO_H

#include <errno.h>
#include <stdint.h>

#define RESET_PIN               (1U << 0)
#define RESET_SOFTWARE          (1U << 1)
#define RESET_BROWNOUT          (1U << 2)
#define RESET_POR               (1U << 3)
#define RESET_WATCHDOG          (1U << 4)
#define RESET_DEBUG             (1U << 5)
#define RESET_SECURITY          (1U << 6)
#define RESET_LOW_POWER_WAKE    (1U << 7)
#define RESET_CPU_LOCKUP        (1U << 8)
#define RESET_PARITY            (1U << 9)
#define RESET_PLL               (1U << 10)
#define RESET_CLOCK             (1U << 11)
#define RESET_HARDWARE          (1U << 12)
#define RESET_USER              (1U << 13)
#define RESET_TEMPERATURE       (1U << 14)

static inline int hwinfo_get_reset_cause(uint32_t *cause)
{
    (void)cause;

    return -ENOSYS;
}

static inline int hwinfo_clear_reset_cause(void)
{
    return -ENOSYS;
}

#endif /* SHIM_ZEPHYR_DRIVERS_HWINFO_H */
