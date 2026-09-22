/**
 * @file komoot_turn.c
 * @brief The twenty-four directions of Komoot, in the nine the screen draws
 *
 * The table and the judgements behind it are in model/komoot_turn.h.
 */

#include "app/app_events.h"
#include "model/komoot_turn.h"

/*
 * The numbers are `komoot_direction_t` of rf/ble_komoot_client.h, written
 * out rather than included so that this module, and its test, stay clear
 * of Bluetooth.
 */
#define K_NONE              0U
#define K_STRAIGHT          1U
#define K_START             2U
#define K_FINISH            3U
#define K_SLIGHT_LEFT       4U
#define K_LEFT              5U
#define K_SHARP_LEFT        6U
#define K_SLIGHT_RIGHT      7U
#define K_RIGHT             8U
#define K_SHARP_RIGHT       9U
#define K_FORK_LEFT         10U
#define K_FORK_RIGHT        11U
#define K_U_TURN            12U
#define K_U_TURN_LEFT       13U
#define K_U_TURN_RIGHT      14U
#define K_RB_EXIT1          15U
#define K_RB_EXIT2          16U
#define K_RB_EXIT3          17U
#define K_RB_CCW1           18U
#define K_RB_CCW2           19U
#define K_RB_CCW3           20U
#define K_RB_FALLBACK       21U
#define K_OUT_OF_ROUTE      22U
#define K_FERRY             23U

uint8_t komoot_turn_of(uint8_t direction)
{
    switch (direction) {
    case K_STRAIGHT:
    case K_START:
        return (uint8_t)APP_TURN_STRAIGHT;

    case K_SLIGHT_LEFT:
    /* a fork is a choice of lane and not a corner: the slight arrow */
    case K_FORK_LEFT:
        return (uint8_t)APP_TURN_SLIGHT_LEFT;

    case K_LEFT:
        return (uint8_t)APP_TURN_LEFT;

    case K_SHARP_LEFT:
        return (uint8_t)APP_TURN_SHARP_LEFT;

    case K_SLIGHT_RIGHT:
    case K_FORK_RIGHT:
        return (uint8_t)APP_TURN_SLIGHT_RIGHT;

    case K_RIGHT:
        return (uint8_t)APP_TURN_RIGHT;

    case K_SHARP_RIGHT:
        return (uint8_t)APP_TURN_SHARP_RIGHT;

    /* the three u-turns are one arrow: the rider turns round either way */
    case K_U_TURN:
    case K_U_TURN_LEFT:
    case K_U_TURN_RIGHT:
        return (uint8_t)APP_TURN_UTURN;

    /*
     * A roundabout gets the plain arrow of the side it is taken on; which
     * exit goes in the text beside it. The counter-clockwise ones are the
     * left-hand-traffic case.
     */
    case K_RB_CCW1:
    case K_RB_CCW2:
    case K_RB_CCW3:
        return (uint8_t)APP_TURN_LEFT;

    case K_RB_EXIT1:
    case K_RB_EXIT2:
    case K_RB_EXIT3:
    /*
     * The fallback is a roundabout Komoot could not name an exit for. It
     * still gets an arrow, because the rider is at a roundabout and needs
     * to know; right is what a rider does where traffic keeps right.
     */
    case K_RB_FALLBACK:
        return (uint8_t)APP_TURN_RIGHT;

    case K_FINISH:
        return (uint8_t)APP_TURN_ARRIVE;

    /*
     * Out of route and the ferry are messages, not directions. An arrow
     * for either would point the rider somewhere.
     */
    case K_OUT_OF_ROUTE:
    case K_FERRY:
    case K_NONE:
    default:
        return (uint8_t)APP_TURN_NONE;
    }
}

bool komoot_turn_is_navigation(uint8_t direction)
{
    return (direction != K_NONE) && (direction != K_OUT_OF_ROUTE) &&
           (direction != K_FERRY) && (direction < 24U);
}

uint16_t komoot_turn_distance(uint32_t metres)
{
    return (metres > KOMOOT_DIST_MAX) ? (uint16_t)KOMOOT_DIST_MAX : (uint16_t)metres;
}
