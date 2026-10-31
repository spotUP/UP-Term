#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../handler/wbpark.h"

/* A close hides an eligible window once; the open that follows shows it. */
static void close_then_open(void)
{
    wbpark p;
    memset(&p, 0, sizeof p);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_NONE); /* opened, never parked */
    CHECK_INT(wbpark_note(&p, 0, 1), WBPARK_HIDE);
    CHECK_INT(wbpark_note(&p, 0, 1), WBPARK_NONE); /* a second close: already gone */
    CHECK_INT(wbpark_note(&p, 1, 0), WBPARK_SHOW); /* eligibility only gates the close */
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_NONE);
}

/* IPrefs 3.1 only reaches CloseWorkBench when our window carries WFLG_WBENCHWINDOW; the handler opens it with
 * WA_WBenchWindow exactly when screennotify can park it and the screen is the Workbench. */
static void flag_decision(void)
{
    CHECK_INT(wbpark_flag_window(1, 1), 1);
    CHECK_INT(wbpark_flag_window(0, 1), 0); /* nobody parks it: stay a plain window */
    CHECK_INT(wbpark_flag_window(1, 0), 0); /* an own or foreign screen */
    {   /* wired: create_window asks it and adds the tag */
        static char buf[400000];
        FILE *f = fopen("handler/vtcon_handler.c", "rb");
        size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
        char *q;
        if (f) fclose(f);
        buf[n] = 0;
        q = strstr(buf, "wbpark_flag_window(");
        CHECK_INT(q != 0, 1);
        CHECK_INT(q && strstr(q, "WA_WBenchWindow") && strstr(q, "WA_WBenchWindow") - q < 400, 1);
    }
}

/* A window that may not leave (tabs, own screen) stays put. */
static void not_eligible(void)
{
    wbpark p;
    memset(&p, 0, sizeof p);
    CHECK_INT(wbpark_note(&p, 0, 0), WBPARK_NONE);
    CHECK_INT(wbpark_note(&p, 1, 0), WBPARK_NONE);
}

/* A failed reopen stays parked and the next open retries. */
static void failed_reopen_retries(void)
{
    wbpark p;
    memset(&p, 0, sizeof p);
    CHECK_INT(wbpark_note(&p, 0, 1), WBPARK_HIDE);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_SHOW);
    wbpark_failed(&p);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_SHOW);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_NONE);
}

void suite_wbpark(void)
{
    close_then_open();
    flag_decision();
    not_eligible();
    failed_reopen_retries();
}
