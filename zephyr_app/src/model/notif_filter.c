/**
 * @file notif_filter.c
 * @brief Which of the phone's notifications are worth a rider's attention
 *
 * The rules, and the order they are applied in, are in
 * model/notif_filter.h.
 */

#include <string.h>

#include "model/notif_filter.h"

/** Categories a rider on a bicycle might want, and the ones they will not */
#define NOTIF_DEFAULT_CATEGORIES                                                \
    ((1U << NOTIF_CAT_INCOMING_CALL) | (1U << NOTIF_CAT_MISSED_CALL) |          \
     (1U << NOTIF_CAT_VOICE_MAIL) | (1U << NOTIF_CAT_SOCIAL) |                  \
     (1U << NOTIF_CAT_SCHEDULE))

void notif_filter_init(struct notif_filter *f)
{
    if (f == NULL) {
        return;
    }

    (void)memset(f, 0, sizeof(*f));
    f->categories = (uint16_t)NOTIF_DEFAULT_CATEGORIES;
    f->calls_always = true;
}

void notif_filter_reset(struct notif_filter *f)
{
    if (f == NULL) {
        return;
    }

    (void)memset(f->recent, 0, sizeof(f->recent));
    f->recent_head = 0U;
    f->recent_n = 0U;
    f->last_ms = 0U;
    f->have_last = false;
}

void notif_filter_set_category(struct notif_filter *f, enum notif_category cat, bool on)
{
    if ((f == NULL) || (cat >= NOTIF_CAT_COUNT)) {
        return;
    }

    if (on) {
        f->categories |= (uint16_t)(1U << cat);
    } else {
        f->categories &= (uint16_t)~(1U << cat);
    }
}

bool notif_filter_category(const struct notif_filter *f, enum notif_category cat)
{
    if ((f == NULL) || (cat >= NOTIF_CAT_COUNT)) {
        return false;
    }

    return (f->categories & (uint16_t)(1U << cat)) != 0U;
}

void notif_filter_set_only_calls(struct notif_filter *f, bool on)
{
    if (f != NULL) {
        f->only_calls = on;
    }
}

void notif_filter_set_calls_always(struct notif_filter *f, bool on)
{
    if (f != NULL) {
        f->calls_always = on;
    }
}

/** Has this identifier been through lately? */
static bool seen_lately(const struct notif_filter *f, uint32_t uid)
{
    for (uint8_t i = 0U; i < f->recent_n; i++) {
        if (f->recent[i] == uid) {
            return true;
        }
    }

    return false;
}

static void remember(struct notif_filter *f, uint32_t uid, uint32_t now_ms)
{
    f->recent[f->recent_head] = uid;
    f->recent_head = (uint8_t)((f->recent_head + 1U) % NOTIF_RECENT);
    if (f->recent_n < NOTIF_RECENT) {
        f->recent_n++;
    }
    f->last_ms = now_ms;
    f->have_last = true;
}

enum notif_action notif_filter_decide(struct notif_filter *f, const struct notif_event *e,
                                      uint32_t now_ms)
{
    if ((f == NULL) || (e == NULL)) {
        return NOTIF_DROP_CATEGORY;
    }

    if (e->event_id == (uint8_t)NOTIF_EV_REMOVED) {
        return NOTIF_DROP_REMOVED;
    }

    /*
     * The phone empties everything it already had onto a device that has
     * just connected. A morning of unread messages at the first pedal
     * stroke is how a rider learns to switch this off.
     */
    if (e->preexisting) {
        return NOTIF_DROP_PREEXISTING;
    }

    bool is_call = (e->category == (uint8_t)NOTIF_CAT_INCOMING_CALL);

    if (is_call && f->calls_always) {
        /* the one worth stopping for: no other rule applies to it */
        remember(f, e->uid, now_ms);

        return NOTIF_SHOW;
    }

    if (f->only_calls) {
        return NOTIF_DROP_ONLY_CALLS;
    }

    if (e->silent) {
        /* the phone already decided the rider did not want to be told */
        return NOTIF_DROP_SILENT;
    }

    if (!notif_filter_category(f, (enum notif_category)e->category)) {
        return NOTIF_DROP_CATEGORY;
    }

    if (seen_lately(f, e->uid)) {
        /* the same message, edited or delivered twice */
        return NOTIF_DROP_REPEAT;
    }

    if (!e->important && f->have_last && ((now_ms - f->last_ms) < NOTIF_MIN_GAP_MS)) {
        /* a group chat can make thirty of these a minute */
        return NOTIF_DROP_TOO_SOON;
    }

    remember(f, e->uid, now_ms);

    return NOTIF_SHOW;
}
