/**
 * @file komoot_turn.h
 * @brief The twenty-four directions of Komoot, in the nine the screen draws
 *
 * The Komoot application names twenty-four kinds of turn and this device
 * draws nine arrows. Something has to decide which arrow each name gets,
 * and the decisions are not all obvious: a fork is not a turn, a roundabout
 * exit is not a turn either, and "out of route" and "take the ferry" are
 * not directions at all.
 *
 * It is a table and a few judgements, which is exactly the kind of thing
 * that rots quietly when it lives inside a Bluetooth client. Here it is a
 * function with a test.
 *
 * | Komoot says | The screen draws | Why |
 * |---|---|---|
 * | straight, start | straight | nothing to turn |
 * | slight left/right | the slight arrow | |
 * | left/right | the plain arrow | |
 * | sharp left/right | the sharp arrow | |
 * | fork left/right | the **slight** arrow | a fork is a lane choice, not a corner |
 * | any u-turn | the u-turn arrow | left, right and plain are one arrow here |
 * | any roundabout | the **plain** arrow of its side | the exit number goes in the text, and a roundabout with no side named turns right, which is what a rider does in a country that drives on the right |
 * | finish | arrive | |
 * | out of route, ferry, none | nothing | neither is a direction, and an arrow for them would be a lie |
 *
 * The **distance** is clamped: the screen has four digits and Komoot can
 * send a turn thirty kilometres away at the start of a route.
 */

#ifndef MODEL_KOMOOT_TURN_H
#define MODEL_KOMOOT_TURN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Farthest turn the screen can show, in metres */
#define KOMOOT_DIST_MAX     9999U

/**
 * @brief The arrow for one direction of Komoot
 *
 * @param direction `komoot_direction_t`, as the application sends it
 * @return `enum app_turn`; APP_TURN_NONE for anything that is not a
 * direction, including a value this firmware has never heard of
 */
uint8_t komoot_turn_of(uint8_t direction);

/**
 * @brief Whether a direction should put anything on the screen at all
 *
 * "Out of route" and "take the ferry" are messages and not turns: drawing
 * an arrow for them would point the rider somewhere.
 */
bool komoot_turn_is_navigation(uint8_t direction);

/** The distance, clamped to what the screen can hold */
uint16_t komoot_turn_distance(uint32_t metres);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_KOMOOT_TURN_H */
