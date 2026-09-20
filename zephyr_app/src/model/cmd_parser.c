/**
 * @file cmd_parser.c
 * @brief The command sentences of the legacy (VParser)
 */

#include <stdlib.h>
#include <string.h>

#include "model/cmd_parser.h"

/** Types of sentence, as the legacy names them (`VParser.cpp:11-18`) */
static const struct {
    const char *term;
    enum cmd_kind kind;
} sentence_types[] = {
    {"LOC", CMD_LOC},
    {"HRM", CMD_HRM},
    {"CAD", CMD_CAD},
    {"ANCS", CMD_ANCS},
    {"DWN", CMD_DWN},
    {"QRY", CMD_QRY},
    {"BTN", CMD_BTN},
    {"DBG", CMD_DBG},
};

static void copy_term(char *dst, size_t size, const char *term)
{
    size_t n = strlen(term);

    if (n > (size - 1U)) {
        n = size - 1U;
    }
    (void)memcpy(dst, term, n);
    dst[n] = '\0';
}

static int32_t term_int(const char *term)
{
    return (int32_t)strtol(term, NULL, 10);
}

/** The type of the sentence, from its first term */
static enum cmd_kind kind_of(const char *term)
{
    for (uint8_t i = 0U; i < (sizeof(sentence_types) / sizeof(sentence_types[0])); i++) {
        if (strcmp(term, sentence_types[i].term) == 0) {
            return sentence_types[i].kind;
        }
    }

    return CMD_OTHER;
}

/** One term ended: keep what it carries */
static void take_term(struct cmd_parser *p)
{
    if (p->term_number == 0U) {
        p->kind = kind_of(p->term);
        p->data.kind = p->kind;

        return;
    }

    switch (p->kind) {
    case CMD_LOC:
        switch (p->term_number) {
        case 1U:
            p->data.secj = (uint32_t)term_int(p->term);
            break;
        case 2U:
            p->data.lat_e7 = term_int(p->term);
            break;
        case 3U:
            p->data.lon_e7 = term_int(p->term);
            break;
        case 4U:
            p->data.ele_cm = term_int(p->term);
            break;
        case 5U:
            p->data.speed_cms = term_int(p->term);
            break;
        default:
            break;
        }
        break;

    case CMD_HRM:
        if (p->term_number == 1U) {
            p->data.bpm = (uint16_t)term_int(p->term);
        } else if (p->term_number == 2U) {
            p->data.rr_ms = (uint16_t)term_int(p->term);
        } else {
            /* nothing else in the sentence */
        }
        break;

    case CMD_CAD:
        if (p->term_number == 1U) {
            p->data.rpm = (uint16_t)term_int(p->term);
        } else if (p->term_number == 2U) {
            p->data.cad_speed = (uint16_t)term_int(p->term);
        } else {
            /* nothing else in the sentence */
        }
        break;

    case CMD_ANCS:
        if (p->term_number == 1U) {
            p->data.code = (uint8_t)term_int(p->term);
        } else if (p->term_number == 2U) {
            copy_term(p->data.title, sizeof(p->data.title), p->term);
        } else if (p->term_number == 3U) {
            copy_term(p->data.text, sizeof(p->data.text), p->term);
        } else {
            /* nothing else in the sentence */
        }
        break;

    case CMD_DWN:
    case CMD_BTN:
        if (p->term_number == 1U) {
            p->data.code = (uint8_t)term_int(p->term);
        }
        break;

    case CMD_QRY:
        if (p->term_number == 1U) {
            p->data.qry_type = (uint8_t)term_int(p->term);
        } else if (p->term_number == 2U) {
            copy_term(p->data.name, sizeof(p->data.name), p->term);
        } else {
            /* nothing else in the sentence */
        }
        break;

    case CMD_DBG:
        if (p->term_number == 1U) {
            p->data.code = (uint8_t)term_int(p->term);
        } else if (p->term_number == 2U) {
            copy_term(p->data.text, sizeof(p->data.text), p->term);
        } else {
            /* nothing else in the sentence */
        }
        break;

    default:
        break;
    }
}

void cmd_parser_init(struct cmd_parser *p)
{
    if (p == NULL) {
        return;
    }

    (void)memset(p, 0, sizeof(*p));
    p->kind = CMD_NONE;
}

enum cmd_kind cmd_parser_feed(struct cmd_parser *p, char c)
{
    if (p == NULL) {
        return CMD_NONE;
    }

    /* the sentence begins, whatever came before */
    if (c == '$') {
        (void)memset(&p->data, 0, sizeof(p->data));
        p->term_len = 0U;
        p->term_number = 0U;
        p->started = true;
        p->kind = CMD_OTHER;

        return CMD_NONE;
    }

    if ((c == ',') || (c == '\r') || (c == '\n') || (c == '*')) {
        if (!p->started) {
            return CMD_NONE;
        }

        p->term[p->term_len] = '\0';
        take_term(p);
        p->term_len = 0U;
        p->term_number++;

        /* the end of the line closes the sentence */
        if ((c == '\r') || (c == '\n')) {
            enum cmd_kind kind = p->kind;

            p->started = false;
            p->kind = CMD_NONE;

            return ((kind == CMD_OTHER) || (kind == CMD_NONE)) ? CMD_NONE : kind;
        }

        return CMD_NONE;
    }

    if (p->started && (p->term_len < (CMD_LINE_MAX - 1U))) {
        p->term[p->term_len] = c;
        p->term_len++;
    }

    return CMD_NONE;
}

enum cmd_kind cmd_parser_line(struct cmd_parser *p, const char *line)
{
    enum cmd_kind kind = CMD_NONE;

    if ((p == NULL) || (line == NULL)) {
        return CMD_NONE;
    }

    for (size_t i = 0U; line[i] != '\0'; i++) {
        enum cmd_kind got = cmd_parser_feed(p, line[i]);

        if (got != CMD_NONE) {
            kind = got;
        }
    }

    /* a line without its end still counts, as the tools send it */
    if ((kind == CMD_NONE) && p->started) {
        kind = cmd_parser_feed(p, '\n');
    }

    return kind;
}

const struct cmd_data *cmd_parser_data(const struct cmd_parser *p)
{
    return (p != NULL) ? &p->data : NULL;
}

bool cmd_dwn_allowed(uint8_t code)
{
    switch (code) {
    case CMD_DWN_MSC:
    case CMD_DWN_CALIB_MAG:
        return true;

    default:
        /* formatting, mkfs, the hardfault test and the memory test:
         * they only come from the menu, where the rider confirms them */
        return false;
    }
}
