/**
 * @file memlcd_pixel.h
 * @brief Pixel rules of the memory-in-pixel panels, shared by the driver and
 *        the host renderer of the interface (zephyr_app/tests/ui)
 *
 * Plain C, no Zephyr: the same rules draw the panel in the firmware and the
 * PNG pictures of docs/telas.
 *
 * Quantisation of an RGB565 pixel, expanded to 8 bits per channel
 * (docs/18-interface-telas.md, Implementação):
 *  1. a grey pixel (largest minus smallest channel below 48), the
 *     anti-aliased edge of black on white, goes white when the mean of the
 *     channels is 128 or more, black otherwise;
 *  2. a coloured pixel keeps the top bit of each channel: the 8 colours of
 *     the JDI.
 * Without rule 1, the rounding of RGB565 (6 bits of green, 5 of red and
 * blue) leaves green specks on the edges.
 */

#ifndef MEMLCD_PIXEL_H
#define MEMLCD_PIXEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Colour index on the panel: bit 2 red, bit 1 green, bit 0 blue (7 white) */
static inline uint8_t memlcd_rgb565_to_rgb3(uint16_t p)
{
    uint32_t r = ((uint32_t)(p >> 11) & 0x1FU) << 3;
    uint32_t g = ((uint32_t)(p >> 5) & 0x3FU) << 2;
    uint32_t b = ((uint32_t)p & 0x1FU) << 3;
    uint32_t hi = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    uint32_t lo = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);

    if ((hi - lo) < 48U) {
        return (((r + g + b) / 3U) >= 128U) ? 7U : 0U;
    }
    return (uint8_t)(((r >= 128U) ? 4U : 0U) | ((g >= 128U) ? 2U : 0U) | ((b >= 128U) ? 1U : 0U));
}

/**
 * White (1) or black (0) on the 1-bit panel. White and black of the rule
 * above stay; a coloured pixel (the colour theme drawn on the Sharp) goes by
 * its luminance.
 */
static inline uint8_t memlcd_rgb565_to_mono(uint16_t p)
{
    uint8_t c = memlcd_rgb565_to_rgb3(p);

    if (c == 7U) {
        return 1U;
    }
    if (c == 0U) {
        return 0U;
    }
    uint32_t r = ((uint32_t)(p >> 11) & 0x1FU) << 3;
    uint32_t g = ((uint32_t)(p >> 5) & 0x3FU) << 2;
    uint32_t b = ((uint32_t)p & 0x1FU) << 3;

    return ((((r * 299U) + (g * 587U) + (b * 114U)) / 1000U) >= 128U) ? 1U : 0U;
}

/**
 * Panel position of a point of the picture
 *
 * @param rotation 0, 90, 180 or 270 (90: portrait of the V3, legacy
 *                 setRotation(3): (x, y) goes to column y of line 239 - x)
 * @param width Pixels per panel line (400)
 * @param height Panel lines (240)
 */
static inline void memlcd_map(uint16_t rotation, uint16_t width, uint16_t height, uint16_t x,
                              uint16_t y, uint16_t *col, uint16_t *line)
{
    switch (rotation) {
    case 90U:
        *col = y;
        *line = (uint16_t)((height - 1U) - x);
        break;
    case 180U:
        *col = (uint16_t)((width - 1U) - x);
        *line = (uint16_t)((height - 1U) - y);
        break;
    case 270U:
        *col = (uint16_t)((width - 1U) - y);
        *line = x;
        break;
    default:
        *col = x;
        *line = y;
        break;
    }
}

/**
 * Set one pixel of a Sharp line: 1 bit, least significant bit first
 * (pixel 0 is bit 0 of byte 0), 1 white
 *
 * @return true if the line changed
 */
static inline bool memlcd_put_mono(uint8_t *line, uint16_t col, uint8_t white)
{
    uint8_t *byte = &line[col >> 3];
    uint8_t mask = (uint8_t)(1U << (col & 7U));
    uint8_t old = *byte;

    *byte = (white != 0U) ? (uint8_t)(old | mask) : (uint8_t)(old & (uint8_t)~mask);
    return *byte != old;
}

/**
 * Set one pixel of a JDI line: 3 bits in red, green, blue order, most
 * significant bit first (pixel 0 is bits 7, 6 and 5 of byte 0)
 *
 * @return true if the line changed
 */
static inline bool memlcd_put_rgb3(uint8_t *line, uint16_t col, uint8_t rgb)
{
    uint32_t bit = (uint32_t)col * 3U;
    bool changed = false;

    for (int32_t i = 2; i >= 0; i--) {
        uint8_t *byte = &line[bit >> 3];
        uint8_t mask = (uint8_t)(0x80U >> (bit & 7U));
        uint8_t old = *byte;

        *byte = (((rgb >> (uint32_t)i) & 1U) != 0U) ? (uint8_t)(old | mask)
                                                    : (uint8_t)(old & (uint8_t)~mask);
        changed = changed || (*byte != old);
        bit++;
    }
    return changed;
}

#ifdef __cplusplus
}
#endif

#endif /* MEMLCD_PIXEL_H */
