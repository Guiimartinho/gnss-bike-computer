/**
 * @file reboot.h
 * @brief Host shim of sys_reboot()
 *
 * A test that reboots would end the process: the shim counts the calls so a
 * test can check that the code asked for a reset.
 */

#ifndef SHIM_ZEPHYR_SYS_REBOOT_H
#define SHIM_ZEPHYR_SYS_REBOOT_H

#define SYS_REBOOT_COLD 0
#define SYS_REBOOT_WARM 1

extern unsigned int host_reboot_count;

void sys_reboot(int type);

#endif /* SHIM_ZEPHYR_SYS_REBOOT_H */
