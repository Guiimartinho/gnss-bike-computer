/**
 * @file main.c
 * @brief stravaV10 - Bike GPS Computer with Strava Segments
 *
 * Main entry point for the Zephyr RTOS based firmware.
 * Follows MISRA C:2012 guidelines.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/task_wdt/task_wdt.h>

/*
 * The hardware watchdog behind the task watchdog: watchdog0 of the board
 * (wdt0 on the nRF52840, wdt31 on the nRF54LM20). DT_REG_ADDR gives the
 * address the CPU sees, secure or not.
 */
#if defined(CONFIG_SOC_FAMILY_NORDIC_NRF) && DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay)
#include <hal/nrf_wdt.h>
#define HW_WDT_REGS ((NRF_WDT_Type *)DT_REG_ADDR(DT_ALIAS(watchdog0)))
#endif

#include "app_types.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "hal/hal_uart.h"
#include "drivers/ls027.h"
#include "drivers/baro.h"
#include "drivers/fxos.h"
#include "drivers/stc3100.h"
#include "drivers/gps_mgmt.h"
#include "model/boucle.h"
#include "model/model_lock.h"
#include "model/power_scheduler.h"
#include "model/segment.h"
#include "rf/ble_manager.h"
#include "vue/vue.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** Main loop interval in milliseconds */
#define MAIN_LOOP_INTERVAL_MS       100U

/** Display update interval in milliseconds */
#define DISPLAY_UPDATE_INTERVAL_MS  250U

/** VCOM toggle interval in milliseconds */
#define VCOM_TOGGLE_INTERVAL_MS     1000U

/** Button polling interval in milliseconds */
#define BUTTON_POLL_INTERVAL_MS     20U

/**
 * Task watchdog timeout of each thread, in milliseconds: the legacy WDT
 * (NRFX_WDT_CONFIG_RELOAD_VALUE). Matches CONFIG_TASK_WDT_MIN_TIMEOUT.
 */
#define WDT_TIMEOUT_MS              4000U

/* ==========================================================================
 * Thread Definitions
 * ========================================================================== */

/*
 * main_loop runs the altitude Kalman filter: measurement_update() alone has a
 * 1672-byte frame (udmatrix_t temporaries) and the chain passes 2200 bytes,
 * which overflowed the former 2048-byte stack into the MPU guard. It also
 * runs the GPS pipeline (NMEA parsing and the fix callback) since that left
 * the UART ISR.
 */
#define MAIN_STACK_SIZE     4096U
#define MAIN_PRIORITY       5

/* vue_update() formats floats with snprintf() (~700-850 bytes deep) */
#define DISPLAY_STACK_SIZE  2048U
#define DISPLAY_PRIORITY    7

/*
 * There is no sensor thread: boucle_process() already reads the barometer,
 * IMU and fuel gauge in main_loop, which keeps main_loop the only writer of
 * the model and of the sensor driver caches.
 */
K_THREAD_STACK_DEFINE(main_stack, MAIN_STACK_SIZE);
K_THREAD_STACK_DEFINE(display_stack, DISPLAY_STACK_SIZE);

static struct k_thread main_thread_data;
static struct k_thread display_thread_data;

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** System initialized flag */
static volatile bool system_initialized;

/** Last display update time */
static uint32_t last_display_update;

/** Last VCOM toggle time */
static uint32_t last_vcom_toggle;

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Initialize all hardware
 */
static app_err_t init_hardware(void)
{
    app_err_t err;

    LOG_INF("Initializing hardware...");

    /* GPIO */
    err = hal_gpio_init();
    if (err != APP_OK) {
        LOG_ERR("GPIO init failed: %d", err);
        return err;
    }

    /* I2C */
    err = hal_i2c_init();
    if (err != APP_OK) {
        LOG_ERR("I2C init failed: %d", err);
        return err;
    }

    /* SPI */
    err = hal_spi_init();
    if (err != APP_OK) {
        LOG_ERR("SPI init failed: %d", err);
        return err;
    }

    /* UART */
    err = hal_uart_init();
    if (err != APP_OK) {
        LOG_ERR("UART init failed: %d", err);
        return err;
    }

    LOG_INF("Hardware initialized");
    return APP_OK;
}

/**
 * @brief Initialize all drivers
 */
static app_err_t init_drivers(void)
{
    app_err_t err;

    LOG_INF("Initializing drivers...");

    /* LCD */
    err = ls027_init();
    if (err != APP_OK) {
        LOG_ERR("LCD init failed: %d", err);
        /* Continue anyway - display might recover */
    }

    /* Barometer */
    err = baro_init();
    if (err != APP_OK) {
        LOG_WRN("Barometer init failed: %d", err);
    }

    /* IMU */
    err = fxos_init();
    if (err != APP_OK) {
        LOG_WRN("IMU init failed: %d", err);
    }

    /* Fuel Gauge */
    battery_config_t batt_cfg = {
        .rsense = 100U,
        .capacity_mah = 1500U,
        .voltage_min = 3300U,
        .voltage_max = 4200U,
    };
    err = stc3100_init(&batt_cfg);
    if (err != APP_OK) {
        LOG_WRN("Fuel gauge init failed: %d", err);
    }

    /* GPS */
    err = gps_mgmt_init();
    if (err != APP_OK) {
        LOG_ERR("GPS init failed: %d", err);
        return err;
    }

    LOG_INF("Drivers initialized");
    return APP_OK;
}

/**
 * @brief Initialize application modules
 */
static app_err_t init_application(void)
{
    app_err_t err;

    LOG_INF("Initializing application...");

    /* BLE Manager */
    err = ble_manager_init();
    if (err != APP_OK) {
        LOG_ERR("BLE init failed: %d", err);
        return err;
    }

    /* Main loop controller */
    err = boucle_init();
    if (err != APP_OK) {
        LOG_ERR("Boucle init failed: %d", err);
        return err;
    }

    /* Display/View system */
    err = vue_init();
    if (err != APP_OK) {
        LOG_ERR("Vue init failed: %d", err);
        return err;
    }

    /* Auto-off: 15 min without a location or trainer update */
    power_scheduler_init();

    LOG_INF("Application initialized");
    return APP_OK;
}

/**
 * @brief Feed the nRF WDT if it survived a soft reset
 *
 * Only pin, power-on, brownout and watchdog resets stop the nRF52 WDT: after
 * sys_reboot() (fatal error handler, task watchdog) it keeps counting with
 * the previous timeout, and the boot has to feed it until the threads add
 * their task watchdog channels. The same code serves the nRF54L; if its WDT
 * stops on a soft reset, the check below finds it idle and does nothing.
 */
static void wdt_feed_if_running(void)
{
#if defined(HW_WDT_REGS)
    if (nrf_wdt_started_check(HW_WDT_REGS)) {
        for (uint32_t rr = 0U; rr < NRF_WDT_CHANNEL_NUMBER; rr++) {
            if (nrf_wdt_reload_request_enable_check(HW_WDT_REGS, (nrf_wdt_rr_register_t)rr)) {
                nrf_wdt_reload_request_set(HW_WDT_REGS, (nrf_wdt_rr_register_t)rr);
            }
        }
    }
#endif
}

/**
 * @brief Task watchdog expiry: a thread stopped feeding its channel
 *
 * Runs in the system timer interrupt. Flush the log so the name of the
 * stalled thread reaches the console, then reboot.
 */
static void wdt_expired(int channel_id, void *user_data)
{
    LOG_ERR("Task watchdog: %s stalled (channel %d), rebooting",
            (const char *)user_data, channel_id);
    LOG_PANIC();
    sys_reboot(SYS_REBOOT_COLD);
}

/**
 * @brief Give the calling thread its task watchdog channel
 *
 * @return Channel id for task_wdt_feed(), or a negative error code
 */
static int wdt_channel_add(const char *thread_name)
{
    int channel = task_wdt_add(WDT_TIMEOUT_MS, wdt_expired, (void *)thread_name);

    if (channel < 0) {
        LOG_ERR("Task watchdog channel for %s failed: %d", thread_name, channel);
    }
    return channel;
}

/**
 * @brief Button callback handler
 */
static void button_callback(btn_event_t event)
{
    LOG_DBG("Button event: %d", event);

    /* Pass to view system */
    vue_handle_button(event);

    /* Pass to main loop controller */
    boucle_handle_button(event);
}

/**
 * @brief Display update thread
 */
static void display_thread(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    LOG_INF("Display thread started");

    int wdt_channel = wdt_channel_add("display");

    while (true) {
        uint32_t now = k_uptime_get_32();

        /* Update display */
        if ((now - last_display_update) >= DISPLAY_UPDATE_INTERVAL_MS) {
            vue_update();
            last_display_update = now;
        }

        /* Toggle VCOM */
        if ((now - last_vcom_toggle) >= VCOM_TOGGLE_INTERVAL_MS) {
            ls027_toggle_vcom();
            last_vcom_toggle = now;
        }

        (void)task_wdt_feed(wdt_channel);

        k_msleep(50);
    }
}

/**
 * @brief Publish the battery level on the BLE Battery Service when it changes
 *
 * Called outside the model lock: bt_bas_set_battery_level() sends a GATT
 * notification. The fuel gauge cache is written by main_loop only.
 */
static void update_battery_service(void)
{
    static int16_t last_soc = -1;
    uint8_t soc = stc3100_get_soc();

    if ((int16_t)soc != last_soc) {
        ble_manager_update_battery(soc);
        last_soc = (int16_t)soc;
    }
}

/**
 * @brief Main application thread
 */
static void main_thread(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    LOG_INF("Main thread started");

    /* Wait for system initialization */
    while (!system_initialized) {
        k_msleep(10);
    }

    /* Start BLE advertising */
    (void)ble_manager_start_advertising();

    /* The display thread already runs: model writes go under the lock */
    model_lock();

    /* Start GPS */
    (void)gps_mgmt_start();

    /* Load segments from SD card */
    int seg_count = segment_load_all();
    LOG_INF("Loaded %d segments", seg_count);

    /* Set initial mode */
    (void)boucle_set_mode(APP_MODE_CRS);

    model_unlock();

    /* After the start-up above: loading segments from SD may take longer */
    int wdt_channel = wdt_channel_add("main_loop");

    /* Main processing loop */
    while (true) {
        /* One model step: buttons, GPS and sensors all write the model */
        model_lock();

        /* Process button events */
        hal_gpio_btn_process();

        /* Process GPS data */
        gps_mgmt_process();

        /* Process main loop */
        boucle_process();

        /* Power off after 15 min without activity (legacy power_scheduler) */
        power_scheduler_run();

        model_unlock();

        update_battery_service();

        /* Toggle LED to show we're alive */
        static uint32_t led_toggle_time;
        if ((k_uptime_get_32() - led_toggle_time) >= 1000U) {
            hal_gpio_led_toggle();
            led_toggle_time = k_uptime_get_32();
        }

        /* A whole cycle ran: buttons, GPS, sensors, model and battery */
        (void)task_wdt_feed(wdt_channel);

        k_msleep(MAIN_LOOP_INTERVAL_MS);
    }
}

/* ==========================================================================
 * Main Entry Point
 * ========================================================================== */

int main(void)
{
    LOG_INF("=====================================");
    LOG_INF("stravaV10 - Bike GPS Computer");
    LOG_INF("Version %d.%d.%d",
            APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    LOG_INF("=====================================");

    wdt_feed_if_running();

    /* Initialize hardware */
    app_err_t err = init_hardware();
    if (err != APP_OK) {
        LOG_ERR("Hardware initialization failed!");
        return -1;
    }

    /* Initialize drivers */
    err = init_drivers();
    if (err != APP_OK) {
        LOG_ERR("Driver initialization failed!");
        return -1;
    }

    wdt_feed_if_running();

    /* Initialize application */
    err = init_application();
    if (err != APP_OK) {
        LOG_ERR("Application initialization failed!");
        return -1;
    }

    wdt_feed_if_running();

    /* Register button callback */
    err = hal_gpio_register_btn_callback(button_callback);
    if (err != APP_OK) {
        LOG_WRN("Button callback registration failed");
    }

    /*
     * Task watchdog backed by the nRF WDT: the hardware timer starts with the
     * first channel, when a thread adds it (or keeps running from before a
     * soft reset). Without the device it still catches a stalled thread, but
     * not a system that stops the kernel timer.
     */
    const struct device *hw_wdt = DEVICE_DT_GET_OR_NULL(DT_ALIAS(watchdog0));

    if ((hw_wdt != NULL) && !device_is_ready(hw_wdt)) {
        hw_wdt = NULL;
    }
    if (task_wdt_init(hw_wdt) != 0) {
        LOG_ERR("Task watchdog init failed");
    }
    if (hw_wdt == NULL) {
        LOG_WRN("No hardware watchdog behind the task watchdog");
    }

    /* Mark system as initialized */
    system_initialized = true;
    LOG_INF("System initialization complete");

    /* Create worker threads */
    k_thread_create(&main_thread_data, main_stack, MAIN_STACK_SIZE,
                    main_thread, NULL, NULL, NULL,
                    MAIN_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&main_thread_data, "main_loop");

    k_thread_create(&display_thread_data, display_stack, DISPLAY_STACK_SIZE,
                    display_thread, NULL, NULL, NULL,
                    DISPLAY_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&display_thread_data, "display");

    LOG_INF("All threads started");

    /* Main function returns, threads continue running */
    return 0;
}
