/**
 * @file ui_samples.c
 * @brief Sample snapshots for the host renderer: a ride with the values of
 *        the docs/18 mock-ups
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_samples.h"

static void copy(char *dst, size_t size, const char *src)
{
    (void)snprintf(dst, size, "%s", src);
}

/** Wavy track from (x0, y0) to (x1, y1) in per mille, npts points */
static void track(ui_pt_t *pts, uint32_t npts, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                  float wave)
{
    for (uint32_t i = 0U; i < npts; i++) {
        float t = (float)i / (float)(npts - 1U);
        float x = (float)x0 + (t * (float)(x1 - x0));
        float y = (float)y0 + (t * (float)(y1 - y0)) + (wave * sinf(t * 9.0f));

        pts[i].x = (int16_t)lroundf(x);
        pts[i].y = (int16_t)lroundf(y);
    }
}

static void segment(ui_segment_t *s, bool on, uint8_t pct, float adv, float cur, const char *name,
                    int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rider_i, int16_t course)
{
    (void)memset(s, 0, sizeof(*s));
    s->on = on;
    s->pct = pct;
    s->advance_s = adv;
    s->cur_s = cur;
    copy(s->name, sizeof(s->name), name);
    s->npts = 40U;
    track(s->pts, s->npts, x0, y0, x1, y1, 60.0f);
    s->rider = s->pts[(rider_i < s->npts) ? rider_i : (s->npts - 1U)];
    s->course_deg = course;
}

void ui_sample_ride(ui_model_t *m)
{
    (void)memset(m, 0, sizeof(*m));

    m->status.time_s = (7U * 3600U) + (42U * 60U) + 13U;
    m->status.gnss = UI_GNSS_FIX;
    m->status.ant_link = true;
    m->status.ble_link = true;
    m->status.recording = true;
    m->status.charge = UI_CHARGE_SOLAR;
    m->status.batt_pct = 91U;

    /* the ride and the lap of `model/activity.h`, which the legacy has not */
    m->act.timer_s = 2745U;         /* 45 min 45 s moving */
    m->act.elapsed_s = 3012U;       /* and 50 min 12 s on the clock */
    m->act.lap_timer_s = 545U;
    m->act.lap_dist_m = 3240.0f;
    m->act.avg_kmh = 23.9f;
    m->act.max_kmh = 58.2f;
    m->act.descent_m = 540.0f;
    m->act.laps = 3U;
    m->act.kcal = 612U;

    m->ride.dist_m = 18200.0f;
    m->ride.speed_kmh = 20.0f;
    m->ride.avg_kmh = 23.4f;
    m->ride.climb_m = 575.0f;
    m->ride.alt_m = 812.0f;
    m->ride.va_ms = 0.21f;
    m->ride.score = 0.2f;
    m->ride.pwr_w = 222U;
    m->ride.next_seg_m = 875U;
    m->ride.solar_mw = 18U;
    m->ride.cad_rpm = 88U;
    m->ride.hr_bpm = 147U;
    m->ride.slope_pct = 4;
    m->ride.pr = 2U;

    m->nseg = 0U;
    segment(&m->seg[0], true, 45U, 12.4f, 190.0f, "Serra do Mar", 60, 820, 940, 180, 18U, 120);
    segment(&m->seg[1], true, 35U, -72.2f, 360.0f, "Alto da Boa Vista", 80, 200, 930, 760, 14U, 60);

    m->nav.valid = true;
    m->nav.dist_m = 350U;
    m->nav.turn = UI_TURN_RIGHT;
    copy(m->nav.street, sizeof(m->nav.street), "Rua das Flores");

    m->rr.nzones = 5U;
    m->rr.cur = 2U;
    m->rr.val[0] = 42.0f;
    m->rr.val[1] = 38.0f;
    m->rr.val[2] = 31.0f;
    m->rr.val[3] = 24.0f;
    m->rr.val[4] = 18.0f;

    m->att.pitch_pct = 7.2f;
    m->att.histo_n = UI_HISTO_MAX;
    for (uint32_t i = 0U; i < UI_HISTO_MAX; i++) {
        m->att.histo[i] = (int8_t)lroundf((8.0f * sinf((float)i / 4.0f)) + (float)((i * 7U) % 5U) - 2.0f);
    }
    m->att.heading_deg = 35;
    m->att.rough[0] = 35.2f;
    m->att.rough[1] = 28.7f;
    m->att.rough[2] = 61.4f;
    m->att.rough[3] = 0.4f;

    static const char *const routes[] = {"Serra do Mar", "Volta da represa", "Treino de subida",
                                         "Estrada velha"};

    m->routes.n = (uint8_t)(sizeof(routes) / sizeof(routes[0]));
    for (uint32_t i = 0U; i < m->routes.n; i++) {
        (void)snprintf(m->routes.name[i], sizeof(m->routes.name[i]), "%s", routes[i]);
    }

    m->route.remain_km = 31.5f;
    m->route.n = 120U;
    for (uint32_t i = 0U; i < m->route.n; i++) {
        float t = (float)i / (float)(m->route.n - 1U);

        m->route.pts[i].x = (int16_t)lroundf(-200.0f + (t * 1400.0f));
        m->route.pts[i].y = (int16_t)lroundf(500.0f + (260.0f * sinf((t * 6.0f) - 1.2f)) - (300.0f * (t - 0.42f)));
    }
    m->route.done = 50U;
    m->route.rider = m->route.pts[50];
    m->route.course_deg = 70;
    m->route.scale_m = 250U;
    m->route.scale_pm = 200U;

    /* elevation profile of the same route */
    m->profile.n = 100U;
    for (uint32_t i = 0U; i < m->profile.n; i++) {
        float t = (float)i / (float)(m->profile.n - 1U);

        m->profile.alt_m[i] = (int16_t)lroundf(320.0f + (180.0f * sinf(t * 3.4f)) +
                                               (60.0f * sinf(t * 11.0f)));
    }
    m->profile.here = 38U;
    m->profile.min_m = 318;
    m->profile.max_m = 512;
    m->profile.climb_left_m = 640U;
    m->profile.remain_km = 31.5f;

    m->fec.time_s = (42U * 60U) + 17U;
    m->fec.score = 12.3f;
    m->fec.pwr_w = 245U;
    m->fec.rr_ms = 38U;
    m->fec.cad_rpm = 92U;
    m->fec.hr_bpm = 151U;
    m->fec.zone = 4U;
    {
        static const uint8_t z[UI_PWR_ZONES] = {8U, 22U, 30U, 26U, 12U, 6U, 2U};

        (void)memcpy(m->fec.zone_pct, z, sizeof(z));
    }
    m->fec.vector_valid = true;
    for (uint32_t i = 0U; i < UI_VECTOR_PTS; i++) {
        float a = (6.2831853f * (float)i) / (float)UI_VECTOR_PTS;

        m->fec.vector[i] = (uint8_t)lroundf(100.0f * (0.35f + (0.6f * fabsf(sinf(a + 0.3f)))));
    }

    m->gnss.mode = UI_GNSS_MODE_LEAP;
    m->gnss.fix3d = true;
    m->gnss.nsat = 18U;
    m->gnss.used = 14U;
    for (uint32_t i = 0U; i < m->gnss.nsat; i++) {
        ui_sat_t *s = &m->gnss.sat[i];

        s->az_deg = (int16_t)((i * 47U) % 360U);
        s->el_deg = (int8_t)(10U + ((i * 23U) % 75U));
        s->cn0 = (uint8_t)(44U - ((i * 11U) % 26U));
        s->sys = (uint8_t)(i % 3U);
        s->used = (i < m->gnss.used);
    }
    m->gnss.fix_age_s = 0U;
    m->gnss.hacc_m = 2.1f;

    m->energy.mv = 3920U;
    m->energy.ma = -42;
    m->energy.pct = 91U;
    m->energy.source = UI_CHARGE_SOLAR;
    m->energy.solar_mw = 18U;
    m->energy.solar_limit_mv = 3900U;
    m->energy.temp_c = 31;
    m->energy.autonomy_h = 290U;

    m->sensors.n = 6U;
    m->sensors.s[0] = (ui_sensor_t){UI_SENSOR_HR, UI_LINK_CONNECTED, true, 17334U, "", "147 bpm"};
    m->sensors.s[1] = (ui_sensor_t){UI_SENSOR_BSC, UI_LINK_LOST, true, 15568U, "", ""};
    m->sensors.s[2] = (ui_sensor_t){UI_SENSOR_POWER, UI_LINK_CONNECTED, false, 0U, "Assioma", "222 W"};
    m->sensors.s[3] = (ui_sensor_t){UI_SENSOR_FEC, UI_LINK_NONE, true, 0U, "", ""};
    m->sensors.s[4] = (ui_sensor_t){UI_SENSOR_RADAR, UI_LINK_CONNECTED, true, 3301U, "", "livre"};
    m->sensors.s[5] = (ui_sensor_t){UI_SENSOR_LIGHT, UI_LINK_SEARCH, true, 1204U, "", ""};

    m->pair.searching = true;
    m->pair.kind = UI_SENSOR_HR;
    m->pair.n = 4U;
    m->pair.item[0] = (ui_pair_item_t){true, 17334U, "", -52};
    m->pair.item[1] = (ui_pair_item_t){true, 20111U, "", -71};
    m->pair.item[2] = (ui_pair_item_t){false, 0U, "Polar H10", -60};
    m->pair.item[3] = (ui_pair_item_t){false, 0U, "HRM-Pro", -78};

    m->settings.ftp_w = 250U;
    m->settings.weight_kg = 72U;
    m->settings.gnss_leap = true;
    m->settings.light_auto = true;
    m->settings.solar_limit_mv = 3900U;

    m->debug.seg_loaded = 12U;
    copy(m->debug.version, sizeof(m->debug.version), "3.0.0");
}

void ui_sample_one_segment(ui_model_t *m, bool on)
{
    m->nseg = 1U;
    m->seg[0].on = on;
    if (!on) {
        /* approaching: the model centres the window on the rider (legacy SEG_OFF) */
        m->seg[0].rider.x = 500;
        m->seg[0].rider.y = 500;
    }
}

void ui_sample_two_segments(ui_model_t *m, bool on0, bool on1)
{
    m->nseg = 2U;
    m->seg[0].on = on0;
    m->seg[1].on = on1;
}

void ui_sample_searching(ui_model_t *m)
{
    m->status.gnss = UI_GNSS_SEARCH;
    m->status.recording = false;
    m->gnss.mode = UI_GNSS_MODE_FULL;
    m->gnss.fix3d = false;
    m->gnss.nsat = 8U;
    m->gnss.used = 5U;
    m->gnss.fix_age_s = 8U;
    static const struct {
        int16_t az;
        int8_t el;
        uint8_t cn0;
    } sky[8] = {
        {40, 60, 41}, {110, 35, 38}, {200, 55, 30}, {260, 20, 22},
        {320, 45, 36}, {15, 10, 19}, {150, 80, 40}, {285, 62, 29},
    };

    for (uint32_t i = 0U; i < 8U; i++) {
        m->gnss.sat[i].az_deg = sky[i].az;
        m->gnss.sat[i].el_deg = sky[i].el;
        m->gnss.sat[i].cn0 = sky[i].cn0;
        m->gnss.sat[i].sys = (uint8_t)(i % 3U);
        m->gnss.sat[i].used = (sky[i].cn0 >= 28U);
    }
}

void ui_sample_trainer(ui_model_t *m)
{
    m->status.gnss = UI_GNSS_OFF;
    m->status.charge = UI_CHARGE_NONE;
    m->status.recording = true;
}
