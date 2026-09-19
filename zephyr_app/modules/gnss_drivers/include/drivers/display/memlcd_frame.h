/**
 * @file memlcd_frame.h
 * @brief Frame buffer of the memory-in-pixel panels, kept as it goes on the wire
 *
 * Plain C, no Zephyr, so that the host tests check it. Each panel line sits
 * in memory with its address bytes around the pixels, the way the panel
 * reads it, so that a run of consecutive lines goes out in one SPI buffer
 * (the legacy kept the same 12,482-byte layout for the Sharp):
 *
 * | Panel | Layout | Line | Size (400 x 240) |
 * |---|---|---|---|
 * | Sharp LS027B7DH01 | mode byte, 240 lines, 1 dummy byte | address, 50 bytes, dummy | 12,482 B |
 * | JDI LPM027M128B/C | 240 lines, 2 dummy bytes | M0-M5 and AG9-AG8, AG7-AG0, 150 bytes | 36,482 B |
 *
 * The JDI reads the 6 mode bits only on the first line of a transfer; on the
 * other lines the same 6 clocks are a transfer period whose bits it ignores
 * (LPM027M128B specification, 6.2), so every line keeps the same header.
 */

#ifndef MEMLCD_FRAME_H
#define MEMLCD_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Panel kinds */
#define MEMLCD_SHARP 0U
#define MEMLCD_JDI   1U

/** Pixel bytes of a panel line: 1 bit per pixel (Sharp) or 3 (JDI) */
#define MEMLCD_LINE_BYTES(kind, width) \
    (((kind) == MEMLCD_JDI) ? (((width) * 3U) / 8U) : ((width) / 8U))
/** A line on the wire: its pixels and 2 bytes (Sharp: address and dummy; JDI: header) */
#define MEMLCD_STRIDE(kind, width) (MEMLCD_LINE_BYTES(kind, width) + 2U)
/** Whole frame: the lines and 2 bytes (Sharp: mode byte and last dummy; JDI: 16 clocks) */
#define MEMLCD_FRAME_SIZE(kind, width, height) ((MEMLCD_STRIDE(kind, width) * (height)) + 2U)
/** Words of the changed-line bitmap */
#define MEMLCD_DIRTY_WORDS(height) (((height) + 31U) / 32U)

/** JDI header, first byte, sent most significant bit first: M0 (update), then AG9-AG8 */
#define MEMLCD_JDI_M0_UPDATE 0x80U
/** JDI M1: COM level in serial mode (EXTMODE low) */
#define MEMLCD_JDI_M1_COM    0x40U
/** JDI M2: all clear */
#define MEMLCD_JDI_M2_CLEAR  0x20U
/** Sharp mode byte, sent least significant bit first: M0 (update) */
#define MEMLCD_SHARP_M0_UPDATE 0x01U
/** Sharp M1: VCOM level in serial mode (EXTMODE low) */
#define MEMLCD_SHARP_M1_VCOM   0x02U
/** Sharp M2: all clear */
#define MEMLCD_SHARP_M2_CLEAR  0x04U

struct memlcd_frame {
    uint8_t *buf;       /**< MEMLCD_FRAME_SIZE() bytes */
    uint32_t *dirty;    /**< MEMLCD_DIRTY_WORDS() words, one bit per panel line */
    uint16_t width;     /**< pixels per panel line (400), a multiple of 8 */
    uint16_t height;    /**< panel lines (240) */
    uint16_t rotation;  /**< 0, 90, 180 or 270 (memlcd_map()) */
    uint8_t kind;       /**< MEMLCD_SHARP or MEMLCD_JDI */
};

/** Consecutive changed lines, sent in one SPI buffer */
struct memlcd_run {
    uint16_t first;     /**< first panel line, from 0 */
    uint16_t count;
};

/** Fill the addresses and dummies, paint the frame white (as after an all clear), nothing changed */
void memlcd_frame_init(struct memlcd_frame *f);

/** Paint the frame white and forget the changes: the panel memory after an all clear */
void memlcd_frame_white(struct memlcd_frame *f);

/** Size of the picture the application draws, after the rotation (240 x 400 in portrait) */
void memlcd_frame_size(const struct memlcd_frame *f, uint16_t *width, uint16_t *height);

/**
 * @brief Copy an area of RGB565 pixels into the frame, quantised to the panel
 *
 * @param x, y  top left corner in the picture
 * @param w, h  area size in pixels
 * @param pitch pixels between the starts of two rows of @p src
 * @param src   RGB565, least significant byte first (PIXEL_FORMAT_RGB_565)
 * @return 0, or -1 when the area leaves the picture
 */
int memlcd_frame_blit(struct memlcd_frame *f, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t pitch, const uint8_t *src);

/**
 * @brief Changed lines, grouped in runs
 *
 * When there are more runs than @p max, the last one grows over the gap to
 * the next changed line: unchanged lines are sent again, which the panel
 * accepts, and nothing changed is lost.
 *
 * @return number of runs written to @p runs (0: nothing changed)
 */
size_t memlcd_frame_runs(const struct memlcd_frame *f, struct memlcd_run *runs, size_t max);

/** Forget the changes, after they reached the panel */
void memlcd_frame_clean(struct memlcd_frame *f);

/** Bytes of a line on the wire */
size_t memlcd_frame_stride(const struct memlcd_frame *f);

/** A line on the wire, from its first address byte */
uint8_t *memlcd_frame_line(const struct memlcd_frame *f, uint16_t line);

/** Pixels of a line */
uint8_t *memlcd_frame_pixels(const struct memlcd_frame *f, uint16_t line);

/** Mode byte of the Sharp, before the first line (NULL on the JDI) */
uint8_t *memlcd_frame_lead(const struct memlcd_frame *f);

/** Dummy clocks after the last line: 1 byte on the Sharp, 2 on the JDI */
uint8_t *memlcd_frame_tail(const struct memlcd_frame *f, size_t *len);

/** Whether a panel line changed since the last memlcd_frame_clean() */
bool memlcd_frame_is_dirty(const struct memlcd_frame *f, uint16_t line);

#ifdef __cplusplus
}
#endif

#endif /* MEMLCD_FRAME_H */
