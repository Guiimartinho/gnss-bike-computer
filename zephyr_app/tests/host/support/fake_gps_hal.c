/**
 * @file fake_gps_hal.c
 * @brief Fake UART/GPIO/EPO layer for the gps_mgmt host tests.
 */

#include <string.h>

#include "drivers/gps_epo.h"
#include "hal/hal_uart.h"

#include "fake_gps_hal.h"

#define MAX_LINES 32U
#define LINE_LEN  128U

static char s_lines[MAX_LINES][LINE_LEN];
static unsigned int s_head;
static unsigned int s_count;
static hal_uart_line_callback_t s_line_cb;
static bool s_gpio_out[HAL_GPIO_PIN_COUNT];
static bool s_gpio_in[HAL_GPIO_PIN_COUNT];

void fake_gps_hal_reset(void)
{
    /* The line callback stays: gps_mgmt registers it once, in gps_mgmt_init() */
    s_head = 0U;
    s_count = 0U;
    memset(s_gpio_out, 0, sizeof(s_gpio_out));
    memset(s_gpio_in, 0, sizeof(s_gpio_in));
}

void fake_uart_push_line(const char *line)
{
    unsigned int slot = (s_head + s_count) % MAX_LINES;

    (void)strncpy(s_lines[slot], line, LINE_LEN - 1U);
    s_lines[slot][LINE_LEN - 1U] = '\0';
    if (s_count < MAX_LINES) {
        s_count++;
    }
}

bool fake_gpio_level(hal_gpio_pin_t pin)
{
    return s_gpio_out[pin];
}

void fake_gpio_set_input(hal_gpio_pin_t pin, bool level)
{
    s_gpio_in[pin] = level;
}

/* ---- hal_uart ---- */

app_err_t hal_uart_register_line_callback(hal_uart_port_t port, hal_uart_line_callback_t callback)
{
    (void)port;
    s_line_cb = callback;
    return APP_OK;
}

void hal_uart_process(hal_uart_port_t port)
{
    (void)port;
    while (s_count > 0U) {
        const char *line = s_lines[s_head];

        s_head = (s_head + 1U) % MAX_LINES;
        s_count--;
        if (s_line_cb != NULL) {
            s_line_cb(line);
        }
    }
}

app_err_t hal_uart_set_power(hal_uart_port_t port, bool enable)
{
    (void)port;
    (void)enable;
    return APP_OK;
}

app_err_t hal_uart_transmit_str(hal_uart_port_t port, const char *str)
{
    (void)port;
    (void)str;
    return APP_OK;
}

/* ---- hal_gpio ---- */

app_err_t hal_gpio_set(hal_gpio_pin_t pin, bool state)
{
    s_gpio_out[pin] = state;
    return APP_OK;
}

app_err_t hal_gpio_get(hal_gpio_pin_t pin, bool *state)
{
    *state = s_gpio_in[pin];
    return APP_OK;
}

/* ---- gps_epo (not under test) ---- */

app_err_t gps_epo_start_transfer(uint32_t current_gps_hour)
{
    (void)current_gps_hour;
    return APP_OK;
}

uint32_t gps_epo_calc_gps_hour(const date_data_t *date)
{
    (void)date;
    return 0U;
}
