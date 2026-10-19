/* config/splits_state: a tab's split panes as the text of ENV:up-term/splits
 * and back -- the round trip, the caps that empty a field instead of cutting
 * it, the control-character rule, what a reader ignores. */
#include "harness.h"
#include <string.h>
#include "../config/splits_state.h"

static splits_state S, T;
static char text[2048];

static void fill(splits_state *s, const char *layout, int n)
{
    int k;
    memset(s, 0, sizeof *s);
    strcpy(s->layout, layout);
    s->n = n;
    for (k = 0; k < n; k++) {
        strcpy(s->cwd[k], "file://host/Work/dir0");
        s->cwd[k][20] = (char)('0' + k);
        strcpy(s->cmd[k], "vim file0.c");
        s->cmd[k][8] = (char)('0' + k);
    }
}

static int same(const splits_state *a, const splits_state *b)
{
    int k;
    if (a->n != b->n || strcmp(a->layout, b->layout))
        return 0;
    for (k = 0; k < a->n; k++)
        if (strcmp(a->cwd[k], b->cwd[k]) || strcmp(a->cmd[k], b->cmd[k]))
            return 0;
    return 1;
}

/* text = head, then n bytes of 'a', then tail */
static void build(const char *head, int n, const char *tail)
{
    strcpy(text, head);
    memset(text + strlen(text), 'a', (size_t)n);
    strcpy(text + strlen(head) + n, tail);
}

static int parse(const char *t)
{
    return splits_state_parse(&T, t, (long)strlen(t));
}

static void round_trip(const char *layout, int n)
{
    int len;
    fill(&S, layout, n);
    len = splits_state_text(&S, text, (int)sizeof text);
    CHECK_INT(len, (int)strlen(text));
    CHECK_INT(splits_state_parse(&T, text, len), n);
    CHECK(same(&S, &T));
}

static void a_saved_state_reads_back_for_2_3_and_4_panes(void)
{
    round_trip("r32768(p,p)", 2);
    round_trip("r32768(p,d40000(p,p))", 3);
    round_trip("r32768(d32768(p,p),d32768(p,p))", 4);
}

static void the_text_has_the_documented_shape(void)
{
    fill(&S, "r32768(p,p)", 2);
    S.cmd[1][0] = 0;
    CHECK(splits_state_text(&S, text, (int)sizeof text) > 0);
    CHECK_STR(text, "[splits]\nlayout = r32768(p,p)\n"
                    "pane0-cwd = file://host/Work/dir0\npane0-cmd = vim file0.c\n"
                    "pane1-cwd = file://host/Work/dir1\n");
}

static void empty_fields_are_left_out_and_come_back_empty(void)
{
    fill(&S, "r32768(p,p)", 2);
    S.cwd[0][0] = 0; S.cmd[0][0] = 0; S.cwd[1][0] = 0;
    CHECK(splits_state_text(&S, text, (int)sizeof text) > 0);
    CHECK(strstr(text, "pane0") == 0);
    CHECK(strstr(text, "pane1-cwd") == 0);
    CHECK(strstr(text, "pane1-cmd") != 0);
    CHECK_INT(splits_state_parse(&T, text, (long)strlen(text)), 2);
    CHECK(same(&S, &T));
}

static void a_cwd_longer_than_the_cap_is_kept_empty_not_cut(void)
{
    /* one byte too long (the cap includes the NUL), then far too long */
    build("layout = r32768(p,p)\npane0-cwd = ", SPLITS_CWD_MAX, "\npane1-cmd = ok\n");
    CHECK_INT(parse(text), 2);
    CHECK_STR(T.cwd[0], "");
    CHECK_STR(T.cmd[1], "ok");
    build("layout = r32768(p,p)\npane1-cwd = ", 1100, "\npane1-cmd = ok\n");
    CHECK_INT(parse(text), 2);
    CHECK_STR(T.cwd[1], "");
    CHECK_STR(T.cmd[1], "ok");
    /* the longest that fits is kept whole */
    build("layout = r32768(p,p)\npane0-cwd = ", SPLITS_CWD_MAX - 1, "\n");
    CHECK_INT(parse(text), 2);
    CHECK_INT(strlen(T.cwd[0]), SPLITS_CWD_MAX - 1);
    build("layout = r32768(p,p)\npane0-cmd = ", SPLITS_CMD_MAX, "\n");
    CHECK_INT(parse(text), 2);
    CHECK_STR(T.cmd[0], "");
    build("layout = r32768(p,p)\npane0-cmd = ", SPLITS_CMD_MAX - 1, "\n");
    CHECK_INT(parse(text), 2);
    CHECK_INT(strlen(T.cmd[0]), SPLITS_CMD_MAX - 1);
}

static void the_writer_leaves_out_a_field_it_cannot_hold_whole(void)
{
    fill(&S, "r32768(p,p)", 2);
    memset(S.cwd[0], 'a', sizeof S.cwd[0]);   /* no NUL: not a string that fits */
    CHECK(splits_state_text(&S, text, (int)sizeof text) > 0);
    CHECK(strstr(text, "pane0-cwd") == 0);
}

static void a_command_with_a_control_character_is_kept_empty(void)
{
    int len;
    CHECK_INT(parse("layout = r32768(p,p)\npane0-cmd = vim\tx\npane1-cmd = ls\x7f\x01x\n"), 2);
    CHECK_STR(T.cmd[0], "");
    CHECK_STR(T.cmd[1], "");
    CHECK_INT(parse("layout = r32768(p,p)\npane0-cmd = vim x\n"), 2);
    CHECK_STR(T.cmd[0], "vim x");
    fill(&S, "r32768(p,p)", 2);
    strcpy(S.cmd[0], "rm\x1b[2J");
    strcpy(S.cmd[1], "ls\x7f");
    strcpy(S.cwd[1], "file://h/a\nb");
    len = splits_state_text(&S, text, (int)sizeof text);
    CHECK(len > 0);
    CHECK(strstr(text, "pane0-cmd") == 0);
    CHECK(strstr(text, "pane1-cmd") == 0);
    CHECK(strstr(text, "pane1-cwd") == 0);
}

static void no_usable_layout_gives_0_and_clears(void)
{
    memset(&T, 'x', sizeof T);
    CHECK_INT(parse("layout = r0(p,p)\npane0-cwd = file://h/a\n"), 0);   /* bad ratio */
    CHECK_INT(T.n, 0);
    CHECK_STR(T.layout, "");
    CHECK_STR(T.cwd[0], "");
    CHECK_INT(parse("layout = banana\n"), 0);
    CHECK_INT(parse("pane0-cwd = file://h/a\n"), 0);   /* no layout at all */
    CHECK_INT(parse(""), 0);
    CHECK_INT(parse("layout = r32768(p,p)extra\n"), 0);
}

static void a_one_pane_layout_is_not_a_split(void)
{
    memset(&T, 'x', sizeof T);
    CHECK_INT(parse("layout = p\npane0-cwd = file://h/a\n"), 0);
    CHECK_INT(T.n, 0);
    CHECK_STR(T.cwd[0], "");
    fill(&S, "p", 1);
    CHECK_INT(splits_state_text(&S, text, (int)sizeof text), -1);
}

static void unknown_keys_comments_and_blank_lines_are_ignored(void)
{
    CHECK_INT(parse("; saved by up-term\n# note\n\n[splits]\ncolour = red\n"
                    "layout = r32768(p,p)\nzoom = 1\npane = 5\npane0 = x\n"
                    "pane0-font = topaz\n; pane1-cwd = file://h/no\n"
                    "pane1-cwd = file://h/yes\n"), 2);
    CHECK_STR(T.cwd[0], "");
    CHECK_STR(T.cwd[1], "file://h/yes");
}

static void keys_are_case_insensitive_and_whitespace_is_trimmed(void)
{
    CHECK_INT(parse("  LAYOUT\t=   r32768(p,p)  \n Pane1-CWD=file://h/a b \n"
                    "PANE0-Cmd =\t vim -R x \t\n"), 2);
    CHECK_STR(T.layout, "r32768(p,p)");
    CHECK_STR(T.cwd[1], "file://h/a b");
    CHECK_STR(T.cmd[0], "vim -R x");
}

static void crlf_line_ends_are_accepted(void)
{
    CHECK_INT(parse("[splits]\r\nlayout = r32768(p,p)\r\npane0-cwd = file://h/a\r\n"
                    "pane1-cmd = ls\r\n"), 2);
    CHECK_STR(T.layout, "r32768(p,p)");
    CHECK_STR(T.cwd[0], "file://h/a");
    CHECK_STR(T.cmd[1], "ls");
}

static void a_text_that_does_not_fit_is_refused_not_cut(void)
{
    int len, size;
    fill(&S, "r32768(d40000(p,p),p)", 3);
    len = splits_state_text(&S, text, (int)sizeof text);
    CHECK(len > 0);
    /* exactly len+1 bytes is enough; one fewer must refuse */
    CHECK_INT(splits_state_text(&S, text, len + 1), len);
    for (size = 0; size <= len; size++)
        if (splits_state_text(&S, text, size) != -1) {
            CHECK_INT(size, -1);   /* report the size that was accepted */
            break;
        }
    CHECK_INT(splits_state_text(&S, text, 0), -1);
}

static void a_pane_key_at_or_past_the_count_is_ignored(void)
{
    /* pane2 and pane3 exist in LAY_MAX_PANES but not in a 2-pane layout, even
     * when they stand before the layout line */
    CHECK_INT(parse("pane2-cwd = file://h/x\npane3-cmd = ls\nlayout = r32768(p,p)\n"
                    "pane2-cmd = vi\npane9-cwd = file://h/y\npane99-cwd = z\n"), 2);
    CHECK_INT(T.n, 2);
    CHECK_STR(T.cwd[2], "");
    CHECK_STR(T.cmd[2], "");
    CHECK_STR(T.cmd[3], "");
    CHECK_STR(T.cwd[0], "");
}

void suite_splits_state(void)
{
    a_saved_state_reads_back_for_2_3_and_4_panes();
    the_text_has_the_documented_shape();
    empty_fields_are_left_out_and_come_back_empty();
    a_cwd_longer_than_the_cap_is_kept_empty_not_cut();
    the_writer_leaves_out_a_field_it_cannot_hold_whole();
    a_command_with_a_control_character_is_kept_empty();
    no_usable_layout_gives_0_and_clears();
    a_one_pane_layout_is_not_a_split();
    unknown_keys_comments_and_blank_lines_are_ignored();
    keys_are_case_insensitive_and_whitespace_is_trimmed();
    crlf_line_ends_are_accepted();
    a_text_that_does_not_fit_is_refused_not_cut();
    a_pane_key_at_or_past_the_count_is_ignored();
}
