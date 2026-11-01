#include <string.h>
#include "harness.h"
#include "../handler/screenreq.h"

/* A zeroed request (the handler's con comes from AllocVec MEMF_CLEAR) asks
 * for no move: screen_pending ran screen_switch(c, 0) with no window and
 * flashed the screen once per new XCON: process (2026-10-26). */
static void zeroed_is_none(void)
{
    screenreq r;
    memset(&r, 0, sizeof r);
    CHECK_INT(screenreq_take(&r), SCREENREQ_NONE);
    CHECK_INT(screenreq_take(&r), SCREENREQ_NONE);
}

/* The Workbench kind (0) is a real request, taken once. */
static void ask_take_once(void)
{
    screenreq r;
    screenreq_clear(&r);
    screenreq_ask(&r, 0);
    CHECK_INT(screenreq_take(&r), 0);
    CHECK_INT(screenreq_take(&r), SCREENREQ_NONE);
    screenreq_ask(&r, 1);
    screenreq_ask(&r, 2); /* the later ask stands */
    CHECK_INT(screenreq_take(&r), 2);
    CHECK_INT(screenreq_take(&r), SCREENREQ_NONE);
    screenreq_ask(&r, 1);
    screenreq_clear(&r); /* a new Open: no move left over */
    CHECK_INT(screenreq_take(&r), SCREENREQ_NONE);
    screenreq_ask(&r, -1); /* no kind: no move */
    CHECK_INT(screenreq_take(&r), SCREENREQ_NONE);
}

void suite_screenreq(void)
{
    zeroed_is_none();
    ask_take_once();
}
