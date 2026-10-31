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
    not_eligible();
    failed_reopen_retries();
}
