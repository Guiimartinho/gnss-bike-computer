/**
 * @file tilt.c
 * @brief Pitch, roll, roughness and tilt-compensated heading (see tilt.h)
 */

#include <math.h>
#include <string.h>

#include "svc/tilt.h"

#define RAD_TO_DEG  57.29578f
#define DEG_TO_RAD  0.017453293f

void tilt_window_reset(struct tilt_window *w)
{
    (void)memset(w, 0, sizeof(*w));
}

bool tilt_window_add(struct tilt_window *w, float ax, float ay, float az)
{
    w->ax[w->next] = ax;
    w->ay[w->next] = ay;
    w->az[w->next] = az;
    w->next = (uint8_t)((w->next + 1U) % TILT_WINDOW);
    if (w->n < TILT_WINDOW) {
        w->n++;
    }
    return w->n == TILT_WINDOW;
}

bool tilt_compute(const struct tilt_window *w, struct tilt_out *out)
{
    float mx = 0.0f;
    float my = 0.0f;
    float mz = 0.0f;

    if (w->n == 0U) {
        return false;
    }
    for (uint8_t i = 0U; i < w->n; i++) {
        mx += w->ax[i];
        my += w->ay[i];
        mz += w->az[i];
    }
    mx /= (float)w->n;
    my /= (float)w->n;
    mz /= (float)w->n;

    /* mean absolute deviation of each axis, in legacy counts (fxos.cpp:761-766) */
    const float to_counts = TILT_COUNTS_PER_G / TILT_G;

    out->rough[0] = 0.0f;
    out->rough[1] = 0.0f;
    out->rough[2] = 0.0f;
    for (uint8_t i = 0U; i < w->n; i++) {
        out->rough[0] += fabsf(w->ax[i] - mx);
        out->rough[1] += fabsf(w->ay[i] - my);
        out->rough[2] += fabsf(w->az[i] - mz);
    }
    for (uint8_t a = 0U; a < 3U; a++) {
        out->rough[a] = (out->rough[a] / (float)w->n) * to_counts;
    }

    /*
     * AN4248 takes the gravity vector in north-east-down body axes; the
     * accelerometer gives the specific force in X forward, Y left, Z up.
     * The two sign flips of Z cancel: Gp = (-ax, ay, az).
     */
    float gx = -mx;
    float gy = my;
    float gz = mz;
    float roll = atan2f(gy, gz);
    float pitch = atanf(-gx / ((gy * sinf(roll)) + (gz * cosf(roll))));

    out->roll_deg = roll * RAD_TO_DEG;
    out->pitch_deg = pitch * RAD_TO_DEG;
    return true;
}

float tilt_heading_deg(float pitch_deg, float roll_deg, float mx, float my, float mz)
{
    float roll = roll_deg * DEG_TO_RAD;
    float pitch = pitch_deg * DEG_TO_RAD;
    /* the field in north-east-down body axes: Y and Z flip */
    float bx = mx;
    float by = -my;
    float bz = -mz;
    float sr = sinf(roll);
    float cr = cosf(roll);
    float sp = sinf(pitch);
    float cp = cosf(pitch);

    /* AN4248, eq. 22 */
    float heading = atan2f((bz * sr) - (by * cr), (bx * cp) + (by * sp * sr) + (bz * sp * cr)) *
                    RAD_TO_DEG;

    if (heading < 0.0f) {
        heading += 360.0f;
    }
    if (heading >= 360.0f) {
        heading -= 360.0f;
    }
    return heading;
}
