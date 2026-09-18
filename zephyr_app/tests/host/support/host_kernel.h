/**
 * @file host_kernel.h
 * @brief Controls the fake uptime returned by k_uptime_get() in host tests.
 */

#ifndef HOST_KERNEL_H
#define HOST_KERNEL_H

#include <stdint.h>

void host_uptime_set(int64_t uptime_ms);
void host_uptime_advance(int64_t delta_ms);

#endif /* HOST_KERNEL_H */
