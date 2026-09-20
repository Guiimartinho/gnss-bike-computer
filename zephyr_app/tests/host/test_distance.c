/**
 * @file test_distance.c
 * @brief Distance ridden against the legacy (src/model/distance.c)
 *
 * `legacy/source/model/Attitude.cpp:431-490` (`Attitude::computeDistance`):
 * sums the distance between raw positions, whatever the speed; the first
 * jump above 25 m throws the total away and starts the accumulation; from
 * there every 15 m is a snapshot, which in the legacy saves the point and
 * the state for the crash recovery.
 *
 * The points are around Nancy (48.6921, 6.1844), where the simulation
 * traces of the legacy were recorded.
 */

#include "unity.h"

#include "legacy_ref.h"
#include "model/distance.h"
#include "model/vecteur.h"

#define BASE_LAT    48.6921f
#define BASE_LON    6.1844f

/** Degrees of latitude for a given number of metres (about 111.320 m/deg) */
#define DEG_LAT(m)  ((float)(m) / 111320.0f)

static struct distance_acc d;

void setUp(void)
{
    distance_init(&d);
}

void tearDown(void) {}

/** Walk north in steps of @p step_m and return how many snapshots happened */
static unsigned int walk_north(float step_m, unsigned int steps)
{
    unsigned int snapshots = 0U;

    for (unsigned int i = 1U; i <= steps; i++) {
        if (distance_add(&d, BASE_LAT + DEG_LAT(step_m * (float)i), BASE_LON)) {
            snapshots++;
        }
    }

    return snapshots;
}

static void test_the_first_position_only_sets_the_reference(void)
{
    TEST_ASSERT_FALSE(distance_add(&d, BASE_LAT, BASE_LON));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, distance_total(&d));
}

static void test_the_first_twenty_five_metres_are_thrown_away(void)
{
    (void)distance_add(&d, BASE_LAT, BASE_LON);

    /* 20 m: not enough to start, and the total still carries them */
    (void)distance_add(&d, BASE_LAT + DEG_LAT(20.0f), BASE_LON);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, distance_total(&d));

    /* passing 25 m throws everything away and starts the count */
    (void)distance_add(&d, BASE_LAT + DEG_LAT(40.0f), BASE_LON);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, distance_total(&d));

    /* from here it counts */
    (void)distance_add(&d, BASE_LAT + DEG_LAT(50.0f), BASE_LON);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 10.0f, distance_total(&d));
}

static void test_a_snapshot_every_fifteen_metres(void)
{
    (void)distance_add(&d, BASE_LAT, BASE_LON);
    (void)distance_add(&d, BASE_LAT + DEG_LAT(30.0f), BASE_LON); /* starts */

    /* ten steps of 8 m: 80 m, five snapshots (15, 30, 45, 60 and 75 m) */
    unsigned int snapshots = 0U;

    for (unsigned int i = 1U; i <= 10U; i++) {
        if (distance_add(&d, BASE_LAT + DEG_LAT(30.0f + (8.0f * (float)i)), BASE_LON)) {
            snapshots++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(5U, snapshots);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 80.0f, distance_total(&d));
}

static void test_a_step_longer_than_fifteen_metres_gives_one_snapshot(void)
{
    (void)distance_add(&d, BASE_LAT, BASE_LON);
    (void)distance_add(&d, BASE_LAT + DEG_LAT(30.0f), BASE_LON); /* starts */

    /* 100 m in one step: the legacy saves one point, not seven */
    TEST_ASSERT_TRUE(distance_add(&d, BASE_LAT + DEG_LAT(130.0f), BASE_LON));
    TEST_ASSERT_FALSE(distance_add(&d, BASE_LAT + DEG_LAT(131.0f), BASE_LON));
}

static void test_standing_still_adds_nothing(void)
{
    (void)distance_add(&d, BASE_LAT, BASE_LON);
    (void)distance_add(&d, BASE_LAT + DEG_LAT(30.0f), BASE_LON); /* starts */

    for (unsigned int i = 0U; i < 20U; i++) {
        TEST_ASSERT_FALSE(distance_add(&d, BASE_LAT + DEG_LAT(30.0f), BASE_LON));
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, distance_total(&d));
}

static void test_the_total_matches_the_legacy_formula(void)
{
    (void)distance_add(&d, BASE_LAT, BASE_LON);
    (void)distance_add(&d, BASE_LAT + DEG_LAT(30.0f), BASE_LON); /* starts at zero */

    float legacy_total = 0.0f;
    float lat = BASE_LAT + DEG_LAT(30.0f);

    for (unsigned int i = 1U; i <= 40U; i++) {
        float next = BASE_LAT + DEG_LAT(30.0f + (7.5f * (float)i));

        legacy_total += legacy_distance_between(lat, BASE_LON, next, BASE_LON);
        (void)distance_add(&d, next, BASE_LON);
        lat = next;
    }

    TEST_ASSERT_FLOAT_WITHIN(0.5f, legacy_total, distance_total(&d));
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 300.0f, distance_total(&d));
}

static void test_the_crash_recovery_puts_a_total_back(void)
{
    distance_restore(&d, 12345.0f);
    TEST_ASSERT_EQUAL_FLOAT(12345.0f, distance_total(&d));

    /* it comes back already started, and the next 15 m give a snapshot */
    (void)distance_add(&d, BASE_LAT, BASE_LON);
    TEST_ASSERT_FALSE(distance_add(&d, BASE_LAT + DEG_LAT(10.0f), BASE_LON));
    TEST_ASSERT_TRUE(distance_add(&d, BASE_LAT + DEG_LAT(20.0f), BASE_LON));
}

static void test_a_null_accumulator_does_nothing(void)
{
    distance_init(NULL);
    distance_restore(NULL, 10.0f);
    TEST_ASSERT_FALSE(distance_add(NULL, BASE_LAT, BASE_LON));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, distance_total(NULL));
}

static void test_a_long_ride_keeps_one_snapshot_per_fifteen_metres(void)
{
    (void)distance_add(&d, BASE_LAT, BASE_LON);
    (void)distance_add(&d, BASE_LAT + DEG_LAT(30.0f), BASE_LON); /* starts */

    /*
     * 5 km in steps of 8 m, the pace of 30 km/h at 1 Hz. The rule waits for
     * more than 15 m since the last snapshot, and the epoch only brings
     * 8 m: one snapshot every two epochs, that is one per 16 m, not per
     * 15 m. The legacy behaves the same way.
     */
    unsigned int snapshots = walk_north(8.0f, 625U);

    TEST_ASSERT_UINT_WITHIN(2U, 313U, snapshots);
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 5014.0f, distance_total(&d));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_first_position_only_sets_the_reference);
    RUN_TEST(test_the_first_twenty_five_metres_are_thrown_away);
    RUN_TEST(test_a_snapshot_every_fifteen_metres);
    RUN_TEST(test_a_step_longer_than_fifteen_metres_gives_one_snapshot);
    RUN_TEST(test_standing_still_adds_nothing);
    RUN_TEST(test_the_total_matches_the_legacy_formula);
    RUN_TEST(test_the_crash_recovery_puts_a_total_back);
    RUN_TEST(test_a_null_accumulator_does_nothing);
    RUN_TEST(test_a_long_ride_keeps_one_snapshot_per_fifteen_metres);

    return UNITY_END();
}
