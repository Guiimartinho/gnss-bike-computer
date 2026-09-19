/**
 * @file memlcd_frame.c
 * @brief Frame buffer of the memory-in-pixel panels (memlcd_frame.h)
 */

#include <string.h>

#include "drivers/display/memlcd_frame.h"
#include "drivers/display/memlcd_pixel.h"

size_t memlcd_frame_stride(const struct memlcd_frame *f)
{
    return MEMLCD_STRIDE((uint32_t)f->kind, (uint32_t)f->width);
}

uint8_t *memlcd_frame_line(const struct memlcd_frame *f, uint16_t line)
{
    /* The Sharp mode byte comes before the first line */
    size_t lead = (f->kind == MEMLCD_SHARP) ? 1U : 0U;

    return &f->buf[lead + ((size_t)line * memlcd_frame_stride(f))];
}

uint8_t *memlcd_frame_pixels(const struct memlcd_frame *f, uint16_t line)
{
    /* Sharp: address before the pixels; JDI: two header bytes */
    return memlcd_frame_line(f, line) + ((f->kind == MEMLCD_SHARP) ? 1U : 2U);
}

uint8_t *memlcd_frame_lead(const struct memlcd_frame *f)
{
    return (f->kind == MEMLCD_SHARP) ? &f->buf[0] : NULL;
}

uint8_t *memlcd_frame_tail(const struct memlcd_frame *f, size_t *len)
{
    *len = (f->kind == MEMLCD_SHARP) ? 1U : 2U;
    return memlcd_frame_line(f, f->height);
}

bool memlcd_frame_is_dirty(const struct memlcd_frame *f, uint16_t line)
{
    return (f->dirty[line >> 5] & (1UL << (line & 31U))) != 0U;
}

static void mark_dirty(struct memlcd_frame *f, uint16_t line)
{
    f->dirty[line >> 5] |= (1UL << (line & 31U));
}

void memlcd_frame_clean(struct memlcd_frame *f)
{
    (void)memset(f->dirty, 0, MEMLCD_DIRTY_WORDS((uint32_t)f->height) * sizeof(f->dirty[0]));
}

void memlcd_frame_white(struct memlcd_frame *f)
{
    size_t n = MEMLCD_LINE_BYTES((uint32_t)f->kind, (uint32_t)f->width);

    /* 1 is white on both panels (JDI: red, green and blue on) */
    for (uint16_t line = 0U; line < f->height; line++) {
        (void)memset(memlcd_frame_pixels(f, line), 0xFF, n);
    }
    memlcd_frame_clean(f);
}

void memlcd_frame_init(struct memlcd_frame *f)
{
    size_t tail_len;
    uint8_t *tail;

    (void)memset(f->buf, 0, MEMLCD_FRAME_SIZE((uint32_t)f->kind, (uint32_t)f->width,
                                              (uint32_t)f->height));
    for (uint16_t line = 0U; line < f->height; line++) {
        uint8_t *w = memlcd_frame_line(f, line);
        uint32_t number = (uint32_t)line + 1U;  /* the panels count lines from 1 */

        if (f->kind == MEMLCD_SHARP) {
            /* address, pixels, dummy (already 0) */
            w[0] = (uint8_t)number;
        } else {
            /* M0-M5 (update, 3-bit data) and AG9-AG8, then AG7-AG0 */
            w[0] = (uint8_t)(MEMLCD_JDI_M0_UPDATE | ((number >> 8) & 0x03U));
            w[1] = (uint8_t)(number & 0xFFU);
        }
    }
    tail = memlcd_frame_tail(f, &tail_len);
    (void)memset(tail, 0, tail_len);
    memlcd_frame_white(f);
}

void memlcd_frame_size(const struct memlcd_frame *f, uint16_t *width, uint16_t *height)
{
    bool portrait = (f->rotation == 90U) || (f->rotation == 270U);

    *width = portrait ? f->height : f->width;
    *height = portrait ? f->width : f->height;
}

int memlcd_frame_blit(struct memlcd_frame *f, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t pitch, const uint8_t *src)
{
    uint16_t pw;
    uint16_t ph;

    memlcd_frame_size(f, &pw, &ph);
    if ((w == 0U) || (h == 0U) || (pitch < w) || (((uint32_t)x + w) > pw) ||
        (((uint32_t)y + h) > ph)) {
        return -1;
    }
    for (uint16_t j = 0U; j < h; j++) {
        const uint8_t *row = &src[(size_t)j * pitch * 2U];

        for (uint16_t i = 0U; i < w; i++) {
            uint16_t p = (uint16_t)((uint16_t)row[2U * i] | ((uint16_t)row[(2U * i) + 1U] << 8));
            uint16_t col;
            uint16_t line;
            bool changed;

            memlcd_map(f->rotation, f->width, f->height, (uint16_t)(x + i), (uint16_t)(y + j),
                       &col, &line);
            if (f->kind == MEMLCD_SHARP) {
                changed = memlcd_put_mono(memlcd_frame_pixels(f, line), col,
                                          memlcd_rgb565_to_mono(p));
            } else {
                changed = memlcd_put_rgb3(memlcd_frame_pixels(f, line), col,
                                          memlcd_rgb565_to_rgb3(p));
            }
            if (changed) {
                mark_dirty(f, line);
            }
        }
    }
    return 0;
}

size_t memlcd_frame_runs(const struct memlcd_frame *f, struct memlcd_run *runs, size_t max)
{
    size_t n = 0U;

    if (max == 0U) {
        return 0U;
    }
    for (uint16_t line = 0U; line < f->height; line++) {
        if (!memlcd_frame_is_dirty(f, line)) {
            continue;
        }
        if ((n > 0U) && (((uint32_t)runs[n - 1U].first + runs[n - 1U].count) == line)) {
            runs[n - 1U].count++;
        } else if (n < max) {
            runs[n].first = line;
            runs[n].count = 1U;
            n++;
        } else {
            /* No room left: stretch the last run over the gap */
            runs[n - 1U].count = (uint16_t)((line - runs[n - 1U].first) + 1U);
        }
    }
    return n;
}
