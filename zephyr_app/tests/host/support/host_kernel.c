/**
 * @file host_kernel.c
 * @brief Controllable uptime for host tests (see shim/zephyr/kernel.h).
 */

#include <zephyr/kernel.h>

#include "host_kernel.h"

static int64_t s_uptime_ms;

void host_uptime_set(int64_t uptime_ms)
{
    s_uptime_ms = uptime_ms;
}

void host_uptime_advance(int64_t delta_ms)
{
    s_uptime_ms += delta_ms;
}

int64_t k_uptime_get(void)
{
    return s_uptime_ms;
}

uint32_t k_uptime_get_32(void)
{
    return (uint32_t)s_uptime_ms;
}

int32_t k_msleep(int32_t ms)
{
    s_uptime_ms += ms;
    return 0;
}

/* Reboots asked for by the code under test (shim/zephyr/sys/reboot.h) */
unsigned int host_reboot_count;

void sys_reboot(int type)
{
    (void)type;
    host_reboot_count++;
}
