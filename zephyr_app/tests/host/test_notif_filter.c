/**
 * @file test_notif_filter.c
 * @brief Which phone notifications reach the rider (src/model/notif_filter.c)
 *
 * Fetching a notification is the easy half and the NCS client does it. The
 * half that decides whether the feature is usable is this one, and every
 * rule below is there because of a way the feature becomes unusable:
 *
 * - the phone hands over everything it already had the moment the device
 *   connects, so a morning of unread messages lands at the first pedal
 *   stroke;
 * - a group chat makes thirty notifications a minute;
 * - a message being edited arrives again with the same identifier;
 * - and through all of that, an incoming call still has to get through,
 *   because it is the one a rider may want to stop for.
 *
 * There is no legacy for any of this; the rules are the ones written out
 * in `model/notif_filter.h`.
 */

#include <string.h>

#include "unity.h"

#include "model/notif_filter.h"

static struct notif_filter f;

void setUp(void)
{
    notif_filter_init(&f);
}

void tearDown(void) {}

/** One notification as the phone would describe it */
static struct notif_event ev(uint32_t uid, enum notif_category cat)
{
    struct notif_event e = {0};

    e.uid = uid;
    e.category = (uint8_t)cat;
    e.event_id = (uint8_t)NOTIF_EV_ADDED;

    return e;
}

/* ==========================================================================
 * What gets through by default
 * ========================================================================== */

static void test_a_message_gets_through(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1000U));
}

static void test_the_categories_a_rider_on_a_bicycle_wants(void)
{
    TEST_ASSERT_TRUE(notif_filter_category(&f, NOTIF_CAT_INCOMING_CALL));
    TEST_ASSERT_TRUE(notif_filter_category(&f, NOTIF_CAT_MISSED_CALL));
    TEST_ASSERT_TRUE(notif_filter_category(&f, NOTIF_CAT_VOICE_MAIL));
    TEST_ASSERT_TRUE(notif_filter_category(&f, NOTIF_CAT_SOCIAL));
    TEST_ASSERT_TRUE(notif_filter_category(&f, NOTIF_CAT_SCHEDULE));

    /* and the ones nobody reads on a bicycle */
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_NEWS));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_EMAIL));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_HEALTH));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_BUSINESS));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_LOCATION));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_ENTERTAINMENT));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_OTHER));
}

static void test_a_category_the_rider_turned_off(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_NEWS);

    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(&f, &e, 1000U));

    notif_filter_set_category(&f, NOTIF_CAT_NEWS, true);
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 2000U));
}

static void test_a_category_can_be_turned_off_again(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    notif_filter_set_category(&f, NOTIF_CAT_SOCIAL, false);
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(&f, &e, 1000U));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_SOCIAL));
}

/* ==========================================================================
 * The flood at connection time
 * ========================================================================== */

static void test_everything_the_phone_already_had_is_dropped(void)
{
    /*
     * The rule the whole feature depends on. Forty unread messages at the
     * moment of connecting, and the rider sees none of them.
     */
    for (uint32_t i = 0U; i < 40U; i++) {
        struct notif_event e = ev(i, NOTIF_CAT_SOCIAL);

        e.preexisting = true;
        TEST_ASSERT_EQUAL_INT(NOTIF_DROP_PREEXISTING, notif_filter_decide(&f, &e, 1000U + i));
    }

    /* and a new one straight afterwards still gets through */
    struct notif_event fresh = ev(100U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &fresh, 2000U));
}

static void test_even_a_call_the_phone_already_had_is_dropped(void)
{
    /* an incoming call from before the connection is not ringing now */
    struct notif_event e = ev(1U, NOTIF_CAT_INCOMING_CALL);

    e.preexisting = true;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_PREEXISTING, notif_filter_decide(&f, &e, 1000U));
}

static void test_a_notification_being_taken_away_shows_nothing(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    e.event_id = (uint8_t)NOTIF_EV_REMOVED;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_REMOVED, notif_filter_decide(&f, &e, 1000U));
}

/* ==========================================================================
 * The call that always gets through
 * ========================================================================== */

static void test_a_call_ignores_every_other_rule(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_INCOMING_CALL);

    /* silent, and right after another notification, and with calls off as
     * a category: it still rings */
    notif_filter_set_category(&f, NOTIF_CAT_INCOMING_CALL, false);
    e.silent = true;

    struct notif_event other = ev(2U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &other, 1000U));
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1100U));
}

static void test_a_call_can_be_made_to_follow_the_rules(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_INCOMING_CALL);

    notif_filter_set_calls_always(&f, false);
    notif_filter_set_category(&f, NOTIF_CAT_INCOMING_CALL, false);

    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(&f, &e, 1000U));
}

static void test_only_calls_stops_everything_else(void)
{
    /* the switch for a hard session or a race */
    notif_filter_set_only_calls(&f, true);

    struct notif_event msg = ev(1U, NOTIF_CAT_SOCIAL);
    struct notif_event cal = ev(2U, NOTIF_CAT_SCHEDULE);
    struct notif_event call = ev(3U, NOTIF_CAT_INCOMING_CALL);

    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_ONLY_CALLS, notif_filter_decide(&f, &msg, 1000U));
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_ONLY_CALLS, notif_filter_decide(&f, &cal, 2000U));
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &call, 3000U));

    notif_filter_set_only_calls(&f, false);
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &msg, 100000U));
}

static void test_a_missed_call_is_not_an_incoming_one(void)
{
    /* it gets through by category, but it obeys the other rules */
    notif_filter_set_only_calls(&f, true);

    struct notif_event e = ev(1U, NOTIF_CAT_MISSED_CALL);

    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_ONLY_CALLS, notif_filter_decide(&f, &e, 1000U));
}

/* ==========================================================================
 * What the phone itself silenced
 * ========================================================================== */

static void test_a_silent_notification_is_dropped(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    e.silent = true;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_SILENT, notif_filter_decide(&f, &e, 1000U));
}

/* ==========================================================================
 * The same one twice, and too many at once
 * ========================================================================== */

static void test_the_same_notification_again_is_dropped(void)
{
    /* a message being edited arrives with the identifier it already had */
    struct notif_event e = ev(42U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1000U));

    e.event_id = (uint8_t)NOTIF_EV_MODIFIED;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_REPEAT, notif_filter_decide(&f, &e, 100000U));
}

static void test_only_the_last_few_identifiers_are_remembered(void)
{
    /*
     * Eight is what the ring holds. The ninth new one pushes the first out,
     * and that first one could then be shown again — which is fine: by
     * then it is old news, and remembering every identifier of a ride is
     * not worth the memory.
     */
    /* eight written out, not taken from the constant: a test that used the
     * constant on both sides would follow it wherever it was changed to */
    TEST_ASSERT_EQUAL_UINT8(8U, NOTIF_RECENT);

    for (uint32_t i = 0U; i < 8U; i++) {
        struct notif_event e = ev(i, NOTIF_CAT_SOCIAL);

        e.important = true;     /* so the gap rule does not get in the way */
        TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1000U + (i * 100U)));
    }

    struct notif_event first = ev(0U, NOTIF_CAT_SOCIAL);

    first.important = true;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_REPEAT, notif_filter_decide(&f, &first, 5000U));

    /* one more new one, and the first has fallen out of the ring */
    struct notif_event more = ev(99U, NOTIF_CAT_SOCIAL);

    more.important = true;
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &more, 6000U));
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &first, 7000U));
}

static void test_an_identifier_of_zero_is_a_real_identifier(void)
{
    /*
     * The ring starts as zeros, so a notification whose identifier really
     * is zero looked like one already seen and the first message of a ride
     * vanished. Nothing in the service stops a phone from using zero.
     */
    struct notif_event e = ev(0U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1000U));
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_REPEAT, notif_filter_decide(&f, &e, 100000U));
}

static void test_a_group_chat_does_not_take_over_the_screen(void)
{
    struct notif_event first = ev(1U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &first, 1000U));

    /* thirty more in the next minute, each with its own identifier */
    for (uint32_t i = 2U; i < 32U; i++) {
        struct notif_event e = ev(i, NOTIF_CAT_SOCIAL);

        TEST_ASSERT_EQUAL_INT(NOTIF_DROP_TOO_SOON,
                              notif_filter_decide(&f, &e, 1000U + (i * 500U)));
    }

    /* and once the gap has passed, the next one is shown */
    struct notif_event later = ev(100U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW,
                          notif_filter_decide(&f, &later, 1000U + NOTIF_MIN_GAP_MS));
}

static void test_the_first_notification_of_all_is_never_too_soon(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 0U));
}

static void test_an_important_one_jumps_the_gap(void)
{
    struct notif_event first = ev(1U, NOTIF_CAT_SOCIAL);
    struct notif_event urgent = ev(2U, NOTIF_CAT_SCHEDULE);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &first, 1000U));

    urgent.important = true;
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &urgent, 1100U));
}

static void test_an_important_one_still_obeys_the_category(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_NEWS);

    e.important = true;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(&f, &e, 1000U));
}

/* ==========================================================================
 * The phone going away
 * ========================================================================== */

static void test_the_phone_reconnecting_starts_clean(void)
{
    struct notif_event e = ev(42U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1000U));

    notif_filter_reset(&f);

    /* the same message from a phone that reconnected is news again */
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &e, 1100U));

    /* and the rider's choices survived */
    TEST_ASSERT_TRUE(notif_filter_category(&f, NOTIF_CAT_SOCIAL));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_NEWS));
}

static void test_the_clock_wrapping_does_not_silence_everything(void)
{
    /* k_uptime_get_32() wraps after 49 days; unsigned arithmetic carries */
    uint32_t late = 0xFFFFF000UL;
    struct notif_event a = ev(1U, NOTIF_CAT_SOCIAL);
    struct notif_event b = ev(2U, NOTIF_CAT_SOCIAL);

    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW, notif_filter_decide(&f, &a, late));
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_TOO_SOON, notif_filter_decide(&f, &b, late + 1000U));
    TEST_ASSERT_EQUAL_INT(NOTIF_SHOW,
                          notif_filter_decide(&f, &b, late + NOTIF_MIN_GAP_MS));
}

static void test_nothing_blows_up_without_a_filter(void)
{
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    notif_filter_init(NULL);
    notif_filter_reset(NULL);
    notif_filter_set_category(NULL, NOTIF_CAT_SOCIAL, true);
    notif_filter_set_category(&f, NOTIF_CAT_COUNT, true);
    notif_filter_set_only_calls(NULL, true);
    notif_filter_set_calls_always(NULL, true);
    TEST_ASSERT_FALSE(notif_filter_category(NULL, NOTIF_CAT_SOCIAL));
    TEST_ASSERT_FALSE(notif_filter_category(&f, NOTIF_CAT_COUNT));
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(NULL, &e, 0U));
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(&f, NULL, 0U));
}

static void test_a_category_the_service_does_not_have(void)
{
    /* a phone sending a category number past the table drops it */
    struct notif_event e = ev(1U, NOTIF_CAT_SOCIAL);

    e.category = 200U;
    TEST_ASSERT_EQUAL_INT(NOTIF_DROP_CATEGORY, notif_filter_decide(&f, &e, 1000U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_message_gets_through);
    RUN_TEST(test_the_categories_a_rider_on_a_bicycle_wants);
    RUN_TEST(test_a_category_the_rider_turned_off);
    RUN_TEST(test_a_category_can_be_turned_off_again);
    RUN_TEST(test_everything_the_phone_already_had_is_dropped);
    RUN_TEST(test_even_a_call_the_phone_already_had_is_dropped);
    RUN_TEST(test_a_notification_being_taken_away_shows_nothing);
    RUN_TEST(test_a_call_ignores_every_other_rule);
    RUN_TEST(test_a_call_can_be_made_to_follow_the_rules);
    RUN_TEST(test_only_calls_stops_everything_else);
    RUN_TEST(test_a_missed_call_is_not_an_incoming_one);
    RUN_TEST(test_a_silent_notification_is_dropped);
    RUN_TEST(test_the_same_notification_again_is_dropped);
    RUN_TEST(test_only_the_last_few_identifiers_are_remembered);
    RUN_TEST(test_an_identifier_of_zero_is_a_real_identifier);
    RUN_TEST(test_a_group_chat_does_not_take_over_the_screen);
    RUN_TEST(test_the_first_notification_of_all_is_never_too_soon);
    RUN_TEST(test_an_important_one_jumps_the_gap);
    RUN_TEST(test_an_important_one_still_obeys_the_category);
    RUN_TEST(test_the_phone_reconnecting_starts_clean);
    RUN_TEST(test_the_clock_wrapping_does_not_silence_everything);
    RUN_TEST(test_nothing_blows_up_without_a_filter);
    RUN_TEST(test_a_category_the_service_does_not_have);

    return UNITY_END();
}
