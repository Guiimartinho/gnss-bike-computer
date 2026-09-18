/**
 * @file kernel.h
 * @brief Host-test shim for <zephyr/kernel.h>.
 *
 * Only what the logic modules under test use: mutexes that never block and a
 * controllable uptime (host_uptime_set/advance in support/host_kernel.c when a
 * test links it).
 */

#ifndef HOST_SHIM_ZEPHYR_KERNEL_H
#define HOST_SHIM_ZEPHYR_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int64_t ticks;
} k_timeout_t;

#define K_FOREVER  ((k_timeout_t){ .ticks = -1 })
#define K_NO_WAIT  ((k_timeout_t){ .ticks = 0 })
#define K_MSEC(ms) ((k_timeout_t){ .ticks = (ms) })

struct k_mutex {
    int lock_count;
};

#define K_MUTEX_DEFINE(name) struct k_mutex name = { 0 }

static inline int k_mutex_init(struct k_mutex *mutex)
{
    mutex->lock_count = 0;
    return 0;
}

static inline int k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout)
{
    (void)timeout;
    mutex->lock_count++;
    return 0;
}

static inline int k_mutex_unlock(struct k_mutex *mutex)
{
    mutex->lock_count--;
    return 0;
}

uint32_t k_uptime_get_32(void);
int64_t k_uptime_get(void);

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) ((void)(x))
#endif

#endif /* HOST_SHIM_ZEPHYR_KERNEL_H */
