/**
 * @file backlight.h
 * @brief Light of the display: the state machine of docs/16 (Luz do display)
 *
 * Plain C, no Zephyr (host test test_backlight). The V3 has no light; the new
 * board lights the panel (the Azumo front light of the Sharp, or the light of
 * the JDI) by PWM:
 *
 *  - Off: the start, and the only state when the menu turns the light off;
 *  - Temporary: a key lights it for BACKLIGHT_TEMP_MS, a new key restarts it;
 *  - Automatic: little ambient light (OPT3001) keeps it on until the light
 *    comes back.
 *
 * The light level has hysteresis (dark below BACKLIGHT_DARK_LUX, back above
 * BACKLIGHT_BRIGHT_LUX) so that it does not blink. Thresholds and time are
 * starting points, to be set on the bench with the real window and panel.
 */

#ifndef SVC_BACKLIGHT_H
#define SVC_BACKLIGHT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Light after a key, as docs/16 (bench) */
#define BACKLIGHT_TEMP_MS       10000U
/** Below this, little ambient light (bench) */
#define BACKLIGHT_DARK_LUX      20.0f
/** Above this, the ambient light is back (bench) */
#define BACKLIGHT_BRIGHT_LUX    50.0f

typedef enum {
    BACKLIGHT_OFF = 0,
    BACKLIGHT_TEMP,
    BACKLIGHT_AUTO
} backlight_state_t;

typedef struct {
    backlight_state_t state;
    bool enabled;           /**< menu Tela e luz: automatic (true) or off */
    bool dark;              /**< little ambient light, after the hysteresis */
    uint32_t key_ms;        /**< time of the last key */
} backlight_t;

/** Off, light level unknown (taken as enough light) */
void backlight_init(backlight_t *b, bool enabled);

/** Menu Tela e luz: off turns the light off at once; on follows the ambient light */
void backlight_enable(backlight_t *b, bool enabled);

/** A key was pressed */
void backlight_key(backlight_t *b, uint32_t now_ms);

/** New ambient light level, in lux */
void backlight_lux(backlight_t *b, float lux);

/** Time goes by: ends the temporary light */
void backlight_tick(backlight_t *b, uint32_t now_ms);

/** Whether the light is on */
bool backlight_is_on(const backlight_t *b);

#ifdef __cplusplus
}
#endif

#endif /* SVC_BACKLIGHT_H */
