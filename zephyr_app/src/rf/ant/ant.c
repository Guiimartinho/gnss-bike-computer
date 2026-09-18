/**
 * @file ant.c
 * @brief ANT radio stack start-up on the sdk-ant add-on
 */

#include <zephyr/logging/log.h>

#include <ant_init.h>
#include <ant_key_manager.h>

#include "rf/ant.h"

LOG_MODULE_REGISTER(rf_ant, CONFIG_LOG_DEFAULT_LEVEL);

/* ANT+ network number, ANTPLUS_NETWORK_NUMBER in legacy/rf/ant.h:10 */
#define ANTPLUS_NETWORK_NUMBER 0U

app_err_t rf_ant_init(void)
{
    ant_err_t err = ant_init();

    if (err != 0) {
        LOG_ERR("ant_init failed: %d", (int)err);
        return APP_ERR_IO;
    }

    err = ant_plus_key_set(ANTPLUS_NETWORK_NUMBER);
    if (err != 0) {
        LOG_ERR("ant_plus_key_set failed: %d", (int)err);
        return APP_ERR_IO;
    }

    LOG_INF("ANT stack started, ANT+ key on network %u", ANTPLUS_NETWORK_NUMBER);
    return APP_OK;
}
