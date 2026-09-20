/**
 * @file test_memlcd.c
 * @brief Frame and pixel rules of the display driver
 *        (modules/gnss_drivers: memlcd_pixel.h, memlcd_frame.c)
 *
 * References:
 *  - quantisation: docs/18-interface-telas.md (Implementação), the rule the
 *    host renderer of docs/telas uses;
 *  - portrait: legacy drawPixel() with setRotation(3), (x, y) to column y of
 *    line 239 - x (docs/08, Pipeline de desenho);
 *  - Sharp LS027B7DH01 (LCP-2110015A, 6-5): LSB first, 1 is white, per line
 *    address, 50 bytes and a dummy byte, a last dummy byte; the legacy buffer
 *    has the same 12,482 bytes;
 *  - JDI LPM027M128B (6.2 and 8): MSB first, M0 high, AG9-AG0 the line from
 *    1 to 240 in binary, red, green, blue per pixel, 16 clocks at the end.
 */

#include <string.h>

#include "unity.h"

#include "drivers/display/memlcd_frame.h"
#include "drivers/display/memlcd_pixel.h"

#define W 400U
#define H 240U

static uint8_t sharp_buf[MEMLCD_FRAME_SIZE(MEMLCD_SHARP, W, H)];
static uint8_t jdi_buf[MEMLCD_FRAME_SIZE(MEMLCD_JDI, W, H)];
static uint32_t sharp_dirty[MEMLCD_DIRTY_WORDS(H)];
static uint32_t jdi_dirty[MEMLCD_DIRTY_WORDS(H)];
static struct memlcd_frame sharp;
static struct memlcd_frame jdi;
/* one picture in portrait, RGB565 least significant byte first */
static uint8_t picture[240U * 400U * 2U];

void setUp(void)
{
    sharp = (struct memlcd_frame){sharp_buf, sharp_dirty, W, H, 90U, MEMLCD_SHARP};
    jdi = (struct memlcd_frame){jdi_buf, jdi_dirty, W, H, 90U, MEMLCD_JDI};
    memlcd_frame_init(&sharp);
    memlcd_frame_init(&jdi);
}

void tearDown(void)
{
}

static void fill(uint16_t colour, uint16_t w, uint16_t h)
{
    for (uint32_t i = 0U; i < ((uint32_t)w * h); i++) {
        picture[2U * i] = (uint8_t)(colour & 0xFFU);
        picture[(2U * i) + 1U] = (uint8_t)(colour >> 8);
    }
}

static uint32_t dirty_lines(const struct memlcd_frame *f)
{
    uint32_t n = 0U;

    for (uint16_t l = 0U; l < f->height; l++) {
        n += memlcd_frame_is_dirty(f, l) ? 1U : 0U;
    }
    return n;
}

/* --------------------------------------------------------------------------
 * Pixel rules
 * -------------------------------------------------------------------------- */

static void test_pure_colours_keep_the_top_bit_of_each_channel(void)
{
    TEST_ASSERT_EQUAL_UINT8(7U, memlcd_rgb565_to_rgb3(0xFFFFU));
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_rgb3(0x0000U));
    TEST_ASSERT_EQUAL_UINT8(4U, memlcd_rgb565_to_rgb3(0xF800U));    /* red */
    TEST_ASSERT_EQUAL_UINT8(2U, memlcd_rgb565_to_rgb3(0x07E0U));    /* green */
    TEST_ASSERT_EQUAL_UINT8(1U, memlcd_rgb565_to_rgb3(0x001FU));    /* blue */
    TEST_ASSERT_EQUAL_UINT8(6U, memlcd_rgb565_to_rgb3(0xFFE0U));    /* yellow */
    TEST_ASSERT_EQUAL_UINT8(3U, memlcd_rgb565_to_rgb3(0x07FFU));    /* cyan */
    TEST_ASSERT_EQUAL_UINT8(5U, memlcd_rgb565_to_rgb3(0xF81FU));    /* magenta */
}

static void test_grey_edges_go_black_or_white_by_their_mean(void)
{
    /* 128, 128, 128: mean 128, white */
    TEST_ASSERT_EQUAL_UINT8(7U, memlcd_rgb565_to_rgb3(0x8410U));
    /* 120, 124, 120: mean below 128, black (not the green of the top bits) */
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_rgb3(0x7BEFU));
    /* 120, 128, 120: green would win by the top bit alone */
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_rgb3((uint16_t)((15U << 11) | (32U << 5) | 15U)));
}

static void test_a_channel_48_apart_is_a_colour(void)
{
    /* 80, 128, 80: max - min = 48, coloured: green */
    TEST_ASSERT_EQUAL_UINT8(2U, memlcd_rgb565_to_rgb3((uint16_t)((10U << 11) | (32U << 5) | 10U)));
}

static void test_the_sharp_shows_colours_by_their_luminance(void)
{
    TEST_ASSERT_EQUAL_UINT8(1U, memlcd_rgb565_to_mono(0xFFFFU));
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_mono(0x0000U));
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_mono(0xF800U));    /* red, 74 */
    TEST_ASSERT_EQUAL_UINT8(1U, memlcd_rgb565_to_mono(0x07E0U));    /* green, 147 */
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_mono(0x001FU));    /* blue, 28 */
    TEST_ASSERT_EQUAL_UINT8(1U, memlcd_rgb565_to_mono(0xFFE0U));    /* yellow, 222 */
    TEST_ASSERT_EQUAL_UINT8(0U, memlcd_rgb565_to_mono(0xF81FU));    /* magenta, 102 */
}

static void test_portrait_is_the_legacy_rotation(void)
{
    uint16_t col;
    uint16_t line;

    memlcd_map(90U, W, H, 0U, 0U, &col, &line);
    TEST_ASSERT_EQUAL_UINT16(0U, col);
    TEST_ASSERT_EQUAL_UINT16(239U, line);
    memlcd_map(90U, W, H, 239U, 399U, &col, &line);
    TEST_ASSERT_EQUAL_UINT16(399U, col);
    TEST_ASSERT_EQUAL_UINT16(0U, line);
    memlcd_map(90U, W, H, 10U, 20U, &col, &line);
    TEST_ASSERT_EQUAL_UINT16(20U, col);
    TEST_ASSERT_EQUAL_UINT16(229U, line);
}

static void test_the_other_rotations_cover_the_panel(void)
{
    uint16_t col;
    uint16_t line;

    memlcd_map(0U, W, H, 5U, 7U, &col, &line);
    TEST_ASSERT_EQUAL_UINT16(5U, col);
    TEST_ASSERT_EQUAL_UINT16(7U, line);
    memlcd_map(180U, W, H, 0U, 0U, &col, &line);
    TEST_ASSERT_EQUAL_UINT16(399U, col);
    TEST_ASSERT_EQUAL_UINT16(239U, line);
    memlcd_map(270U, W, H, 0U, 0U, &col, &line);
    TEST_ASSERT_EQUAL_UINT16(399U, col);
    TEST_ASSERT_EQUAL_UINT16(0U, line);
}

static void test_sharp_pixel_zero_is_bit_zero(void)
{
    uint8_t line[50];

    (void)memset(line, 0xFF, sizeof(line));
    TEST_ASSERT_TRUE(memlcd_put_mono(line, 0U, 0U));
    TEST_ASSERT_EQUAL_HEX8(0xFEU, line[0]);
    TEST_ASSERT_TRUE(memlcd_put_mono(line, 15U, 0U));
    TEST_ASSERT_EQUAL_HEX8(0x7FU, line[1]);
    TEST_ASSERT_FALSE(memlcd_put_mono(line, 15U, 0U));
    TEST_ASSERT_TRUE(memlcd_put_mono(line, 0U, 1U));
    TEST_ASSERT_EQUAL_HEX8(0xFFU, line[0]);
}

static void test_jdi_pixels_go_red_green_blue_from_the_top_bit(void)
{
    uint8_t line[150];

    (void)memset(line, 0, sizeof(line));
    TEST_ASSERT_TRUE(memlcd_put_rgb3(line, 0U, 4U));        /* red: bit 7 */
    TEST_ASSERT_EQUAL_HEX8(0x80U, line[0]);
    TEST_ASSERT_TRUE(memlcd_put_rgb3(line, 1U, 1U));        /* blue: bit 2 */
    TEST_ASSERT_EQUAL_HEX8(0x84U, line[0]);
    TEST_ASSERT_TRUE(memlcd_put_rgb3(line, 2U, 7U));        /* bits 1, 0 and 7 of the next */
    TEST_ASSERT_EQUAL_HEX8(0x87U, line[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80U, line[1]);
    TEST_ASSERT_TRUE(memlcd_put_rgb3(line, 399U, 7U));      /* last pixel: bits 2, 1, 0 */
    TEST_ASSERT_EQUAL_HEX8(0x07U, line[149]);
    TEST_ASSERT_FALSE(memlcd_put_rgb3(line, 399U, 7U));
}

/* --------------------------------------------------------------------------
 * Frame on the wire
 * -------------------------------------------------------------------------- */

static void test_sharp_frame_is_the_legacy_12482_bytes(void)
{
    size_t tail_len;
    uint8_t *tail = memlcd_frame_tail(&sharp, &tail_len);

    TEST_ASSERT_EQUAL_UINT32(12482U, sizeof(sharp_buf));
    TEST_ASSERT_EQUAL_UINT32(52U, memlcd_frame_stride(&sharp));
    TEST_ASSERT_EQUAL_PTR(&sharp_buf[0], memlcd_frame_lead(&sharp));
    TEST_ASSERT_EQUAL_PTR(&sharp_buf[1], memlcd_frame_line(&sharp, 0U));
    TEST_ASSERT_EQUAL_UINT8(1U, memlcd_frame_line(&sharp, 0U)[0]);
    TEST_ASSERT_EQUAL_UINT8(240U, memlcd_frame_line(&sharp, 239U)[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFFU, memlcd_frame_pixels(&sharp, 0U)[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00U, memlcd_frame_line(&sharp, 0U)[51]);   /* line dummy */
    TEST_ASSERT_EQUAL_PTR(&sharp_buf[12481], tail);
    TEST_ASSERT_EQUAL_UINT32(1U, tail_len);
}

static void test_jdi_frame_has_the_10_bit_address_on_every_line(void)
{
    size_t tail_len;
    uint8_t *tail = memlcd_frame_tail(&jdi, &tail_len);

    TEST_ASSERT_EQUAL_UINT32(36482U, sizeof(jdi_buf));
    TEST_ASSERT_EQUAL_UINT32(152U, memlcd_frame_stride(&jdi));
    TEST_ASSERT_NULL(memlcd_frame_lead(&jdi));
    /* line 1: M0 = 1, AG9-AG8 = 0, AG7-AG0 = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x80U, memlcd_frame_line(&jdi, 0U)[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01U, memlcd_frame_line(&jdi, 0U)[1]);
    /* line 240: 0011110000 */
    TEST_ASSERT_EQUAL_HEX8(0x80U, memlcd_frame_line(&jdi, 239U)[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF0U, memlcd_frame_line(&jdi, 239U)[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFFU, memlcd_frame_pixels(&jdi, 239U)[149]);
    TEST_ASSERT_EQUAL_PTR(&jdi_buf[36480], tail);
    TEST_ASSERT_EQUAL_UINT32(2U, tail_len);
    TEST_ASSERT_EQUAL_HEX8(0x00U, tail[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00U, tail[1]);
}

static void test_the_picture_is_240_by_400_in_portrait(void)
{
    uint16_t w;
    uint16_t h;

    memlcd_frame_size(&sharp, &w, &h);
    TEST_ASSERT_EQUAL_UINT16(240U, w);
    TEST_ASSERT_EQUAL_UINT16(400U, h);
    sharp.rotation = 0U;
    memlcd_frame_size(&sharp, &w, &h);
    TEST_ASSERT_EQUAL_UINT16(400U, w);
    TEST_ASSERT_EQUAL_UINT16(240U, h);
}

static void test_one_black_pixel_changes_one_line(void)
{
    fill(0x0000U, 1U, 1U);
    TEST_ASSERT_EQUAL_INT(0, memlcd_frame_blit(&sharp, 0U, 0U, 1U, 1U, 1U, picture));
    TEST_ASSERT_EQUAL_UINT32(1U, dirty_lines(&sharp));
    TEST_ASSERT_TRUE(memlcd_frame_is_dirty(&sharp, 239U));
    TEST_ASSERT_EQUAL_HEX8(0xFEU, memlcd_frame_pixels(&sharp, 239U)[0]);

    TEST_ASSERT_EQUAL_INT(0, memlcd_frame_blit(&jdi, 0U, 0U, 1U, 1U, 1U, picture));
    TEST_ASSERT_EQUAL_UINT32(1U, dirty_lines(&jdi));
    TEST_ASSERT_EQUAL_HEX8(0x1FU, memlcd_frame_pixels(&jdi, 239U)[0]);
}

static void test_drawing_what_is_there_changes_nothing(void)
{
    fill(0xFFFFU, 240U, 400U);
    TEST_ASSERT_EQUAL_INT(0, memlcd_frame_blit(&jdi, 0U, 0U, 240U, 400U, 240U, picture));
    TEST_ASSERT_EQUAL_UINT32(0U, dirty_lines(&jdi));
}

static void test_a_full_black_picture_changes_every_line(void)
{
    fill(0x0000U, 240U, 400U);
    TEST_ASSERT_EQUAL_INT(0, memlcd_frame_blit(&sharp, 0U, 0U, 240U, 400U, 240U, picture));
    TEST_ASSERT_EQUAL_UINT32(240U, dirty_lines(&sharp));
    for (uint16_t l = 0U; l < 240U; l++) {
        for (uint32_t i = 0U; i < 50U; i++) {
            TEST_ASSERT_EQUAL_HEX8(0x00U, memlcd_frame_pixels(&sharp, l)[i]);
        }
        /* the address survives */
        TEST_ASSERT_EQUAL_UINT8(l + 1U, memlcd_frame_line(&sharp, l)[0]);
    }
}

static void black_at(uint32_t index)
{
    picture[2U * index] = 0U;
    picture[(2U * index) + 1U] = 0U;
}

static void test_the_pitch_skips_the_end_of_each_row(void)
{
    /* 2 x 2 area in a buffer 4 pixels wide: the second row starts at pixel 4 */
    fill(0xFFFFU, 4U, 2U);
    black_at(0U);               /* (0, 0) */
    black_at(2U);               /* (2, 0), out of the area: not the start of row 1 */
    black_at(5U);               /* (1, 1) */
    TEST_ASSERT_EQUAL_INT(0, memlcd_frame_blit(&sharp, 0U, 0U, 2U, 2U, 4U, picture));
    /* x = 0 is line 239: column 0 black, column 1 white */
    TEST_ASSERT_EQUAL_HEX8(0xFEU, memlcd_frame_pixels(&sharp, 239U)[0]);
    /* x = 1 is line 238: column 0 white, column 1 black */
    TEST_ASSERT_EQUAL_HEX8(0xFDU, memlcd_frame_pixels(&sharp, 238U)[0]);
    TEST_ASSERT_EQUAL_UINT32(2U, dirty_lines(&sharp));
}

static void test_an_area_out_of_the_picture_is_refused(void)
{
    fill(0x0000U, 2U, 2U);
    TEST_ASSERT_EQUAL_INT(-1, memlcd_frame_blit(&sharp, 239U, 0U, 2U, 1U, 2U, picture));
    TEST_ASSERT_EQUAL_INT(-1, memlcd_frame_blit(&sharp, 0U, 399U, 1U, 2U, 1U, picture));
    TEST_ASSERT_EQUAL_INT(-1, memlcd_frame_blit(&sharp, 0U, 0U, 2U, 1U, 1U, picture));
    TEST_ASSERT_EQUAL_INT(-1, memlcd_frame_blit(&sharp, 0U, 0U, 0U, 1U, 1U, picture));
    TEST_ASSERT_EQUAL_UINT32(0U, dirty_lines(&sharp));
}

static void test_changed_lines_go_out_in_runs(void)
{
    struct memlcd_run runs[4];

    fill(0x0000U, 1U, 400U);
    /* picture columns x = 236..234 and 229 and 39..0 are panel lines 3..5, 10, 200..239 */
    for (uint16_t x = 234U; x <= 236U; x++) {
        (void)memlcd_frame_blit(&sharp, x, 0U, 1U, 1U, 1U, picture);
    }
    (void)memlcd_frame_blit(&sharp, 229U, 0U, 1U, 1U, 1U, picture);
    for (uint16_t x = 0U; x < 40U; x++) {
        (void)memlcd_frame_blit(&sharp, x, 5U, 1U, 1U, 1U, picture);
    }
    TEST_ASSERT_EQUAL_UINT32(3U, memlcd_frame_runs(&sharp, runs, 4U));
    TEST_ASSERT_EQUAL_UINT16(3U, runs[0].first);
    TEST_ASSERT_EQUAL_UINT16(3U, runs[0].count);
    TEST_ASSERT_EQUAL_UINT16(10U, runs[1].first);
    TEST_ASSERT_EQUAL_UINT16(1U, runs[1].count);
    TEST_ASSERT_EQUAL_UINT16(200U, runs[2].first);
    TEST_ASSERT_EQUAL_UINT16(40U, runs[2].count);
}

static void test_too_many_runs_stretch_the_last_one(void)
{
    struct memlcd_run runs[2];

    fill(0x0000U, 1U, 1U);
    (void)memlcd_frame_blit(&sharp, 236U, 0U, 1U, 1U, 1U, picture);    /* line 3 */
    (void)memlcd_frame_blit(&sharp, 229U, 0U, 1U, 1U, 1U, picture);    /* line 10 */
    (void)memlcd_frame_blit(&sharp, 39U, 0U, 1U, 1U, 1U, picture);     /* line 200 */
    (void)memlcd_frame_blit(&sharp, 0U, 0U, 1U, 1U, 1U, picture);      /* line 239 */
    TEST_ASSERT_EQUAL_UINT32(2U, memlcd_frame_runs(&sharp, runs, 2U));
    TEST_ASSERT_EQUAL_UINT16(3U, runs[0].first);
    TEST_ASSERT_EQUAL_UINT16(1U, runs[0].count);
    /* lines 10 to 239: nothing changed is lost */
    TEST_ASSERT_EQUAL_UINT16(10U, runs[1].first);
    TEST_ASSERT_EQUAL_UINT16(230U, runs[1].count);
}

static void test_clean_and_white_forget_the_changes(void)
{
    struct memlcd_run runs[1];

    fill(0x0000U, 1U, 1U);
    (void)memlcd_frame_blit(&jdi, 0U, 0U, 1U, 1U, 1U, picture);
    memlcd_frame_clean(&jdi);
    TEST_ASSERT_EQUAL_UINT32(0U, memlcd_frame_runs(&jdi, runs, 1U));
    /* the pixel stays black until the all clear paints the frame white */
    TEST_ASSERT_EQUAL_HEX8(0x1FU, memlcd_frame_pixels(&jdi, 239U)[0]);
    (void)memlcd_frame_blit(&jdi, 0U, 1U, 1U, 1U, 1U, picture);
    memlcd_frame_white(&jdi);
    TEST_ASSERT_EQUAL_UINT32(0U, dirty_lines(&jdi));
    TEST_ASSERT_EQUAL_HEX8(0xFFU, memlcd_frame_pixels(&jdi, 239U)[0]);
    /* headers untouched */
    TEST_ASSERT_EQUAL_HEX8(0xF0U, memlcd_frame_line(&jdi, 239U)[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pure_colours_keep_the_top_bit_of_each_channel);
    RUN_TEST(test_grey_edges_go_black_or_white_by_their_mean);
    RUN_TEST(test_a_channel_48_apart_is_a_colour);
    RUN_TEST(test_the_sharp_shows_colours_by_their_luminance);
    RUN_TEST(test_portrait_is_the_legacy_rotation);
    RUN_TEST(test_the_other_rotations_cover_the_panel);
    RUN_TEST(test_sharp_pixel_zero_is_bit_zero);
    RUN_TEST(test_jdi_pixels_go_red_green_blue_from_the_top_bit);
    RUN_TEST(test_sharp_frame_is_the_legacy_12482_bytes);
    RUN_TEST(test_jdi_frame_has_the_10_bit_address_on_every_line);
    RUN_TEST(test_the_picture_is_240_by_400_in_portrait);
    RUN_TEST(test_one_black_pixel_changes_one_line);
    RUN_TEST(test_drawing_what_is_there_changes_nothing);
    RUN_TEST(test_a_full_black_picture_changes_every_line);
    RUN_TEST(test_the_pitch_skips_the_end_of_each_row);
    RUN_TEST(test_an_area_out_of_the_picture_is_refused);
    RUN_TEST(test_changed_lines_go_out_in_runs);
    RUN_TEST(test_too_many_runs_stretch_the_last_one);
    RUN_TEST(test_clean_and_white_forget_the_changes);
    return UNITY_END();
}
