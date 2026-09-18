/**
 * @file vue_fec.h
 * @brief FE-C (Indoor trainer) mode display interface
 *
 * Provides display functions for indoor trainer mode.
 * Follows MISRA C:2012 guidelines.
 */

#ifndef VUE_VUE_FEC_H
#define VUE_VUE_FEC_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

/** FEC display page */
typedef enum {
    VUE_FEC_PAGE_MAIN = 0,      /**< Main metrics display */
    VUE_FEC_PAGE_ZONES,         /**< Power zones histogram */
    VUE_FEC_PAGE_DETAILS        /**< Detailed metrics */
} vue_fec_page_t;

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

/**
 * @brief Initialize FEC view module
 */
void vue_fec_init(void);

/**
 * @brief Render FEC display
 * @note Call this periodically to update the display
 */
void vue_fec_render(void);

/**
 * @brief Handle button event in FEC mode
 * @param event Button event
 */
void vue_fec_handle_button(btn_event_t event);

/**
 * @brief Get current FEC page
 * @return Current page
 */
vue_fec_page_t vue_fec_get_page(void);

/**
 * @brief Set FEC page
 * @param page Page to display
 */
void vue_fec_set_page(vue_fec_page_t page);

#ifdef __cplusplus
}
#endif

#endif /* VUE_VUE_FEC_H */
