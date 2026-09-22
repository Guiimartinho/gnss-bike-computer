/**
 * @file power_metrics.c
 * @brief Normalised power, intensity factor and training stress
 *
 * The method, the units and the rule about feeding it are in
 * model/power_metrics.h.
 */

#include <math.h>
#include <string.h>

#include "model/power_metrics.h"

void power_metrics_init(struct power_metrics *pm, uint16_t ftp_w)
{
    if (pm == NULL) {
        return;
    }

    (void)memset(pm, 0, sizeof(*pm));
    pm->ftp_w = ftp_w;
}

void power_metrics_set_ftp(struct power_metrics *pm, uint16_t ftp_w)
{
    if (pm != NULL) {
        pm->ftp_w = ftp_w;
    }
}

void power_metrics_add(struct power_metrics *pm, uint16_t power_w)
{
    if (pm == NULL) {
        return;
    }

    if (power_w > PM_POWER_MAX_W) {
        /*
         * A spike no rider produces. It counts as a zero second instead of
         * being dropped: dropping it would slide the whole window and make
         * the rolling average cover more than thirty seconds.
         */
        power_w = 0U;
    }

    /* the sample leaving the window goes out of the running sum */
    pm->ring_sum -= pm->ring[pm->head];
    pm->ring[pm->head] = power_w;
    pm->ring_sum += power_w;
    pm->head = (uint8_t)((pm->head + 1U) % PM_WINDOW_S);

    if (pm->filled < PM_WINDOW_S) {
        pm->filled++;
    }

    pm->total_sum += power_w;
    pm->total_n++;
    pm->seconds++;

    if (pm->filled < PM_WINDOW_S) {
        /* fewer than thirty seconds: there is no rolling average yet */
        return;
    }

    double avg = (double)pm->ring_sum / (double)PM_WINDOW_S;
    double sq = avg * avg;

    pm->quartic_sum += sq * sq;
    pm->quartic_n++;
}

uint16_t power_metrics_np(const struct power_metrics *pm)
{
    if ((pm == NULL) || (pm->quartic_n == 0U)) {
        return 0U;
    }

    float mean = (float)(pm->quartic_sum / (double)pm->quartic_n);

    /* the fourth root, as two square roots */
    return (uint16_t)(sqrtf(sqrtf(mean)) + 0.5f);
}

uint16_t power_metrics_avg(const struct power_metrics *pm)
{
    if ((pm == NULL) || (pm->total_n == 0U)) {
        return 0U;
    }

    return (uint16_t)((pm->total_sum + (pm->total_n / 2U)) / pm->total_n);
}

uint16_t power_metrics_if100(const struct power_metrics *pm)
{
    if ((pm == NULL) || (pm->ftp_w == 0U)) {
        return 0U;
    }

    uint32_t np = power_metrics_np(pm);

    return (uint16_t)(((np * 100U) + (pm->ftp_w / 2U)) / pm->ftp_w);
}

uint16_t power_metrics_tss(const struct power_metrics *pm)
{
    if ((pm == NULL) || (pm->ftp_w == 0U)) {
        return 0U;
    }

    uint32_t np = power_metrics_np(pm);

    if (np == 0U) {
        return 0U;
    }

    /*
     * TSS = t x NP x IF / (FTP x 3600) x 100, and IF = NP / FTP, so
     * TSS = t x NP^2 x 100 / (FTP^2 x 3600). In 64 bits because
     * t x NP^2 x 100 passes four thousand million within the first hour.
     */
    uint64_t num = (uint64_t)pm->seconds * np * np * 100U;
    uint64_t den = (uint64_t)pm->ftp_w * pm->ftp_w * 3600U;

    return (uint16_t)((num + (den / 2U)) / den);
}

uint16_t power_metrics_vi100(const struct power_metrics *pm)
{
    uint32_t avg = power_metrics_avg(pm);

    if (avg == 0U) {
        return 0U;
    }

    uint32_t np = power_metrics_np(pm);

    return (uint16_t)(((np * 100U) + (avg / 2U)) / avg);
}
