/**
 * @file radar.h
 * @brief Vehicles coming from behind, as a rear radar reports them
 *
 * The legacy has no radar: it came out before the Varia existed, and
 * `docs/17-dispositivos-ble-ant.md` lists the profile as not done. This is
 * the model of it — the list of what is behind, how far, how fast it is
 * closing and how much of a threat it is — kept apart from the two radios
 * that can feed it:
 *
 * - **BLE**, where the Garmin Varia exposes a service of its own, and
 * - **ANT+**, where the Bike Radar profile (device type 40) is the open
 *   one that every radar on the market speaks.
 *
 * Only this file and `radar.c` are pure and tested on the PC; the wire
 * formats live with each radio.
 *
 * What the rider sees comes from here: the nearest threat, how many there
 * are, and a strip down the side of the screen with one mark per vehicle.
 * A target that stops being reported fades out over `RADAR_HOLD_MS` rather
 * than vanishing, because a radar drops a frame now and then and a mark
 * that blinks is worse than one that lingers.
 */

#ifndef MODEL_RADAR_H
#define MODEL_RADAR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Vehicles the model keeps; the profiles report at most eight */
#define RADAR_TARGETS_MAX   8U

/** How long a target that stopped being reported stays on the screen */
#define RADAR_HOLD_MS       4000U

/** After this long without a word, a target is drawn as fading */
#define RADAR_FADE_MS       1000U

/** Range a radar reaches, in metres, past which a reading is dropped */
#define RADAR_RANGE_MAX_M   160U

/** Closing speed above which a vehicle counts as approaching fast, km/h */
#define RADAR_FAST_KMH      50U

/** How much of a threat one vehicle is (ANT+ Bike Radar, threat level) */
enum radar_level {
    RADAR_LEVEL_NONE = 0,       /**< nothing there */
    RADAR_LEVEL_APPROACHING,    /**< coming, no hurry */
    RADAR_LEVEL_FAST,           /**< closing fast */
    RADAR_LEVEL_DANGER          /**< the profile calls it a threat */
};

/** Which side of the road it is on */
enum radar_side {
    RADAR_SIDE_UNKNOWN = 0,
    RADAR_SIDE_RIGHT,
    RADAR_SIDE_LEFT
};

/** One vehicle behind */
struct radar_target {
    uint16_t range_m;           /**< distance behind the rider */
    uint16_t closing_kmh;       /**< speed it is closing at */
    uint32_t seen_ms;           /**< uptime of the last report */
    uint8_t level;              /**< enum radar_level */
    uint8_t side;               /**< enum radar_side */
    uint8_t id;                 /**< the identifier the radar gave it */
    bool live;                  /**< reported within RADAR_FADE_MS; fading otherwise */
};

/** One frame from a radar: everything it sees at one instant */
struct radar_frame {
    struct radar_target t[RADAR_TARGETS_MAX];
    uint8_t n;
};

/** What the model holds */
struct radar {
    struct radar_target t[RADAR_TARGETS_MAX];
    uint8_t n;                  /**< targets held, live or fading */
    uint32_t last_ms;           /**< uptime of the last frame */
    bool linked;                /**< a radar is connected */
};

/** Start with nothing behind */
void radar_init(struct radar *r);

/**
 * Take one frame.
 *
 * Targets are matched by the identifier the radar gives them, so one that
 * keeps being reported keeps its slot. Anything out of range or with no
 * distance is dropped. Targets are kept ordered by distance, nearest
 * first, which is the order the screen draws them in.
 *
 * @param now_ms uptime, for the ageing
 */
void radar_feed(struct radar *r, const struct radar_frame *f, uint32_t now_ms);

/**
 * Age the list: drop what has not been reported for RADAR_HOLD_MS.
 *
 * Called every epoch, whether a frame came or not.
 */
void radar_tick(struct radar *r, uint32_t now_ms);

/** The radio says the radar connected or went away */
void radar_set_link(struct radar *r, bool linked, uint32_t now_ms);

/** The nearest vehicle, or NULL when there is none */
const struct radar_target *radar_nearest(const struct radar *r);

/** Vehicles held, live and fading */
uint8_t radar_count(const struct radar *r);

/** The worst threat level of everything behind */
uint8_t radar_worst(const struct radar *r);

/**
 * Threat level from the distance and the closing speed, for a radio whose
 * frames do not carry one (the BLE service of the Varia does not).
 */
uint8_t radar_level_of(uint16_t range_m, uint16_t closing_kmh);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_RADAR_H */
