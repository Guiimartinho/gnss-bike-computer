/**
 * @file util.h
 * @brief Host-test shim for <zephyr/sys/util.h>: the macros the SMF header uses.
 *
 * IF_ENABLED(), COND_CODE_1() and IS_ENABLED() with the same preprocessor
 * technique as zephyr/include/zephyr/sys/util_internal.h: a flag defined as
 * 1 pastes into a name that expands to a comma, which shifts the argument
 * that gets picked.
 */

#ifndef HOST_SHIM_ZEPHYR_SYS_UTIL_H
#define HOST_SHIM_ZEPHYR_SYS_UTIL_H

#include <stddef.h>
#include <stdint.h>

#define Z_HOST_XXXX1 Z_HOST_YYYY,

#define Z_HOST_DEBRACKET(...) __VA_ARGS__
#define Z_HOST_GET_ARG2_DEBRACKET(ignore_this, val, ...) Z_HOST_DEBRACKET val
#define Z_HOST_COND_CODE(one_or_two_args, _if_code, _else_code)                                   \
    Z_HOST_GET_ARG2_DEBRACKET(one_or_two_args _if_code, _else_code)
#define Z_HOST_COND_CODE_1(_flag, _if_1_code, _else_code)                                         \
    Z_HOST_COND_CODE(Z_HOST_XXXX##_flag, _if_1_code, _else_code)

#define COND_CODE_1(_flag, _if_1_code, _else_code) Z_HOST_COND_CODE_1(_flag, _if_1_code, _else_code)
#define IF_ENABLED(_flag, _code) COND_CODE_1(_flag, _code, ())

#define Z_HOST_IS_ENABLED3(ignore_this, val, ...) val
#define Z_HOST_IS_ENABLED2(one_or_two_args) Z_HOST_IS_ENABLED3(one_or_two_args 1, 0)
#define Z_HOST_IS_ENABLED1(config_macro) Z_HOST_IS_ENABLED2(Z_HOST_XXXX##config_macro)
#define IS_ENABLED(config_macro) Z_HOST_IS_ENABLED1(config_macro)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

#endif /* HOST_SHIM_ZEPHYR_SYS_UTIL_H */
