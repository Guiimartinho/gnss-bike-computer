/**
 * @file memlcd.c
 * @brief Memory-in-pixel display driver: JDI LPM027M128B/C and Sharp LS027B7DH01
 *
 * What the Zephyr ls0xx driver does not do and the bike computer needs:
 *  - portrait: the application draws 240 x 400 and the driver turns it
 *    (rotation 90, the legacy setRotation(3));
 *  - RGB565 in, quantised to the panel (memlcd_pixel.h, the same rule as the
 *    PNG pictures of docs/telas): 8 colours on the JDI, black and white on
 *    the Sharp, so that the interface is the same code on both;
 *  - the frame buffer kept as it goes on the wire (memlcd_frame.h); LVGL's
 *    partial areas are copied into it and only the changed lines go to the
 *    panel, once per frame, when LVGL says the frame is complete;
 *  - the COM inverted by the EXTCOMIN pin (a k_timer toggles it: rising
 *    edges invert the COM) or, without the pin, by the serial VCOM bit, sent
 *    by delayable work (the myStravaB V3 ties EXTMODE low);
 *  - the power sequence of the datasheets: all clear, DISP, COM.
 *
 * Datasheets: Sharp LS027B7DH01 (LCP-2110015A, 6-2 to 6-5) and JDI
 * LPM027M128B (4.3, 6.1 to 6.8, 7, 8), whose protocol the LPM027M128C
 * shares. Not tested on a panel yet.
 */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/display/memlcd.h>
#include <drivers/display/memlcd_frame.h>

LOG_MODULE_REGISTER(memlcd, CONFIG_MEMLCD_LOG_LEVEL);

/** SPI buffers of a frame: mode byte, up to this many runs of lines, tail */
#define MEMLCD_MAX_RUNS         16U
/** COM inversions per second: EXTCOMIN up to 20 Hz on the Sharp, 140 Hz on the JDI */
#define MEMLCD_COM_HZ_MIN       1U
#define MEMLCD_COM_HZ_MAX_SHARP 20U
#define MEMLCD_COM_HZ_MAX_JDI   140U
/** Supply of the panel up (power-gpios): margin for the regulator, to measure on the bench */
#define MEMLCD_POWER_UP_MS      10
/** Pixel memory initialisation after an all clear: 1 ms or more (T2, T5) */
#define MEMLCD_CLEAR_MS         2
/** After DISP rises: latch release and COM polarity, 100 to 200 us when EXTCOMIN runs (T3, T4) */
#define MEMLCD_DISP_ON_US       200U
/** After DISP falls: COM and latch initialisation, 30 us or more (T6) */
#define MEMLCD_DISP_OFF_US      50U
/** Serial VCOM while a frame goes out: try again after this */
#define MEMLCD_BUSY_RETRY_MS    5

struct memlcd_config {
    struct spi_dt_spec bus;
    struct gpio_dt_spec disp;       /**< port NULL: DISP tied high on the board */
    struct gpio_dt_spec extcomin;   /**< port NULL: serial VCOM (EXTMODE low) */
    struct gpio_dt_spec power;      /**< port NULL: supply always on */
    uint32_t com_hz;                /**< COM inversions per second at start */
};

struct memlcd_data {
    struct memlcd_frame frame;
    const struct device *dev;
    struct k_mutex lock;            /**< SPI bus, frame buffer and COM level */
    struct k_timer extcomin_timer;
    struct k_work_delayable com_work;
    struct memlcd_run runs[MEMLCD_MAX_RUNS];
    struct spi_buf bufs[MEMLCD_MAX_RUNS + 2U];
    uint8_t cmd[2];                 /**< mode commands, in RAM for the SPI DMA */
    uint32_t com_hz;
    bool com;                       /**< serial VCOM level (M1) */
    bool com_on;
    bool on;                        /**< supply up, not powered off */
};

static bool serial_com(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;

    return cfg->extcomin.port == NULL;
}

/** M1 of the mode byte: the COM level in serial mode, ignored with EXTCOMIN */
static uint8_t com_bit(const struct device *dev)
{
    const struct memlcd_data *data = dev->data;

    if (!serial_com(dev) || !data->com) {
        return 0U;
    }
    return (data->frame.kind == MEMLCD_SHARP) ? MEMLCD_SHARP_M1_VCOM : MEMLCD_JDI_M1_COM;
}

/** Mode select and 16 clocks: all clear, or no update carrying M1. Lock held. */
static int send_mode(const struct device *dev, bool all_clear)
{
    const struct memlcd_config *cfg = dev->config;
    struct memlcd_data *data = dev->data;
    uint8_t clear = (data->frame.kind == MEMLCD_SHARP) ? MEMLCD_SHARP_M2_CLEAR
                                                       : MEMLCD_JDI_M2_CLEAR;
    struct spi_buf buf = {.buf = data->cmd, .len = sizeof(data->cmd)};
    struct spi_buf_set set = {.buffers = &buf, .count = 1U};

    data->cmd[0] = (uint8_t)(com_bit(dev) | (all_clear ? clear : 0U));
    data->cmd[1] = 0U;
    return spi_write_dt(&cfg->bus, &set);
}

/** The changed lines to the panel, in one transfer. Lock held. */
static int send_frame(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;
    struct memlcd_data *data = dev->data;
    struct memlcd_frame *f = &data->frame;
    size_t runs = memlcd_frame_runs(f, data->runs, MEMLCD_MAX_RUNS);
    size_t stride = memlcd_frame_stride(f);
    size_t n = 0U;
    size_t tail_len;
    uint8_t *tail;
    int err;

    if (runs == 0U) {
        return 0;
    }
    if (f->kind == MEMLCD_SHARP) {
        uint8_t *lead = memlcd_frame_lead(f);

        *lead = (uint8_t)(MEMLCD_SHARP_M0_UPDATE | com_bit(dev));
        data->bufs[n].buf = lead;
        data->bufs[n].len = 1U;
        n++;
    } else {
        /* M1 counts on the first line of the transfer only */
        uint8_t *first = memlcd_frame_line(f, data->runs[0].first);

        first[0] = (uint8_t)((first[0] & (uint8_t)~MEMLCD_JDI_M1_COM) | com_bit(dev));
    }
    for (size_t i = 0U; i < runs; i++) {
        data->bufs[n].buf = memlcd_frame_line(f, data->runs[i].first);
        data->bufs[n].len = (size_t)data->runs[i].count * stride;
        n++;
    }
    tail = memlcd_frame_tail(f, &tail_len);
    data->bufs[n].buf = tail;
    data->bufs[n].len = tail_len;
    n++;

    const struct spi_buf_set set = {.buffers = data->bufs, .count = n};

    err = spi_write_dt(&cfg->bus, &set);
    if (err == 0) {
        memlcd_frame_clean(f);
    } else {
        LOG_ERR("frame not sent: %d", err);
    }
    return err;
}

/** EXTCOMIN: timer expiry, in ISR context; only the pin moves here */
static void extcomin_toggle(struct k_timer *timer)
{
    const struct device *dev = k_timer_user_data_get(timer);
    const struct memlcd_config *cfg = dev->config;

    (void)gpio_pin_toggle_dt(&cfg->extcomin);
}

/** Serial VCOM: the next COM level, once per inversion */
static void com_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct memlcd_data *data = CONTAINER_OF(dwork, struct memlcd_data, com_work);

    if (!data->com_on) {
        return;
    }
    if (k_mutex_lock(&data->lock, K_NO_WAIT) != 0) {
        /* A frame is going out; the system work queue does not wait for it */
        (void)k_work_schedule(dwork, K_MSEC(MEMLCD_BUSY_RETRY_MS));
        return;
    }
    data->com = !data->com;
    if (send_mode(data->dev, false) != 0) {
        LOG_WRN("VCOM not sent");
    }
    k_mutex_unlock(&data->lock);
    (void)k_work_schedule(dwork, K_USEC(USEC_PER_SEC / data->com_hz));
}

static void com_start(const struct device *dev)
{
    struct memlcd_data *data = dev->data;

    data->com_on = true;
    if (serial_com(dev)) {
        (void)k_work_reschedule(&data->com_work, K_USEC(USEC_PER_SEC / data->com_hz));
    } else {
        /* Two toggles per rising edge */
        k_timeout_t half = K_USEC(USEC_PER_SEC / (2U * data->com_hz));

        k_timer_start(&data->extcomin_timer, half, half);
    }
}

static void com_stop(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;
    struct memlcd_data *data = dev->data;

    data->com_on = false;
    if (serial_com(dev)) {
        struct k_work_sync sync;

        (void)k_work_cancel_delayable_sync(&data->com_work, &sync);
    } else {
        k_timer_stop(&data->extcomin_timer);
        (void)gpio_pin_set_dt(&cfg->extcomin, 0);
    }
}

int memlcd_set_com_hz(const struct device *dev, uint32_t hz)
{
    struct memlcd_data *data = dev->data;
    uint32_t max = (data->frame.kind == MEMLCD_SHARP) ? MEMLCD_COM_HZ_MAX_SHARP
                                                      : MEMLCD_COM_HZ_MAX_JDI;

    if ((hz < MEMLCD_COM_HZ_MIN) || (hz > max)) {
        return -EINVAL;
    }
    data->com_hz = hz;
    if (data->com_on) {
        com_start(dev);
    }
    return 0;
}

int memlcd_clear(const struct device *dev)
{
    struct memlcd_data *data = dev->data;
    int err;

    (void)k_mutex_lock(&data->lock, K_FOREVER);
    err = send_mode(dev, true);
    if (err == 0) {
        memlcd_frame_white(&data->frame);
    }
    k_mutex_unlock(&data->lock);
    return err;
}

int memlcd_power_off(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;
    struct memlcd_data *data = dev->data;
    int err;

    if (!data->on) {
        return 0;
    }
    err = memlcd_clear(dev);
    k_msleep(MEMLCD_CLEAR_MS);
    if (cfg->disp.port != NULL) {
        (void)gpio_pin_set_dt(&cfg->disp, 0);
    }
    k_busy_wait(MEMLCD_DISP_OFF_US);
    com_stop(dev);
    (void)k_mutex_lock(&data->lock, K_FOREVER);
    data->on = false;
    k_mutex_unlock(&data->lock);
    if (cfg->power.port != NULL) {
        (void)gpio_pin_set_dt(&cfg->power, 0);
    }
    return err;
}

static int memlcd_blanking_on(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;

    if (cfg->disp.port == NULL) {
        return -ENOTSUP;
    }
    return gpio_pin_set_dt(&cfg->disp, 0);
}

static int memlcd_blanking_off(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;
    int err;

    if (cfg->disp.port == NULL) {
        return 0;
    }
    err = gpio_pin_set_dt(&cfg->disp, 1);
    k_busy_wait(MEMLCD_DISP_ON_US);
    return err;
}

static int memlcd_write(const struct device *dev, const uint16_t x, const uint16_t y,
                        const struct display_buffer_descriptor *desc, const void *buf)
{
    struct memlcd_data *data = dev->data;
    int err = 0;

    if (buf == NULL) {
        return -EINVAL;
    }
    (void)k_mutex_lock(&data->lock, K_FOREVER);
    if (memlcd_frame_blit(&data->frame, x, y, desc->width, desc->height, desc->pitch, buf) != 0) {
        LOG_ERR("area %u,%u %ux%u out of the picture", x, y, desc->width, desc->height);
        err = -EINVAL;
    } else if (!desc->frame_incomplete && data->on) {
        err = send_frame(dev);
    }
    k_mutex_unlock(&data->lock);
    return err;
}

static int memlcd_display_clear(const struct device *dev)
{
    return memlcd_clear(dev);
}

static void memlcd_get_capabilities(const struct device *dev, struct display_capabilities *caps)
{
    const struct memlcd_data *data = dev->data;

    (void)memset(caps, 0, sizeof(*caps));
    memlcd_frame_size(&data->frame, &caps->x_resolution, &caps->y_resolution);
    caps->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
    caps->current_pixel_format = PIXEL_FORMAT_RGB_565;
    caps->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int memlcd_set_pixel_format(const struct device *dev, const enum display_pixel_format pf)
{
    ARG_UNUSED(dev);
    return (pf == PIXEL_FORMAT_RGB_565) ? 0 : -ENOTSUP;
}

static int pin_init(const struct gpio_dt_spec *pin, gpio_flags_t flags, const char *name)
{
    if (pin->port == NULL) {
        return 0;
    }
    if (!gpio_is_ready_dt(pin)) {
        LOG_ERR("%s pin not ready", name);
        return -ENODEV;
    }
    return gpio_pin_configure_dt(pin, flags);
}

static int memlcd_init(const struct device *dev)
{
    const struct memlcd_config *cfg = dev->config;
    struct memlcd_data *data = dev->data;
    int err;

    data->dev = dev;
    k_mutex_init(&data->lock);
    k_timer_init(&data->extcomin_timer, extcomin_toggle, NULL);
    k_timer_user_data_set(&data->extcomin_timer, (void *)dev);
    k_work_init_delayable(&data->com_work, com_work_handler);
    memlcd_frame_init(&data->frame);
    data->com_hz = CLAMP(cfg->com_hz, MEMLCD_COM_HZ_MIN,
                         (data->frame.kind == MEMLCD_SHARP) ? MEMLCD_COM_HZ_MAX_SHARP
                                                            : MEMLCD_COM_HZ_MAX_JDI);

    if (!spi_is_ready_dt(&cfg->bus)) {
        LOG_ERR("SPI bus not ready");
        return -ENODEV;
    }
    /* Picture off until the application has drawn it (display_blanking_off()) */
    err = pin_init(&cfg->disp, GPIO_OUTPUT_INACTIVE, "DISP");
    if (err == 0) {
        err = pin_init(&cfg->extcomin, GPIO_OUTPUT_INACTIVE, "EXTCOMIN");
    }
    if (err == 0) {
        err = pin_init(&cfg->power, GPIO_OUTPUT_ACTIVE, "power");
    }
    if (err != 0) {
        return err;
    }
    if (cfg->power.port != NULL) {
        k_msleep(MEMLCD_POWER_UP_MS);
    }
    data->on = true;
    /* Pixel memory initialisation, then the COM (ignored while DISP is low) */
    err = memlcd_clear(dev);
    if (err != 0) {
        LOG_ERR("all clear failed: %d", err);
        return err;
    }
    k_msleep(MEMLCD_CLEAR_MS);
    com_start(dev);
    LOG_INF("%s %ux%u, rotation %u, COM %s at %u Hz", dev->name, data->frame.width,
            data->frame.height, data->frame.rotation, serial_com(dev) ? "serial" : "EXTCOMIN",
            data->com_hz);
    return 0;
}

static DEVICE_API(display, memlcd_api) = {
    .blanking_on = memlcd_blanking_on,
    .blanking_off = memlcd_blanking_off,
    .write = memlcd_write,
    .clear = memlcd_display_clear,
    .get_capabilities = memlcd_get_capabilities,
    .set_pixel_format = memlcd_set_pixel_format,
};

/* SPI mode 0, chip select active high; the Sharp takes the least significant bit first */
#define MEMLCD_SPI_OP(kind_)                                                                  \
    (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_CS_ACTIVE_HIGH |                             \
     (((kind_) == MEMLCD_SHARP) ? SPI_TRANSFER_LSB : SPI_TRANSFER_MSB))

#define MEMLCD_SYM(prefix, node) _CONCAT(prefix, DT_DEP_ORD(node))

#define MEMLCD_DEFINE(node, kind_)                                                            \
    BUILD_ASSERT((DT_PROP(node, width) % 8) == 0, "memlcd: width must be a multiple of 8");  \
    static uint8_t MEMLCD_SYM(memlcd_buf_, node)[MEMLCD_FRAME_SIZE(                          \
        (kind_), DT_PROP(node, width), DT_PROP(node, height))] __aligned(4);                  \
    static uint32_t MEMLCD_SYM(memlcd_dirty_, node)[MEMLCD_DIRTY_WORDS(DT_PROP(node, height))]; \
    static const struct memlcd_config MEMLCD_SYM(memlcd_cfg_, node) = {                      \
        .bus = SPI_DT_SPEC_GET(node, MEMLCD_SPI_OP(kind_)),                                   \
        .disp = GPIO_DT_SPEC_GET_OR(node, disp_gpios, {0}),                                  \
        .extcomin = GPIO_DT_SPEC_GET_OR(node, extcomin_gpios, {0}),                          \
        .power = GPIO_DT_SPEC_GET_OR(node, power_gpios, {0}),                                \
        .com_hz = DT_PROP(node, extcomin_frequency),                                         \
    };                                                                                       \
    static struct memlcd_data MEMLCD_SYM(memlcd_data_, node) = {                             \
        .frame = {                                                                           \
            .buf = MEMLCD_SYM(memlcd_buf_, node),                                            \
            .dirty = MEMLCD_SYM(memlcd_dirty_, node),                                        \
            .width = DT_PROP(node, width),                                                   \
            .height = DT_PROP(node, height),                                                 \
            .rotation = DT_PROP(node, rotation),                                             \
            .kind = (kind_),                                                                  \
        },                                                                                   \
    };                                                                                       \
    DEVICE_DT_DEFINE(node, memlcd_init, NULL, &MEMLCD_SYM(memlcd_data_, node),               \
                     &MEMLCD_SYM(memlcd_cfg_, node), POST_KERNEL,                            \
                     CONFIG_DISPLAY_INIT_PRIORITY, &memlcd_api);

DT_FOREACH_STATUS_OKAY_VARGS(jdi_lpm027m128b, MEMLCD_DEFINE, MEMLCD_JDI)
DT_FOREACH_STATUS_OKAY_VARGS(jdi_lpm027m128c, MEMLCD_DEFINE, MEMLCD_JDI)
DT_FOREACH_STATUS_OKAY_VARGS(sharp_ls027b7dh01, MEMLCD_DEFINE, MEMLCD_SHARP)
