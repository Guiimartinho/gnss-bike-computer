/**
 * @file backlight.c
 * @brief Light of the display (backlight.h, docs/16 Luz do display)
 */

#include "svc/backlight.h"

void backlight_init(backlight_t *b, bool enabled)
{
    b->state = BACKLIGHT_OFF;
    b->enabled = enabled;
    b->dark = false;
    b->key_ms = 0U;
}

void backlight_enable(backlight_t *b, bool enabled)
{
    b->enabled = enabled;
    if (!enabled) {
        b->state = BACKLIGHT_OFF;
    } else if (b->dark) {
        b->state = BACKLIGHT_AUTO;
    } else {
        /* stays off until a key or little light */
    }
}

void backlight_key(backlight_t *b, uint32_t now_ms)
{
    if (!b->enabled) {
        return;
    }
    b->key_ms = now_ms;
    if (b->state == BACKLIGHT_OFF) {
        b->state = BACKLIGHT_TEMP;
    }
}

void backlight_lux(backlight_t *b, float lux)
{
    if (lux < BACKLIGHT_DARK_LUX) {
        b->dark = true;
    } else if (lux > BACKLIGHT_BRIGHT_LUX) {
        b->dark = false;
    } else {
        /* between the two thresholds nothing changes */
    }
    if (!b->enabled) {
        return;
    }
    if (b->dark) {
        b->state = BACKLIGHT_AUTO;
    } else if (b->state == BACKLIGHT_AUTO) {
        b->state = BACKLIGHT_OFF;
    } else {
        /* off or temporary: the light level does not change them */
    }
}

void backlight_tick(backlight_t *b, uint32_t now_ms)
{
    if ((b->state == BACKLIGHT_TEMP) && ((now_ms - b->key_ms) >= BACKLIGHT_TEMP_MS)) {
        b->state = BACKLIGHT_OFF;
    }
}

bool backlight_is_on(const backlight_t *b)
{
    return b->state != BACKLIGHT_OFF;
}
