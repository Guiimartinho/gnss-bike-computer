/**
 * @file gfxfont.h
 * @brief GFX Font structures for custom font rendering
 *
 * Compatible with Adafruit GFX font format.
 * Follows MISRA C:2012 guidelines.
 */

#ifndef VUE_GFXFONT_H
#define VUE_GFXFONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * @brief Glyph descriptor structure
 *
 * Describes a single character glyph in a custom font.
 */
typedef struct {
    uint16_t bitmap_offset;  /**< Offset into font bitmap array */
    uint8_t  width;          /**< Glyph bitmap width in pixels */
    uint8_t  height;         /**< Glyph bitmap height in pixels */
    uint8_t  x_advance;      /**< Distance to advance cursor (x axis) */
    int8_t   x_offset;       /**< X offset from cursor position */
    int8_t   y_offset;       /**< Y offset from cursor position (baseline relative) */
} gfx_glyph_t;

/**
 * @brief Font descriptor structure
 *
 * Describes a complete font with all glyphs.
 */
typedef struct {
    const uint8_t    *bitmap;    /**< Glyph bitmaps, concatenated */
    const gfx_glyph_t *glyph;    /**< Glyph array */
    uint8_t           first;     /**< First ASCII character code */
    uint8_t           last;      /**< Last ASCII character code */
    uint8_t           y_advance; /**< Newline distance (y axis) */
} gfx_font_t;

/* ==========================================================================
 * Font Size Constants
 * ========================================================================== */

/** Small font: 5x7 built-in */
#define FONT_SIZE_SMALL     1U

/** Medium font: 2x scaled (10x14 effective) */
#define FONT_SIZE_MEDIUM    2U

/** Large font: 3x scaled (15x21 effective) */
#define FONT_SIZE_LARGE     3U

/** Extra large font: 4x scaled (20x28 effective) */
#define FONT_SIZE_XLARGE    4U

#ifdef __cplusplus
}
#endif

#endif /* VUE_GFXFONT_H */
