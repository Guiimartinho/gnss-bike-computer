/**
 * @file log.h
 * @brief Host-test shim for <zephyr/logging/log.h>: logging compiles away.
 */

#ifndef HOST_SHIM_ZEPHYR_LOGGING_LOG_H
#define HOST_SHIM_ZEPHYR_LOGGING_LOG_H

#define LOG_MODULE_REGISTER(...)
#define LOG_MODULE_DECLARE(...)

#define LOG_ERR(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define LOG_DBG(...) ((void)0)

#define LOG_HEXDUMP_ERR(...) ((void)0)
#define LOG_HEXDUMP_WRN(...) ((void)0)
#define LOG_HEXDUMP_INF(...) ((void)0)
#define LOG_HEXDUMP_DBG(...) ((void)0)

#endif /* HOST_SHIM_ZEPHYR_LOGGING_LOG_H */
