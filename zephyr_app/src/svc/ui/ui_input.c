/**
 * @file ui_input.c
 * @brief The three keys of the device, from the input subsystem to zbus
 *
 * gpio-keys debounces the buttons and zephyr,input-longpress (node label
 * longpress in the overlays) turns each into a short or a long press: short
 * on release before 1 s, long after 1 s held (docs/18, Botões). This
 * callback runs in the input thread and only publishes chan_input; the ui
 * thread decides what the key does.
 *
 * Codes of the longpress node: short LEFT, ENTER, RIGHT; long HOME, MENU, END.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include "app/app_channels.h"

#define UI_KEYS_NODE DT_NODELABEL(longpress)

#if DT_NODE_HAS_STATUS_OKAY(UI_KEYS_NODE)

/* The signature is the input subsystem's callback type (non-const event) */
// cppcheck-suppress constParameterCallback
static void ui_keys_cb(struct input_event *evt, void *user_data)
{
    struct app_input in = {0};

    ARG_UNUSED(user_data);
    /* short: press and release together; long: press after 1 s, release later */
    if ((evt->type != INPUT_EV_KEY) || (evt->value == 0)) {
        return;
    }
    switch (evt->code) {
    case INPUT_KEY_LEFT:
        in.key = APP_KEY_LEFT;
        break;
    case INPUT_KEY_ENTER:
        in.key = APP_KEY_CENTER;
        break;
    case INPUT_KEY_RIGHT:
        in.key = APP_KEY_RIGHT;
        break;
    case INPUT_KEY_HOME:
        in.key = APP_KEY_LEFT;
        in.long_press = true;
        break;
    case INPUT_KEY_MENU:
        in.key = APP_KEY_CENTER;
        in.long_press = true;
        break;
    case INPUT_KEY_END:
        in.key = APP_KEY_RIGHT;
        in.long_press = true;
        break;
    default:
        return;
    }
    (void)app_publish(&chan_input, &in);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(UI_KEYS_NODE), ui_keys_cb, NULL);

#endif /* DT_NODE_HAS_STATUS_OKAY(UI_KEYS_NODE) */
