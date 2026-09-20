/**
 * @file ui_render.c
 * @brief Host renderer of the user interface: every screen to a PPM file
 *
 * Builds each screen with the sample ride, lets LVGL draw it into a
 * 240 x 400 RGB565 buffer and writes it as the panel shows it: the JDI keeps
 * the top bit of each channel (8 colours) and the Sharp is black and white,
 * by the rules of the display driver (memlcd_pixel.h).
 * It also checks what the panels need:
 *  - only black and white on the mono theme, after the panel quantisation;
 *  - no label runs out of the box that holds it;
 *  - the key navigation and the actions of the legacy menus.
 * It prints the most LVGL heap and stack the screens used.
 * Exit status 1 when a check fails. Usage: ui_render [output_dir]
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

#include "lvgl.h"
#include "drivers/display/memlcd_pixel.h"
#include "ui/ui.h"
#include "ui_internal.h"
#include "ui_samples.h"

static uint16_t fb[UI_WIDTH * UI_HEIGHT];
static uint32_t tick_ms;
static const char *out_dir = "ui_out";
static int errors;
static int rendered;
static size_t heap_peak;
static uintptr_t stack_top;
static size_t stack_peak;
static char heap_peak_screen[40];

/** The 8 colours as a reflective panel shows them (docs/18, tools/docs/screens_drawing.py) */
static const uint8_t panel_rgb[8][3] = {
    {0x16, 0x18, 0x1a},     /* 000 black */
    {0x2f, 0x5f, 0xb8},     /* 001 blue */
    {0x2e, 0x9d, 0x4a},     /* 010 green */
    {0x2a, 0xa3, 0xb5},     /* 011 cyan */
    {0xc8, 0x32, 0x3c},     /* 100 red */
    {0xa8, 0x3a, 0x95},     /* 101 magenta */
    {0xd9, 0xa9, 0x00},     /* 110 yellow */
    {0xf4, 0xf5, 0xf0},     /* 111 white */
};

/*
 * Stack of the interface: the stack under main() is painted, the screens
 * are built and drawn, and snap() scans the paint before its own checks and
 * file writes, then paints again. On the PC (x86-64: 8-byte pointers, and on
 * Windows 32 bytes of shadow space per call) it bounds from above what LVGL
 * and the screens take from the ui thread on the Cortex-M33; the display
 * driver path is measured apart with CONFIG_STACK_USAGE (docs/05).
 */
#define STACK_PAINT_BYTES   (128U * 1024U)
#define STACK_PAINT         0xA5U

static uintptr_t paint_low;

static __attribute__((noinline)) void stack_paint(void)
{
    volatile uint8_t area[STACK_PAINT_BYTES];

    for (size_t i = 0U; i < STACK_PAINT_BYTES; i++) {
        area[i] = STACK_PAINT;
    }
    /* only the address is kept, to scan the painted stack after the drawing */
    // cppcheck-suppress danglingLifetime
    paint_low = (uintptr_t)&area[0];
}

/** Bytes used under @p top since stack_paint() */
static __attribute__((noinline)) size_t stack_used(uintptr_t top)
{
    const volatile uint8_t *p = (const volatile uint8_t *)paint_low;
    size_t i = 0U;

    while ((i < STACK_PAINT_BYTES) && (p[i] == STACK_PAINT)) {
        i++;
    }
    return (size_t)(top - (paint_low + i));
}

static uint32_t host_tick(void)
{
    return tick_ms;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    (void)area;
    (void)px;
    lv_display_flush_ready(disp);
}

static char actions[512];

static void on_action(ui_action_t action, int32_t arg, void *user)
{
    char one[32];

    (void)user;
    if (action == UI_ACT_KEY) {
        return;
    }
    (void)snprintf(one, sizeof(one), "|%d:%ld|", (int)action, (long)arg);
    (void)strncat(actions, one, sizeof(actions) - strlen(actions) - 1U);
}

/**
 * LVGL 9.5 anti-aliases rounded corners, circles, slanted lines and
 * triangles whatever lv_display_set_antialiasing() says (it only reaches
 * layers and images). The display driver keeps the top bit of each channel,
 * as the panel does, so those pixels land on a panel colour anyway. What must
 * not happen is colour on the black and white theme after that step.
 */
static void check_palette(const char *name, ui_theme_t theme)
{
    uint32_t coloured = 0U;

    if (theme != UI_THEME_MONO) {
        return;
    }
    for (uint32_t i = 0U; i < (UI_WIDTH * UI_HEIGHT); i++) {
        uint32_t idx = memlcd_rgb565_to_rgb3(fb[i]);

        if ((idx != 0U) && (idx != 7U)) {
            coloured++;
        }
    }
    if (coloured != 0U) {
        printf("FAIL %s: %lu coloured pixels on the black and white theme\n", name, (unsigned long)coloured);
        errors++;
    }
}

/** Labels that run out of their parent box */
static void check_clip(const lv_obj_t *obj, const char *name)
{
    uint32_t n = lv_obj_get_child_count(obj);

    for (uint32_t i = 0U; i < n; i++) {
        const lv_obj_t *c = lv_obj_get_child(obj, (int32_t)i);

        if (lv_obj_check_type(c, &lv_label_class) && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
            lv_area_t pa;
            lv_area_t ca;

            lv_obj_get_coords(obj, &pa);
            lv_obj_get_coords(c, &ca);
            if ((ca.x1 < pa.x1) || (ca.x2 > pa.x2) || (ca.y1 < pa.y1) || (ca.y2 > pa.y2)) {
                printf("FAIL %s: label \"%s\" (%ld..%ld, %ld..%ld) out of its box (%ld..%ld, %ld..%ld)\n",
                       name, lv_label_get_text(c), (long)ca.x1, (long)ca.x2, (long)ca.y1, (long)ca.y2,
                       (long)pa.x1, (long)pa.x2, (long)pa.y1, (long)pa.y2);
                errors++;
            }
        }
        check_clip(c, name);
    }
}

static void dump(const char *name, ui_theme_t theme)
{
    char path[256];
    FILE *f;

    (void)snprintf(path, sizeof(path), "%s/%s_%s.ppm", out_dir, name,
                   (theme == UI_THEME_MONO) ? "mono" : "cor");
    f = fopen(path, "wb");
    if (f == NULL) {
        printf("FAIL cannot write %s\n", path);
        errors++;
        return;
    }
    (void)fprintf(f, "P6\n%d %d\n255\n", UI_WIDTH, UI_HEIGHT);
    for (uint32_t i = 0U; i < (UI_WIDTH * UI_HEIGHT); i++) {
        (void)fwrite(panel_rgb[memlcd_rgb565_to_rgb3(fb[i])], 1, 3, f);
    }
    (void)fclose(f);
}

/** Renders the active screen, checks it, writes it and tracks the LVGL heap and stack in use */
static void snap(const char *name, ui_theme_t theme)
{
    lv_mem_monitor_t mon;
    size_t used;

    lv_refr_now(lv_display_get_default());
    /* stack of the screens since the last paint, before the checks and the file */
    used = stack_used(stack_top);
    if (used > stack_peak) {
        stack_peak = used;
    }
    check_palette(name, theme);
    check_clip(lv_screen_active(), name);
    check_clip(lv_layer_top(), name);
    dump(name, theme);
    rendered++;

    lv_mem_monitor(&mon);
    if ((mon.total_size - mon.free_size) > heap_peak) {
        heap_peak = mon.total_size - mon.free_size;
        (void)snprintf(heap_peak_screen, sizeof(heap_peak_screen), "%s", name);
    }
    stack_paint();
}

static void step(uint32_t ms)
{
    tick_ms += ms;
    ui_tick(tick_ms);
    lv_timer_handler();
}

static void expect_screen(ui_screen_t want, const char *what)
{
    if (ui_current() != want) {
        printf("FAIL %s: screen %d, expected %d\n", what, (int)ui_current(), (int)want);
        errors++;
    }
}

static void expect_action(const char *want, const char *what)
{
    if (strstr(actions, want) == NULL) {
        printf("FAIL %s: actions \"%s\" miss \"%s\"\n", what, actions, want);
        errors++;
    }
}

static void render_theme(ui_theme_t theme)
{
    /* static, as the snapshot of the firmware: off the measured stack */
    static ui_model_t m;
    ui_config_t cfg = {theme, UI_LANG_PT, "3.0.0", on_action, NULL};

    tick_ms = 0U;
    actions[0] = '\0';
    ui_sample_ride(&m);
    if (ui_init(&cfg, tick_ms) != 0) {
        printf("FAIL ui_init\n");
        errors++;
        return;
    }
    ui_update(&m, tick_ms);
    snap("01_partida", theme);

    /* CRS pages */
    ui_set_mode(UI_MODE_CRS);
    ui_update(&m, tick_ms);
    expect_screen(UI_SCREEN_CRS1, "mode CRS");
    snap("02_crs1", theme);

    ui_sample_one_segment(&m, true);
    ui_update(&m, tick_ms);
    snap("03_crs1_1seg", theme);

    ui_sample_one_segment(&m, false);
    ui_update(&m, tick_ms);
    snap("04_crs1_1seg_perto", theme);

    ui_sample_two_segments(&m, true, true);
    ui_update(&m, tick_ms);
    snap("05_crs1_2seg", theme);

    ui_sample_two_segments(&m, false, true);
    ui_update(&m, tick_ms);
    snap("06_crs1_2seg_1perto", theme);

    ui_sample_two_segments(&m, true, false);
    ui_update(&m, tick_ms);
    snap("07_crs1_2seg_2perto", theme);

    ui_sample_two_segments(&m, false, false);
    ui_update(&m, tick_ms);
    snap("08_crs1_2seg_perto", theme);

    ui_sample_ride(&m);
    ui_update(&m, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_CRS2, "right on page 1");
    snap("09_crs2", theme);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_CRS3, "right on page 2");
    snap("10_crs3", theme);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_CRS1, "right on page 3");

    /* notification over page 1 */
    ui_notify("Segmento", "Serra do Mar", "+12.4 s", true, 6000U, tick_ms);
    snap("11_notificacao", theme);
    ui_key(UI_KEY_LEFT, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_CRS1, "key closes the notification");

    /* GNSS searching: the page gives way to the sky */
    ui_sample_searching(&m);
    ui_update(&m, tick_ms);
    expect_screen(UI_SCREEN_GPS, "old position");
    snap("12_gnss", theme);
    ui_sample_ride(&m);
    ui_update(&m, tick_ms);
    expect_screen(UI_SCREEN_CRS1, "fix back");

    /* PRC with and without a route */
    ui_set_mode(UI_MODE_PRC);
    ui_update(&m, tick_ms);
    expect_screen(UI_SCREEN_PRC, "mode PRC");
    snap("13_prc", theme);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    expect_action("|12:1|", "PRC zoom in");
    m.route.n = 0U;
    ui_update(&m, tick_ms);
    snap("14_prc_sem_percurso", theme);
    ui_sample_ride(&m);

    /* FEC, waiting for the trainer and riding */
    ui_sample_trainer(&m);
    m.fec.time_s = 0U;
    ui_set_mode(UI_MODE_FEC);
    ui_update(&m, tick_ms);
    snap("15_fec_conectando", theme);
    ui_sample_ride(&m);
    ui_sample_trainer(&m);
    ui_update(&m, tick_ms);
    snap("16_fec", theme);

    /* DBG */
    ui_sample_ride(&m);
    ui_set_mode(UI_MODE_DBG);
    ui_update(&m, tick_ms);
    snap("17_dbg", theme);

    /* menus: the centre opens the menu after the 5 s lock */
    ui_set_mode(UI_MODE_CRS);
    ui_update(&m, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_CRS1, "menu locked in the first 5 s");
    step(6000U);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_MENU, "centre opens the menu");
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    snap("18_menu", theme);

    /* Modo PRC: the list of routes, then the second route */
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_ROUTES, "Modo PRC opens the routes");
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    snap("18b_percursos", theme);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_action("|14:1||0:1|", "second route, then mode PRC");
    expect_screen(UI_SCREEN_PRC, "PRC after the route");

    /* without routes: error notification and back to the page */
    m.routes.n = 0U;
    ui_update(&m, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_PRC, "no route keeps the page");
    snap("18c_sem_percursos", theme);
    ui_key(UI_KEY_LEFT, UI_PRESS_SHORT, tick_ms);
    ui_sample_ride(&m);
    ui_set_mode(UI_MODE_CRS);
    ui_update(&m, tick_ms);

    /* Ajustes */
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_LEFT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_LEFT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_SETTINGS, "Ajustes from the menu");
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    snap("19_ajustes", theme);

    /* sensors and pairing */
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_SENSORS, "Sensores from Ajustes");
    ui_update(&m, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    snap("20_sensores", theme);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_PAIR, "pairing from Sensores");
    expect_action("|2:0|", "pair start for FC");
    ui_update(&m, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    snap("21_parear", theme);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_action("|3:0|", "pair the first device");
    expect_screen(UI_SCREEN_SENSORS, "back to Sensores");

    /* value editor */
    ui_go(UI_SCREEN_SETTINGS);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_screen(UI_SCREEN_VALUE, "FTP editor");
    snap("22_valor", theme);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    ui_key(UI_KEY_CENTER, UI_PRESS_SHORT, tick_ms);
    expect_action("|5:251|", "FTP saved plus one");

    /* screen and light, format confirmation, energy */
    ui_go(UI_SCREEN_LIGHT);
    snap("23_tela_luz", theme);
    ui_go(UI_SCREEN_CONFIRM);
    ui_key(UI_KEY_RIGHT, UI_PRESS_SHORT, tick_ms);
    snap("24_formatar", theme);
    ui_go(UI_SCREEN_ENERGY);
    ui_update(&m, tick_ms);
    snap("25_energia", theme);

    /* system screens */
    m.status.charge = UI_CHARGE_USB;
    m.status.recording = false;
    ui_update(&m, tick_ms);
    ui_show(UI_SCREEN_USB);
    snap("26_usb", theme);
    /* the USB screen stays over a mode change; the pages come back when the card does */
    ui_set_mode(UI_MODE_FEC);
    expect_screen(UI_SCREEN_USB, "mode change on the USB screen");
    ui_show_pages();
    expect_screen(UI_SCREEN_FEC, "pages back after USB");
    ui_show(UI_SCREEN_SHUTDOWN);
    ui_set_progress(70U);
    snap("27_desligando", theme);

    /* update over the air: it takes the screen and gives the pages back */
    ui_show_pages();
    ui_set_dfu(UI_DFU_RUNNING, 45U);
    expect_screen(UI_SCREEN_DFU, "the update takes the screen");
    snap("28_atualizacao", theme);
    ui_set_dfu(UI_DFU_DONE, 100U);
    snap("29_atualizacao_pronta", theme);
    ui_set_dfu(UI_DFU_IDLE, 0U);
    expect_screen(UI_SCREEN_FEC, "pages back after the update");

    /* long centre on a page asks to shut down */
    ui_set_mode(UI_MODE_CRS);
    ui_go(UI_SCREEN_CRS1);
    ui_key(UI_KEY_CENTER, UI_PRESS_LONG, tick_ms);
    expect_action("|1:0|", "long centre shuts down");
}

int main(int argc, char **argv)
{
    lv_display_t *disp;
    volatile uint8_t top_marker = 0U;
    size_t used;

    stack_top = (uintptr_t)&top_marker;
    if (argc > 1) {
        out_dir = argv[1];
    }
    (void)MKDIR(out_dir);

    lv_init();
    lv_tick_set_cb(host_tick);
    disp = lv_display_create(UI_WIDTH, UI_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);

    stack_paint();
    render_theme(UI_THEME_COLOR);
    render_theme(UI_THEME_MONO);
    /* the key steps after the last picture */
    used = stack_used(stack_top);
    if (used > stack_peak) {
        stack_peak = used;
    }

    printf("LVGL heap in use at most %lu bytes (%s), with 64-bit pointers\n", (unsigned long)heap_peak,
           heap_peak_screen);
    printf("Stack of the screens at most %lu bytes, on x86-64\n", (unsigned long)stack_peak);
    printf("%d screens rendered to %s, %d problems\n", rendered, out_dir, errors);
    return (errors == 0) ? 0 : 1;
}
