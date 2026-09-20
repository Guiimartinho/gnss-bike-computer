/**
 * @file ui_fields.c
 * @brief Data fields of the pages, with the legacy formats
 *
 * Each field is a legacy cadran with its label, unit and number format:
 * VueCRS.cpp, VuePRC.cpp, VueFEC.cpp and VueGPS.cpp (distance in km with one
 * truncated decimal, speed with one, vertical speed with two, average speed
 * with two, altitude and suffer score with one, the rest as integers).
 */

#include <stdio.h>
#include <string.h>

#include "ui_internal.h"

typedef struct {
    ui_text_t label;
    const char *unit;
    ui_role_t label_role;
} ui_fspec_t;

static const ui_fspec_t fspec[UI_F_COUNT] = {
    [UI_F_DIST] = {T_DIST, "km", UI_C_FG},
    [UI_F_PWR] = {T_PWR, "W", UI_C_FG},
    [UI_F_SPEED] = {T_SPEED, "km/h", UI_C_FG},
    [UI_F_CLIMB] = {T_CLIMB, "m", UI_C_FG},
    [UI_F_CAD] = {T_CAD, "rpm", UI_C_FG},
    [UI_F_HR] = {T_HR, "bpm", UI_C_FG},
    [UI_F_SLOPE] = {T_SLOPE, "%", UI_C_FG},
    [UI_F_VA] = {T_VA, "m/s", UI_C_FG},
    [UI_F_NEXT_SEG] = {T_NEXT_SEG, "m", UI_C_SEG},
    [UI_F_AVG] = {T_AVG, "km/h", UI_C_FG},
    [UI_F_SCORE] = {T_SCORE, NULL, UI_C_FG},
    [UI_F_SOLAR] = {T_SOLAR, "mW", UI_C_FG},
    [UI_F_BATT] = {T_BATT, "%", UI_C_FG},
    [UI_F_PR] = {T_PR, NULL, UI_C_FG},
    [UI_F_NEXT_TURN] = {T_NEXT_TURN, "m", UI_C_NAV},
    [UI_F_FEC_TIME] = {T_ELAPSED, NULL, UI_C_FG},
    [UI_F_FEC_CAD] = {T_CAD, "rpm", UI_C_FG},
    [UI_F_FEC_HR] = {T_HR, "bpm", UI_C_FG},
    [UI_F_FEC_SCORE] = {T_SCORE, NULL, UI_C_FG},
    [UI_F_FEC_PWR] = {T_PWR, "W", UI_C_FG},
    [UI_F_TIME] = {T_TIME, NULL, UI_C_FG},
    [UI_F_ALT] = {T_ALT, "m", UI_C_FG},
};

typedef struct {
    ui_fid_t id;
    ui_field_t f;
} ui_field_slot_t;

#define UI_FIELDS_MAX   16U

static ui_field_slot_t slots[UI_FIELDS_MAX];
static uint32_t nslots;

void ui_fields_forget(void)
{
    (void)memset(slots, 0, sizeof(slots));
    nslots = 0U;
}

void ui_fields_create(lv_obj_t *parent, const ui_fplace_t *places, uint32_t n)
{
    for (uint32_t i = 0U; (i < n) && (nslots < UI_FIELDS_MAX); i++) {
        const ui_fplace_t *p = &places[i];
        const ui_fspec_t *s = &fspec[p->id];
        ui_field_slot_t *slot = &slots[nslots++];

        slot->id = p->id;
        ui_field_create(&slot->f, parent, p->col, p->row, p->span, ui_txt(s->label), s->unit,
                        s->label_role);
    }
    ui_fields_update();
}

/** Value of a field as the legacy prints it, and its colour */
static void field_value(ui_fid_t id, char *buf, size_t size, ui_role_t *role)
{
    const ui_model_t *m = &ui_ctx.m;

    *role = UI_C_FG;
    switch (id) {
    case UI_F_DIST:
        (void)ui_fmt_float(buf, size, m->ride.dist_m / 1000.0f, 1U);
        break;
    case UI_F_PWR:
        (void)ui_fmt_int(buf, size, m->ride.pwr_w);
        break;
    case UI_F_SPEED:
        (void)ui_fmt_float(buf, size, m->ride.speed_kmh, 1U);
        break;
    case UI_F_CLIMB:
        (void)ui_fmt_int(buf, size, (int32_t)m->ride.climb_m);
        break;
    case UI_F_CAD:
        (void)ui_fmt_int(buf, size, m->ride.cad_rpm);
        break;
    case UI_F_HR:
        (void)ui_fmt_int(buf, size, m->ride.hr_bpm);
        break;
    case UI_F_SLOPE:
        (void)ui_fmt_int(buf, size, m->ride.slope_pct);
        break;
    case UI_F_VA:
        (void)ui_fmt_float(buf, size, m->ride.va_ms, 2U);
        break;
    case UI_F_NEXT_SEG:
        (void)ui_fmt_int(buf, size, m->ride.next_seg_m);
        break;
    case UI_F_AVG:
        (void)ui_fmt_float(buf, size, m->ride.avg_kmh, 2U);
        break;
    case UI_F_SCORE:
        (void)ui_fmt_float(buf, size, m->ride.score, 1U);
        break;
    case UI_F_SOLAR:
        (void)ui_fmt_int(buf, size, m->ride.solar_mw);
        *role = (m->ride.solar_mw > 0U) ? UI_C_GOOD : UI_C_FG;
        break;
    case UI_F_BATT:
        (void)ui_fmt_int(buf, size, m->status.batt_pct);
        *role = (m->status.batt_pct <= 20U) ? UI_C_BAD : UI_C_FG;
        break;
    case UI_F_PR:
        (void)ui_fmt_int(buf, size, m->ride.pr);
        break;
    case UI_F_NEXT_TURN:
        if (m->nav.valid) {
            (void)ui_fmt_int(buf, size, m->nav.dist_m);
        } else {
            (void)snprintf(buf, size, "---");
        }
        break;
    case UI_F_FEC_TIME:
        (void)ui_fmt_hms(buf, size, m->fec.time_s, ':');
        break;
    case UI_F_FEC_CAD:
        (void)ui_fmt_int(buf, size, m->fec.cad_rpm);
        break;
    case UI_F_FEC_HR:
        (void)ui_fmt_int(buf, size, m->fec.hr_bpm);
        break;
    case UI_F_FEC_SCORE:
        (void)ui_fmt_float(buf, size, m->fec.score, 1U);
        break;
    case UI_F_FEC_PWR:
        (void)ui_fmt_int(buf, size, m->fec.pwr_w);
        break;
    case UI_F_TIME:
        (void)ui_fmt_hms(buf, size, m->status.time_s, ':');
        break;
    case UI_F_ALT:
        (void)ui_fmt_float(buf, size, m->ride.alt_m, 1U);
        break;
    default:
        (void)snprintf(buf, size, "---");
        break;
    }
}

void ui_fields_update(void)
{
    char buf[16];
    ui_role_t role;

    for (uint32_t i = 0U; i < nslots; i++) {
        field_value(slots[i].id, buf, sizeof(buf), &role);
        ui_field_set(&slots[i].f, buf, role);
    }
}
