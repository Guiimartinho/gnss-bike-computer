/**
 * @file test_qry.c
 * @brief The `$QRY` command (src/model/qry.c)
 *
 * `$QRY` is of this port; the stravaV10 has no such sentence, so there is
 * no legacy to check against. What the tests hold down is the shape of the
 * replies, which something on the other end has to parse, and the one rule
 * that matters: **an activity cannot be erased over the wire.** A ride the
 * rider spent four hours on is not something a stray command, or a bug in
 * whatever is talking to the device, should be able to take away.
 */

#include <string.h>

#include "unity.h"

#include "model/qry.h"

static char out[QRY_REPLY_LEN];
static char path[80];

void setUp(void)
{
    (void)memset(out, 0, sizeof(out));
    (void)memset(path, 0, sizeof(path));
}

void tearDown(void) {}

/* ==========================================================================
 * The shape of a listing
 * ========================================================================== */

static void test_one_line_of_a_listing(void)
{
    TEST_ASSERT_TRUE(qry_format_entry(out, sizeof(out), "260926.FIT", 148992U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,1,260926.FIT,148992\r\n", out);
}

static void test_a_listing_ends_by_saying_how_many(void)
{
    TEST_ASSERT_TRUE(qry_format_end(out, sizeof(out), 7U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,1,END,7\r\n", out);
}

static void test_an_empty_storage_still_ends(void)
{
    /* nothing to list is an answer, not a silence */
    TEST_ASSERT_TRUE(qry_format_end(out, sizeof(out), 0U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,1,END,0\r\n", out);
}

static void test_a_file_of_no_size(void)
{
    TEST_ASSERT_TRUE(qry_format_entry(out, sizeof(out), "VAZIO.WKT", 0U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,1,VAZIO.WKT,0\r\n", out);
}

static void test_a_file_bigger_than_the_storage_could_hold(void)
{
    /* the size is whatever the file system says; nothing is truncated */
    TEST_ASSERT_TRUE(qry_format_entry(out, sizeof(out), "GRANDE.FIT", 4294967295U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,1,GRANDE.FIT,4294967295\r\n", out);
}

static void test_a_name_too_long_for_the_line_writes_nothing(void)
{
    /*
     * A half-written sentence is worse than none: the other end would
     * parse a name that is not the name of anything.
     */
    char huge[QRY_REPLY_LEN * 2];

    (void)memset(huge, 'A', sizeof(huge) - 1U);
    huge[sizeof(huge) - 1U] = '\0';

    TEST_ASSERT_EQUAL_UINT32(0U, qry_format_entry(out, sizeof(out), huge, 1U));
}

/* ==========================================================================
 * The other two answers
 * ========================================================================== */

static void test_the_answer_when_it_worked(void)
{
    TEST_ASSERT_TRUE(qry_format_ok(out, sizeof(out), 3U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,3,OK\r\n", out);
}

static void test_the_answer_when_it_did_not(void)
{
    TEST_ASSERT_TRUE(qry_format_error(out, sizeof(out), 3U, QRY_ERR_NOTFOUND) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,3,ERR,NOTFOUND\r\n", out);
}

static void test_every_reason_has_a_word(void)
{
    /*
     * Words and not numbers: whoever reads this is a person at a terminal
     * or a script, and both read NOTFOUND faster than they read -2.
     */
    TEST_ASSERT_EQUAL_STRING("OK", qry_error_word(QRY_ERR_NONE));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", qry_error_word(QRY_ERR_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("NAME", qry_error_word(QRY_ERR_NAME));
    TEST_ASSERT_EQUAL_STRING("FORBIDDEN", qry_error_word(QRY_ERR_FORBIDDEN));
    TEST_ASSERT_EQUAL_STRING("NOTFOUND", qry_error_word(QRY_ERR_NOTFOUND));
    TEST_ASSERT_EQUAL_STRING("IO", qry_error_word(QRY_ERR_IO));
    TEST_ASSERT_EQUAL_STRING("USESMP", qry_error_word(QRY_ERR_USE_SMP));

    /* and anything that is not one of them still says something */
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", qry_error_word((enum qry_error)99));
}

static void test_asking_for_a_file_points_at_the_other_way(void)
{
    /*
     * Sending a file is refused on purpose: mcumgr on the SMP link does it
     * with offsets, a checksum and a transfer that resumes, and doing it
     * again over a serial line in twenty-byte notifications would be
     * slower and worse.
     */
    TEST_ASSERT_TRUE(qry_format_error(out, sizeof(out), 2U, QRY_ERR_USE_SMP) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,2,ERR,USESMP\r\n", out);
}

/* ==========================================================================
 * What may be erased, and what may not
 * ========================================================================== */

static void test_a_route_may_be_erased(void)
{
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NONE, qry_check_erase("NANCY.PAR", path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING("/SD:/NANCY.PAR", path);
}

static void test_an_activity_may_not_be_erased(void)
{
    /*
     * The rule this file exists for. The policy of `model/file_policy.h`
     * lets an activity be read and never written, and erasing is a write:
     * four hours of a rider's life do not go away because something sent a
     * sentence.
     */
    TEST_ASSERT_EQUAL_INT(QRY_ERR_FORBIDDEN,
                          qry_check_erase("@260926.txt", path, sizeof(path)));
}

static void test_a_name_that_climbs_out_of_the_storage(void)
{
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("../secret", path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("a/b.PAR", path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("a\\b.PAR", path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("/SD:/NANCY.PAR", path, sizeof(path)));

    /*
     * Two dots with no separator anywhere: the name would pass the policy
     * on its extension alone, and it is the `..` rule and nothing else
     * that stops it. Without this case the rule could be deleted and every
     * other test would still pass.
     */
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("..PAR", path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("A..PAR", path, sizeof(path)));
}

static void test_a_name_that_is_not_there_at_all(void)
{
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("", path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase(NULL, path, sizeof(path)));
}

static void test_a_name_too_long_for_the_path(void)
{
    char huge[64];

    (void)memset(huge, 'B', sizeof(huge) - 1U);
    huge[sizeof(huge) - 1U] = '\0';

    /* the root plus the name does not fit, and nothing is half built */
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase(huge, path, 20U));
}

static void test_a_file_the_firmware_does_not_understand(void)
{
    /* nothing the device cannot read has any business taking room */
    TEST_ASSERT_EQUAL_INT(QRY_ERR_FORBIDDEN,
                          qry_check_erase("RANDOM.BIN", path, sizeof(path)));
}

static void test_nothing_blows_up_without_somewhere_to_write(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, qry_format_entry(NULL, 10U, "A", 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, qry_format_entry(out, 0U, "A", 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, qry_format_end(NULL, 10U, 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, qry_format_ok(NULL, 10U, 3U));
    TEST_ASSERT_EQUAL_UINT32(0U, qry_format_error(NULL, 10U, 3U, QRY_ERR_IO));
    TEST_ASSERT_EQUAL_INT(QRY_ERR_NAME, qry_check_erase("A.PAR", NULL, 10U));

    /* a file with no name in the listing still writes a whole sentence */
    TEST_ASSERT_TRUE(qry_format_entry(out, sizeof(out), NULL, 5U) > 0U);
    TEST_ASSERT_EQUAL_STRING("$QRY,1,,5\r\n", out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_one_line_of_a_listing);
    RUN_TEST(test_a_listing_ends_by_saying_how_many);
    RUN_TEST(test_an_empty_storage_still_ends);
    RUN_TEST(test_a_file_of_no_size);
    RUN_TEST(test_a_file_bigger_than_the_storage_could_hold);
    RUN_TEST(test_a_name_too_long_for_the_line_writes_nothing);
    RUN_TEST(test_the_answer_when_it_worked);
    RUN_TEST(test_the_answer_when_it_did_not);
    RUN_TEST(test_every_reason_has_a_word);
    RUN_TEST(test_asking_for_a_file_points_at_the_other_way);
    RUN_TEST(test_a_route_may_be_erased);
    RUN_TEST(test_an_activity_may_not_be_erased);
    RUN_TEST(test_a_name_that_climbs_out_of_the_storage);
    RUN_TEST(test_a_name_that_is_not_there_at_all);
    RUN_TEST(test_a_name_too_long_for_the_path);
    RUN_TEST(test_a_file_the_firmware_does_not_understand);
    RUN_TEST(test_nothing_blows_up_without_somewhere_to_write);

    return UNITY_END();
}
