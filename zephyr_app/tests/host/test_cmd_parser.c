/**
 * @file test_cmd_parser.c
 * @brief Command sentences of the legacy (src/model/cmd_parser.c)
 *
 * `libraries/VParser/VParser.cpp:59-320`: sentences that begin with `$`,
 * with a type of three or four letters, terms separated by commas and a
 * line end. The kinds and what each one carries are in
 * `docs/07-radio-ant-ble.md`, seção stravaAP e comandos.
 */

#include <string.h>

#include "unity.h"

#include "model/cmd_parser.h"

static struct cmd_parser p;

void setUp(void)
{
    cmd_parser_init(&p);
}

void tearDown(void) {}

static void test_a_position_from_the_pc(void)
{
    /* $LOC,secj,lat*1e7,lon*1e7,ele*100,v_cm/s */
    TEST_ASSERT_EQUAL_INT(CMD_LOC, cmd_parser_line(&p, "$LOC,45296,486921000,61844000,24500,830\r\n"));

    const struct cmd_data *d = cmd_parser_data(&p);

    TEST_ASSERT_EQUAL_UINT32(45296U, d->secj);
    TEST_ASSERT_EQUAL_INT32(486921000, d->lat_e7);
    TEST_ASSERT_EQUAL_INT32(61844000, d->lon_e7);
    TEST_ASSERT_EQUAL_INT32(24500, d->ele_cm);
    TEST_ASSERT_EQUAL_INT32(830, d->speed_cms);
}

static void test_a_position_south_and_west(void)
{
    /* the port rides in Brazil: both signs have to survive */
    TEST_ASSERT_EQUAL_INT(CMD_LOC, cmd_parser_line(&p, "$LOC,1,-235500000,-465000000,-1200,0\r\n"));

    const struct cmd_data *d = cmd_parser_data(&p);

    TEST_ASSERT_EQUAL_INT32(-235500000, d->lat_e7);
    TEST_ASSERT_EQUAL_INT32(-465000000, d->lon_e7);
    TEST_ASSERT_EQUAL_INT32(-1200, d->ele_cm);
}

static void test_the_heart_rate_and_the_cadence(void)
{
    TEST_ASSERT_EQUAL_INT(CMD_HRM, cmd_parser_line(&p, "$HRM,152,390\r\n"));
    TEST_ASSERT_EQUAL_UINT16(152U, cmd_parser_data(&p)->bpm);
    TEST_ASSERT_EQUAL_UINT16(390U, cmd_parser_data(&p)->rr_ms);

    TEST_ASSERT_EQUAL_INT(CMD_CAD, cmd_parser_line(&p, "$CAD,88,1250\r\n"));
    TEST_ASSERT_EQUAL_UINT16(88U, cmd_parser_data(&p)->rpm);
    TEST_ASSERT_EQUAL_UINT16(1250U, cmd_parser_data(&p)->cad_speed);
}

static void test_an_order_and_a_question(void)
{
    TEST_ASSERT_EQUAL_INT(CMD_DWN, cmd_parser_line(&p, "$DWN,16\r\n"));
    TEST_ASSERT_EQUAL_UINT8(CMD_DWN_MSC, cmd_parser_data(&p)->code);

    TEST_ASSERT_EQUAL_INT(CMD_QRY, cmd_parser_line(&p, "$QRY,2,@50925.txt\r\n"));
    TEST_ASSERT_EQUAL_UINT8(CMD_QRY_SEND, cmd_parser_data(&p)->qry_type);
    TEST_ASSERT_EQUAL_STRING("@50925.txt", cmd_parser_data(&p)->name);

    TEST_ASSERT_EQUAL_INT(CMD_QRY, cmd_parser_line(&p, "$QRY,1,EMPTY.TXT\r\n"));
    TEST_ASSERT_EQUAL_UINT8(CMD_QRY_LIST, cmd_parser_data(&p)->qry_type);
}

static void test_a_notification_of_the_phone(void)
{
    TEST_ASSERT_EQUAL_INT(CMD_ANCS, cmd_parser_line(&p, "$ANCS,3,Mensagem,Chegando em 5 min\r\n"));

    const struct cmd_data *d = cmd_parser_data(&p);

    TEST_ASSERT_EQUAL_UINT8(3U, d->code);
    TEST_ASSERT_EQUAL_STRING("Mensagem", d->title);
    TEST_ASSERT_EQUAL_STRING("Chegando em 5 min", d->text);
}

static void test_the_sentence_comes_in_piece_by_piece(void)
{
    /* the radio hands the characters as they arrive */
    static const char sentence[] = "$BTN,2\r\n";
    enum cmd_kind kind = CMD_NONE;

    for (size_t i = 0U; i < (sizeof(sentence) - 1U); i++) {
        enum cmd_kind got = cmd_parser_feed(&p, sentence[i]);

        if (got != CMD_NONE) {
            kind = got;
        }
    }

    TEST_ASSERT_EQUAL_INT(CMD_BTN, kind);
    TEST_ASSERT_EQUAL_UINT8(2U, cmd_parser_data(&p)->code);
}

static void test_rubbish_before_the_sentence_is_thrown_away(void)
{
    TEST_ASSERT_EQUAL_INT(CMD_LOC, cmd_parser_line(&p, "lixo,que,veio,antes$LOC,10,1,2,3,4\r\n"));
    TEST_ASSERT_EQUAL_UINT32(10U, cmd_parser_data(&p)->secj);
}

static void test_a_sentence_of_another_kind_is_ignored(void)
{
    TEST_ASSERT_EQUAL_INT(CMD_NONE, cmd_parser_line(&p, "$GPRMC,123519,A,4807.038,N\r\n"));
    TEST_ASSERT_EQUAL_INT(CMD_NONE, cmd_parser_line(&p, "\r\n"));
    TEST_ASSERT_EQUAL_INT(CMD_NONE, cmd_parser_feed(NULL, 'a'));
    TEST_ASSERT_EQUAL_INT(CMD_NONE, cmd_parser_line(&p, ""));
    TEST_ASSERT_NULL(cmd_parser_data(NULL));
}

static void test_a_sentence_without_its_line_end_still_counts(void)
{
    /* the tools of tools/zpm send the line without the CR */
    TEST_ASSERT_EQUAL_INT(CMD_DWN, cmd_parser_line(&p, "$DWN,18"));
    TEST_ASSERT_EQUAL_UINT8(CMD_DWN_CALIB_MAG, cmd_parser_data(&p)->code);
}

static void test_a_new_sentence_drops_the_one_before(void)
{
    TEST_ASSERT_EQUAL_INT(CMD_HRM, cmd_parser_line(&p, "$HRM,99,$HRM,140,400\r\n"));
    /* the second sentence is the one that came through */
    TEST_ASSERT_EQUAL_UINT16(140U, cmd_parser_data(&p)->bpm);
    TEST_ASSERT_EQUAL_UINT16(400U, cmd_parser_data(&p)->rr_ms);
}

static void test_a_term_longer_than_the_line_does_not_run_over(void)
{
    char big[CMD_LINE_MAX + 100U];

    (void)memset(big, 'A', sizeof(big));
    big[0] = '$';
    big[sizeof(big) - 1U] = '\0';

    /* nothing blows up and the sentence is not one of ours */
    TEST_ASSERT_EQUAL_INT(CMD_NONE, cmd_parser_line(&p, big));
}

static void test_the_orders_that_destroy_come_only_from_the_menu(void)
{
    /* the legacy took them from anyone calling itself stravaAP */
    TEST_ASSERT_FALSE(cmd_dwn_allowed(CMD_DWN_FORMAT));
    TEST_ASSERT_FALSE(cmd_dwn_allowed(CMD_DWN_MKFS));
    TEST_ASSERT_FALSE(cmd_dwn_allowed(CMD_DWN_HARDFAULT));
    TEST_ASSERT_FALSE(cmd_dwn_allowed(CMD_DWN_MEMORY));
    TEST_ASSERT_FALSE(cmd_dwn_allowed(CMD_DWN_DFU));

    TEST_ASSERT_TRUE(cmd_dwn_allowed(CMD_DWN_MSC));
    TEST_ASSERT_TRUE(cmd_dwn_allowed(CMD_DWN_CALIB_MAG));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_position_from_the_pc);
    RUN_TEST(test_a_position_south_and_west);
    RUN_TEST(test_the_heart_rate_and_the_cadence);
    RUN_TEST(test_an_order_and_a_question);
    RUN_TEST(test_a_notification_of_the_phone);
    RUN_TEST(test_the_sentence_comes_in_piece_by_piece);
    RUN_TEST(test_rubbish_before_the_sentence_is_thrown_away);
    RUN_TEST(test_a_sentence_of_another_kind_is_ignored);
    RUN_TEST(test_a_sentence_without_its_line_end_still_counts);
    RUN_TEST(test_a_new_sentence_drops_the_one_before);
    RUN_TEST(test_a_term_longer_than_the_line_does_not_run_over);
    RUN_TEST(test_the_orders_that_destroy_come_only_from_the_menu);

    return UNITY_END();
}
