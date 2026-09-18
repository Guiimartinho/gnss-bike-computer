/**
 * @file hal_uart.c
 * @brief UART Hardware Abstraction Layer implementation
 *
 * The RX interrupt only copies bytes into a ring buffer. Lines are assembled
 * and handed to the line callback by hal_uart_process(), in the calling
 * thread: the NMEA parser and the whole model used to run inside the ISR.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#include "hal/hal_uart.h"

LOG_MODULE_REGISTER(hal_uart, CONFIG_LOG_DEFAULT_LEVEL);

/* ==========================================================================
 * Private Definitions
 * ========================================================================== */

/** Maximum NMEA line length */
#define MAX_LINE_LEN    128U

/** Bytes moved per ring buffer access */
#define RX_CHUNK_LEN    32U

/* ==========================================================================
 * Device Tree Bindings
 * ========================================================================== */

/* UART1 - GPS */
static const struct device *uart_gps_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

/* ==========================================================================
 * Private Variables
 * ========================================================================== */

/** UART port context structure */
typedef struct {
    const struct device *dev;
    struct ring_buf rx_rb;                      /**< ISR producer, thread consumer */
    uint8_t rx_rb_data[HAL_UART_RX_BUF_SIZE];
    uint32_t rx_dropped;                        /**< Bytes lost on a full ring */
    char line_buf[MAX_LINE_LEN];
    uint8_t line_idx;
    hal_uart_rx_callback_t rx_callback;
    hal_uart_line_callback_t line_callback;
    bool enabled;
} uart_ctx_t;

/** UART contexts per port */
static uart_ctx_t uart_ctx[HAL_UART_PORT_COUNT];

/** Initialization flag */
static bool is_initialized;

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief UART ISR callback: move received bytes into the ring buffer only
 */
static void uart_isr_callback(const struct device *dev, void *user_data)
{
    uart_ctx_t *ctx = (uart_ctx_t *)user_data;

    if ((ctx == NULL) || (ctx->dev != dev)) {
        return;
    }

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (!uart_irq_rx_ready(dev)) {
            break;
        }

        uint8_t chunk[RX_CHUNK_LEN];
        int ret = uart_fifo_read(dev, chunk, sizeof(chunk));

        if (ret > 0) {
            uint32_t stored = ring_buf_put(&ctx->rx_rb, chunk, (uint32_t)ret);

            ctx->rx_dropped += (uint32_t)ret - stored;
        }
    }
}

/**
 * @brief Assemble NMEA lines ('$' ... '\n') and hand them to the line callback
 */
static void assemble_line(uart_ctx_t *ctx, uint8_t byte)
{
    if (byte == (uint8_t)'\n') {
        if (ctx->line_idx > 0U) {
            ctx->line_buf[ctx->line_idx] = '\0';

            /* Remove trailing CR if present */
            if (ctx->line_buf[ctx->line_idx - 1U] == '\r') {
                ctx->line_buf[ctx->line_idx - 1U] = '\0';
            }

            if (ctx->line_callback != NULL) {
                ctx->line_callback(ctx->line_buf);
            }
        }
        ctx->line_idx = 0U;
    } else if (byte == (uint8_t)'$') {
        /* Start of NMEA sentence */
        ctx->line_buf[0] = (char)byte;
        ctx->line_idx = 1U;
    } else if (ctx->line_idx > 0U) {
        if (ctx->line_idx < (MAX_LINE_LEN - 1U)) {
            ctx->line_buf[ctx->line_idx] = (char)byte;
            ctx->line_idx++;
        }
    } else {
        /* Ignore bytes before start marker */
    }
}

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

app_err_t hal_uart_init(void)
{
    if (is_initialized) {
        return APP_ERR_ALREADY_INIT;
    }

    /* Initialize GPS UART */
    if (!device_is_ready(uart_gps_dev)) {
        LOG_ERR("UART GPS device not ready");
        return APP_ERR_NOT_INIT;
    }

    /* Initialize context */
    uart_ctx_t *ctx = &uart_ctx[HAL_UART_GPS];

    (void)memset(ctx, 0, sizeof(uart_ctx_t));
    ctx->dev = uart_gps_dev;
    ring_buf_init(&ctx->rx_rb, sizeof(ctx->rx_rb_data), ctx->rx_rb_data);
    ctx->enabled = true;

    /* Configure UART interrupt */
    uart_irq_callback_user_data_set(uart_gps_dev, uart_isr_callback, ctx);
    uart_irq_rx_enable(uart_gps_dev);

    is_initialized = true;
    LOG_INF("UART HAL initialized");

    return APP_OK;
}

app_err_t hal_uart_transmit(hal_uart_port_t port, const uint8_t *data, size_t len)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if ((port >= HAL_UART_PORT_COUNT) || (data == NULL) || (len == 0U)) {
        return APP_ERR_INVALID_PARAM;
    }

    uart_ctx_t *ctx = &uart_ctx[port];

    if (!ctx->enabled) {
        return APP_ERR_NOT_INIT;
    }

    for (size_t i = 0U; i < len; i++) {
        uart_poll_out(ctx->dev, data[i]);
    }

    return APP_OK;
}

app_err_t hal_uart_transmit_str(hal_uart_port_t port, const char *str)
{
    if (str == NULL) {
        return APP_ERR_INVALID_PARAM;
    }

    return hal_uart_transmit(port, (const uint8_t *)str, strlen(str));
}

app_err_t hal_uart_register_rx_callback(hal_uart_port_t port,
                                        hal_uart_rx_callback_t callback)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (port >= HAL_UART_PORT_COUNT) {
        return APP_ERR_INVALID_PARAM;
    }

    uart_ctx[port].rx_callback = callback;
    return APP_OK;
}

app_err_t hal_uart_register_line_callback(hal_uart_port_t port,
                                          hal_uart_line_callback_t callback)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (port >= HAL_UART_PORT_COUNT) {
        return APP_ERR_INVALID_PARAM;
    }

    uart_ctx[port].line_callback = callback;
    return APP_OK;
}

void hal_uart_process(hal_uart_port_t port)
{
    if (!is_initialized || (port >= HAL_UART_PORT_COUNT)) {
        return;
    }

    uart_ctx_t *ctx = &uart_ctx[port];
    uint8_t chunk[RX_CHUNK_LEN];
    uint32_t len;

    if (ctx->rx_dropped > 0U) {
        LOG_WRN("UART %d: %u bytes dropped (RX ring full)", port, (unsigned)ctx->rx_dropped);
        ctx->rx_dropped = 0U;
    }

    while ((len = ring_buf_get(&ctx->rx_rb, chunk, sizeof(chunk))) > 0U) {
        if (ctx->rx_callback != NULL) {
            ctx->rx_callback(chunk, len);
        }
        for (uint32_t i = 0U; i < len; i++) {
            assemble_line(ctx, chunk[i]);
        }
    }
}

size_t hal_uart_rx_available(hal_uart_port_t port)
{
    if (!is_initialized || (port >= HAL_UART_PORT_COUNT)) {
        return 0U;
    }

    return (size_t)ring_buf_size_get(&uart_ctx[port].rx_rb);
}

app_err_t hal_uart_read_byte(hal_uart_port_t port, uint8_t *byte)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if ((port >= HAL_UART_PORT_COUNT) || (byte == NULL)) {
        return APP_ERR_INVALID_PARAM;
    }

    if (ring_buf_get(&uart_ctx[port].rx_rb, byte, 1U) != 1U) {
        return APP_ERR_NOT_FOUND;
    }

    return APP_OK;
}

void hal_uart_flush_rx(hal_uart_port_t port)
{
    if (!is_initialized || (port >= HAL_UART_PORT_COUNT)) {
        return;
    }

    uart_ctx_t *ctx = &uart_ctx[port];

    /* The ISR is the producer: keep it out while the ring is reset */
    unsigned int key = irq_lock();

    ring_buf_reset(&ctx->rx_rb);
    irq_unlock(key);
    ctx->line_idx = 0U;
}

app_err_t hal_uart_set_power(hal_uart_port_t port, bool enable)
{
    if (!is_initialized) {
        return APP_ERR_NOT_INIT;
    }

    if (port >= HAL_UART_PORT_COUNT) {
        return APP_ERR_INVALID_PARAM;
    }

    uart_ctx_t *ctx = &uart_ctx[port];

    if (enable) {
        uart_irq_rx_enable(ctx->dev);
        ctx->enabled = true;
    } else {
        uart_irq_rx_disable(ctx->dev);
        ctx->enabled = false;
    }

    return APP_OK;
}
