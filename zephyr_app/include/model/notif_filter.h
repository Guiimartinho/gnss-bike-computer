/**
 * @file notif_filter.h
 * @brief Which of the phone's notifications are worth a rider's attention
 *
 * A phone has a great many notifications and a bicycle has one small
 * screen and a rider with their hands on the bars. The Apple Notification
 * Center Service will hand over every one of them, so the interesting part
 * of showing phone notifications is not fetching them — the NCS client
 * does that — but **deciding which ones get through**. That decision is
 * here, apart from Bluetooth, so the host tests can read it back.
 *
 * The rules, in the order they are applied:
 *
 * 1. a notification the phone marks **pre-existing** is dropped. Without
 *    this, getting on the bicycle empties a morning of unread messages
 *    onto the screen in one go, which is the single fastest way to make a
 *    rider turn the feature off;
 * 2. a **removed** event takes nothing to the screen;
 * 3. an **incoming call** gets through whatever else is set, because it is
 *    the one a rider may actually want to stop for. The rider can turn
 *    that off too, but it is on by default;
 * 4. with "only calls" on, nothing else gets through: the switch for a
 *    hard session or a race;
 * 5. a notification the phone itself marks **silent** is dropped: the
 *    phone already knows the rider did not want to be told;
 * 6. a category the rider turned off is dropped;
 * 7. a notification whose identifier was seen in the last few is dropped,
 *    because a message being edited or delivered twice arrives again with
 *    the same one;
 * 8. anything within `NOTIF_MIN_GAP_MS` of the last one shown is dropped,
 *    unless the phone marked it **important**. A group chat can produce
 *    thirty notifications a minute and a rider needs none of them.
 *
 * The category numbers are the ones of the service, which the NCS client
 * hands over as `enum bt_ancs_category_id_val`; they are repeated here so
 * that this module, and its tests, do not have to include Bluetooth.
 */

#ifndef MODEL_NOTIF_FILTER_H
#define MODEL_NOTIF_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Categories of the Apple Notification Center Service */
enum notif_category {
    NOTIF_CAT_OTHER = 0,
    NOTIF_CAT_INCOMING_CALL,
    NOTIF_CAT_MISSED_CALL,
    NOTIF_CAT_VOICE_MAIL,
    NOTIF_CAT_SOCIAL,
    NOTIF_CAT_SCHEDULE,
    NOTIF_CAT_EMAIL,
    NOTIF_CAT_NEWS,
    NOTIF_CAT_HEALTH,
    NOTIF_CAT_BUSINESS,
    NOTIF_CAT_LOCATION,
    NOTIF_CAT_ENTERTAINMENT,
    NOTIF_CAT_COUNT
};

/** What the phone said happened */
enum notif_event_id {
    NOTIF_EV_ADDED = 0,
    NOTIF_EV_MODIFIED,
    NOTIF_EV_REMOVED
};

/** Why a notification did or did not reach the screen */
enum notif_action {
    NOTIF_SHOW = 0,
    NOTIF_DROP_REMOVED,
    NOTIF_DROP_PREEXISTING,
    NOTIF_DROP_ONLY_CALLS,
    NOTIF_DROP_SILENT,
    NOTIF_DROP_CATEGORY,
    NOTIF_DROP_REPEAT,
    NOTIF_DROP_TOO_SOON
};

/** Shortest gap between two notifications on the screen */
#define NOTIF_MIN_GAP_MS    20000U

/** How many identifiers are remembered, to catch one arriving twice */
#define NOTIF_RECENT        8U

/** One notification, as the phone describes it */
struct notif_event {
    uint32_t uid;           /**< the phone's identifier for it */
    uint8_t category;       /**< enum notif_category */
    uint8_t event_id;       /**< enum notif_event_id */
    bool silent;
    bool important;
    bool preexisting;       /**< it was already on the phone when we connected */
};

/** What the rider chose, and what has been shown */
struct notif_filter {
    uint16_t categories;    /**< bit per enum notif_category; 1 lets it through */
    bool calls_always;      /**< an incoming call ignores every other rule */
    bool only_calls;        /**< nothing but calls gets through */
    uint32_t recent[NOTIF_RECENT];
    uint8_t recent_head;
    /**
     * How many slots hold an identifier. Without it an empty ring of
     * zeros matches a notification whose identifier really is zero, and
     * the first message of a ride disappears; nothing in the service
     * stops a phone from using zero.
     */
    uint8_t recent_n;
    uint32_t last_ms;       /**< when one last reached the screen */
    bool have_last;
};

/**
 * @brief Start with a sensible choice
 *
 * Calls, missed calls, voice mail, messages and the calendar get through;
 * news, health, business, location and entertainment do not, because a
 * bicycle is not where anyone reads them.
 */
void notif_filter_init(struct notif_filter *f);

/** Let a category through, or stop it */
void notif_filter_set_category(struct notif_filter *f, enum notif_category cat, bool on);

/** Whether a category is let through */
bool notif_filter_category(const struct notif_filter *f, enum notif_category cat);

/** Nothing but incoming calls, for a hard session */
void notif_filter_set_only_calls(struct notif_filter *f, bool on);

/** Whether an incoming call ignores the other rules */
void notif_filter_set_calls_always(struct notif_filter *f, bool on);

/**
 * @brief Decide one notification, and remember it if it gets through
 * @return NOTIF_SHOW, or why not
 */
enum notif_action notif_filter_decide(struct notif_filter *f, const struct notif_event *e,
                                      uint32_t now_ms);

/** The phone went away: forget what has been shown */
void notif_filter_reset(struct notif_filter *f);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_NOTIF_FILTER_H */
