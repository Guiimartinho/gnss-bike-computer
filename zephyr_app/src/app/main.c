/**
 * @file main.c
 * @brief Boot of the GNSS bike computer (docs/16-arquitetura-firmware.md, Partida)
 *
 * main() prepares what the services share (crash record, settings, task
 * watchdog), sets up the model modules and starts the service threads in
 * the order of docs/16: storage and settings first, then energy, sensors,
 * GNSS, radio, the model and the screen. Then it tells the system machine
 * that the boot is over and returns; the services run on their own.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_types.h"
#include "app/app_services.h"
#include "app/app_svc.h"
#include "model/crash_recovery.h"
#include "model/user_settings.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
    LOG_INF("GNSS bike computer %u.%u.%u", (unsigned int)APP_VERSION_MAJOR,
            (unsigned int)APP_VERSION_MINOR, (unsigned int)APP_VERSION_PATCH);

    /* a watchdog that survived a soft reset keeps counting */
    app_wdt_feed_if_running();

    /* the reset cause and a crash record from before (FDIR) */
    if (crash_recovery_init() != APP_OK) {
        LOG_WRN("crash record unavailable");
    }

    /* the settings the model reads at start (FTP, weight, paired sensors) */
    user_settings_t *settings = user_settings_get_global();

    if ((user_settings_init(settings) != APP_OK) || (user_settings_enforce(settings) != APP_OK)) {
        LOG_WRN("settings unavailable: defaults in use");
    }

    app_wdt_feed_if_running();
    app_wdt_init();

    model_svc_init();

    storage_svc_start();
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
    usb_svc_start();
#endif
    power_svc_start();
    sensors_svc_start();
    gnss_svc_start();
    radio_svc_start();
    model_svc_start();
    ui_svc_start();

    power_svc_ready();
    LOG_INF("services started");
    return 0;
}
