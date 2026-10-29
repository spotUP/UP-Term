/* The AMARCHY port's STATUS and BAR STATUS replies fit REXX_HOST_OUT with the largest counters (amarchy/status_core.c). */
#include "../amarchy/status_core.h"
#include "../amarchy/rexx_host.h"
#include "harness.h"
#include <string.h>

static void test_fit(void)
{
    long big[STATUS_NVALS], mid[STATUS_NVALS], five[STATUS_NVALS];
    char out[REXX_HOST_OUT];
    int i;
    for (i = 0; i < STATUS_NVALS; i++) {
        big[i] = 2147483647L;
        mid[i] = 12345L;
        five[i] = 99999L;
    }
    /* STATUS holds counters up to 5 digits (99999 ticks is 28 hours of tick; the old 16 key line needed 168 bytes here
     * and lost ticks=); the last key must survive whole */
    CHECK(status_line(0, five, out, sizeof out) <= REXX_HOST_OUT);
    CHECK(strstr(out, " ticks=99999") != 0);
    CHECK(status_line(0, mid, out, sizeof out) <= REXX_HOST_OUT);
    CHECK(status_line(1, big, out, sizeof out) <= REXX_HOST_OUT);
    CHECK(strstr(out, " ticks=2147483647") != 0);
    CHECK(strncmp(out, "bar=", 4) == 0);
    CHECK(strstr(out, " draws=") != 0 && strstr(out, " raises=") != 0);
    CHECK(strstr(out, " hits=") == 0);
    CHECK(strstr(out, " net=2147483647") != 0);
}

static void test_keys(void)
{
    long v[STATUS_NVALS] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    char out[REXX_HOST_OUT];
    status_line(0, v, out, sizeof out);
    CHECK_STR(out, "hits=1 rows=2 enabled=3 active=4 wins=5 snaps=6 bar=7 wbpens=8 wbrefused=9 shots=10 notes=11 ticks=12");
    CHECK(strstr(out, "draws=") == 0 && strstr(out, "panels=") == 0 && strstr(out, "launches=") == 0 && strstr(out, "raises=") == 0);
}

static void test_verb(void)
{
    CHECK_INT(bar_verb(" status"), BARV_STATUS);
    CHECK_INT(bar_verb(" Off "), BARV_OFF);
    CHECK_INT(bar_verb("\ton"), BARV_ON);
    CHECK_INT(bar_verb(" TOGGLE"), BARV_TOGGLE);
    CHECK_INT(bar_verb(" text"), BARV_TEXT);
    CHECK_INT(bar_verb(" Pens"), BARV_PENS);
    CHECK_INT(bar_verb(" pensx"), BARV_BAD);
    /* R2.5b: the theme-change signal is a different saved theme name */
    CHECK_INT(tw_theme_changed("tokyo-night", "tokyo-night"), 0);
    CHECK_INT(tw_theme_changed("tokyo-night", "catppuccin-latte"), 1);
    CHECK_INT(bar_verb(""), BARV_BAD);
    CHECK_INT(bar_verb(" onx"), BARV_BAD);
    CHECK_INT(bar_verb(" off now"), BARV_BAD);
}

static void test_bar_on_during_hold_is_remembered(void)
{
    int want = 1, a;
    /* held: BAR ON never opens the window, it is remembered for the end of the hold */
    want = 0;
    CHECK_INT(bar_plan(BARV_ON, 1, 0, &want), BARA_NONE);
    CHECK_INT(want, 1);
    CHECK_INT(bar_plan(BARV_OFF, 1, 0, &want), BARA_NONE);
    CHECK_INT(want, 0);
    a = bar_plan(BARV_TOGGLE, 1, 0, &want);             /* the wish was off, so toggle wishes it on */
    CHECK_INT(a, BARA_NONE);
    CHECK_INT(want, 1);
    /* not held: the verbs act at once, as before */
    CHECK_INT(bar_plan(BARV_ON, 0, 0, &want), BARA_OPEN);
    CHECK_INT(bar_plan(BARV_ON, 0, 1, &want), BARA_NONE);
    CHECK_INT(bar_plan(BARV_OFF, 0, 1, &want), BARA_CLOSE);
    CHECK_INT(want, 0);
    CHECK_INT(bar_plan(BARV_TOGGLE, 0, 0, &want), BARA_OPEN);
    CHECK_INT(bar_plan(BARV_STATUS, 0, 0, &want), BARA_NONE);
}

void suite_status_core(void)
{
    test_fit();
    test_keys();
    test_verb();
    test_bar_on_during_hold_is_remembered();
}
