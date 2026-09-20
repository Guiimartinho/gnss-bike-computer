/**
 * @file usb_svc.c
 * @brief USB service: the serial of the commands and the disk on the PC
 *
 * Two things ride on the USB, as in the legacy: a serial port that takes
 * the command sentences (`$LOC`, `$QRY`, ...) and hands the same answers as
 * the radio, and the mass storage that gives the card to the PC
 * (`docs/09-armazenamento-usb.md`).
 *
 * The two never run at the same time. While the device is riding, the file
 * system belongs to the firmware and the PC only gets the serial; the mass
 * storage comes up when the rider asks for it in the menu or a `$DWN,16`
 * arrives, and by then the storage service has already unmounted. Leaving
 * it takes a reset, as the legacy also did.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/zbus/zbus.h>

#include "app/app_channels.h"
#include "app/app_cmd.h"
#include "app/app_svc.h"
#include "model/cmd_parser.h"

LOG_MODULE_REGISTER(usb_svc, CONFIG_LOG_DEFAULT_LEVEL);

/* the thread only reads characters and publishes: the parser is the deepest */
#define USB_STACK_SIZE      2048
#define USB_INBOX_LEN       8

/*
 * Identifiers of the device. 0x1209 is the vendor of open hardware
 * (pid.codes) and 0x0001 the number they keep for tests: a product number
 * of our own has to be asked for there before any of this is sold.
 */
#define USB_VID             0x1209
#define USB_PID             0x0001

struct usb_msg {
    const struct zbus_channel *chan;
    union {
        struct app_system_state sys;
        struct app_power_status power;
    } u;
};

K_MSGQ_DEFINE(usb_inbox, sizeof(struct usb_msg), USB_INBOX_LEN, 4);

static void usb_listener(const struct zbus_channel *chan)
{
    struct usb_msg msg = {.chan = chan};

    if (zbus_chan_msg_size(chan) > sizeof(msg.u)) {
        return;
    }
    (void)memcpy(&msg.u, zbus_chan_const_msg(chan), zbus_chan_msg_size(chan));
    app_inbox_put(&usb_inbox, &msg, "usb");
}

ZBUS_LISTENER_DEFINE(usb_lis, usb_listener);
ZBUS_CHAN_ADD_OBS(chan_system_state, usb_lis, 3);
ZBUS_CHAN_ADD_OBS(chan_power_status, usb_lis, 3);

USBD_DEVICE_DEFINE(gnss_usbd, DEVICE_DT_GET(DT_NODELABEL(usbhs)), USB_VID, USB_PID);

USBD_DESC_LANG_DEFINE(usb_lang);
USBD_DESC_MANUFACTURER_DEFINE(usb_mfr, "gnss-bike-computer");
USBD_DESC_PRODUCT_DEFINE(usb_product, "GNSS Bike Computer");
USBD_DESC_SERIAL_NUMBER_DEFINE(usb_sn);

USBD_DESC_CONFIG_DEFINE(usb_fs_desc, "Serial and disk");
USBD_DESC_CONFIG_DEFINE(usb_hs_desc, "Serial and disk");
USBD_CONFIGURATION_DEFINE(usb_fs_config, USB_SCD_SELF_POWERED, 250, &usb_fs_desc);
USBD_CONFIGURATION_DEFINE(usb_hs_config, USB_SCD_SELF_POWERED, 250, &usb_hs_desc);

/** The storage of the rider, as the PC sees it */
USBD_DEFINE_MSC_LUN(sd, "SD", "GNSS", "Rides", "1.00");

static const struct device *const cdc_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));

/* one sentence of the legacy is at most 150 bytes; this holds two */
RING_BUF_DECLARE(cdc_rx, 320);

static struct cmd_parser usb_parser;
static bool usb_ready;
static bool usb_running;
static bool msc_on;

/* ---- the serial of the commands ------------------------------------------ */

/**
 * The interrupt only moves the bytes out of the controller, as every other
 * interrupt of this firmware does: reading the sentences and publishing
 * what they ask for happens in the thread below.
 */
static void cdc_irq(const struct device *dev, void *user_data)
{
    uint8_t buf[32];

    ARG_UNUSED(user_data);

    if (!uart_irq_update(dev)) {
        return;
    }

    while (uart_irq_rx_ready(dev)) {
        int n = uart_fifo_read(dev, buf, sizeof(buf));

        if (n <= 0) {
            break;
        }
        (void)ring_buf_put(&cdc_rx, buf, (uint32_t)n);
    }
}

/** Read what the interrupt left, in the thread */
static void cdc_process(void)
{
    uint8_t buf[32];
    uint32_t n;

    while ((n = ring_buf_get(&cdc_rx, buf, sizeof(buf))) > 0U) {
        for (uint32_t i = 0U; i < n; i++) {
            if (cmd_parser_feed(&usb_parser, (char)buf[i]) != CMD_NONE) {
                app_cmd_handle(cmd_parser_data(&usb_parser));
            }
        }
    }
}

/* ---- the device ---------------------------------------------------------- */

/** Build the descriptors once; the classes come and go with the mode */
static int usb_setup(void)
{
    int err = usbd_add_descriptor(&gnss_usbd, &usb_lang);

    if (err == 0) {
        err = usbd_add_descriptor(&gnss_usbd, &usb_mfr);
    }
    if (err == 0) {
        err = usbd_add_descriptor(&gnss_usbd, &usb_product);
    }
    if (err == 0) {
        err = usbd_add_descriptor(&gnss_usbd, &usb_sn);
    }
    if (err == 0) {
        err = usbd_add_configuration(&gnss_usbd, USBD_SPEED_FS, &usb_fs_config);
    }
    if (err == 0) {
        err = usbd_add_configuration(&gnss_usbd, USBD_SPEED_HS, &usb_hs_config);
    }
    if (err != 0) {
        LOG_ERR("cannot describe the device: %d", err);
        return err;
    }

    /* a serial port and a disk are two interfaces of one device */
    usbd_device_set_code_triple(&gnss_usbd, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
    usbd_device_set_code_triple(&gnss_usbd, USBD_SPEED_HS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);

    err = usbd_init(&gnss_usbd);
    if (err != 0) {
        LOG_ERR("cannot start the USB: %d", err);
        return err;
    }

    usb_ready = true;

    return 0;
}

/** Register the classes the mode asks for and go on the bus */
static void usb_go(bool with_msc)
{
    if (!usb_ready) {
        return;
    }

    if (usb_running) {
        (void)usbd_disable(&gnss_usbd);
        (void)usbd_unregister_all_classes(&gnss_usbd, USBD_SPEED_FS, 1U);
        (void)usbd_unregister_all_classes(&gnss_usbd, USBD_SPEED_HS, 1U);
        usb_running = false;
    }

    const char *blocked[] = {with_msc ? NULL : "msc_0", NULL};

    (void)usbd_register_all_classes(&gnss_usbd, USBD_SPEED_FS, 1U, blocked);
    (void)usbd_register_all_classes(&gnss_usbd, USBD_SPEED_HS, 1U, blocked);

    int err = usbd_enable(&gnss_usbd);

    if (err != 0) {
        LOG_ERR("cannot go on the bus: %d", err);
        return;
    }

    usb_running = true;
    msc_on = with_msc;
    LOG_INF("USB on, %s", with_msc ? "serial and disk" : "serial only");
}

static void usb_stop(void)
{
    if (!usb_running) {
        return;
    }
    (void)usbd_disable(&gnss_usbd);
    usb_running = false;
    LOG_INF("USB off");
}

/* ---- thread -------------------------------------------------------------- */

static void usb_thread(void *p1, void *p2, void *p3)
{
    struct usb_msg msg;
    bool done = false;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    cmd_parser_init(&usb_parser);

    if (device_is_ready(cdc_dev)) {
        (void)uart_irq_callback_user_data_set(cdc_dev, cdc_irq, NULL);
        uart_irq_rx_enable(cdc_dev);
    } else {
        LOG_ERR("no USB serial");
    }

    (void)usb_setup();

    int wdt = app_wdt_add("usb");

    for (;;) {
        if (app_inbox_get(&usb_inbox, &msg, wdt, APP_SVC_TICK_MS) != 0) {
            cdc_process();
            continue;
        }
        cdc_process();

        if (msg.chan == &chan_power_status) {
            /* the bus only exists while the cable is in */
            if (msg.u.power.vbus && !usb_running) {
                usb_go(msc_on);
            } else if (!msg.u.power.vbus && usb_running) {
                usb_stop();
                msc_on = false;
            } else {
                /* nothing changed */
            }
        } else if (msg.chan == &chan_system_state) {
            if ((msg.u.sys.state == APP_SYS_MSC) && !msc_on) {
                /* the storage service has unmounted by now */
                usb_go(true);
            } else if ((msg.u.sys.state == APP_SYS_SHUTDOWN) && !done) {
                done = true;
                usb_stop();

                struct app_shutdown_ack ack = {.svc = APP_SVC_USB};

                (void)app_publish(&chan_shutdown_ack, &ack);
            } else {
                /* nothing to do in the other states */
            }
        } else {
            /* nothing else reaches this inbox */
        }
    }
}

K_THREAD_DEFINE(usb_tid, USB_STACK_SIZE, usb_thread, NULL, NULL, NULL, APP_PRIO_STORAGE, 0,
                SYS_FOREVER_MS);

void usb_svc_start(void)
{
    k_thread_name_set(usb_tid, "usb");
    k_thread_start(usb_tid);
}
