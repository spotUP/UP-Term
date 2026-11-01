/* screenreq: a move of the console to another screen kind (Workbench, own
 * screen, full screen), asked by the Screen menu or a /screen packet from
 * C:UPTerm and done after the IDCMP loop has replied its messages
 * (handler/vtcon_handler.c screen_pending). No OS calls, so the host tests
 * cover it (tests/test_screenreq.c).
 *
 * The kind is kept as kind + 1, so the all-zero value means "no move asked".
 * The handler allocates its con with MEMF_CLEAR and runs screen_pending on
 * every pass of its main loop, also before the first Open packet: when the
 * zero value meant "move to the Workbench screen" (kind 0), every new XCON:
 * process tried that move with no window, failed and called DisplayBeep(0)
 * once (the flash when the Amarchy text menu opened, 2026-10-26).
 * Portable C89.
 */
#ifndef SCREENREQ_H
#define SCREENREQ_H

#define SCREENREQ_NONE (-1) /* screenreq_take: no move asked */

typedef struct {
    int kind1; /* the asked screen kind + 1; 0 = none (the zeroed value) */
} screenreq;

/* No move asked (the same as a zeroed screenreq). */
static void screenreq_clear(screenreq *r)
{
    r->kind1 = 0;
}

/* Ask for a move to screen kind `kind` (0 or more); a later ask replaces it. */
static void screenreq_ask(screenreq *r, int kind)
{
    r->kind1 = kind >= 0 ? kind + 1 : 0;
}

/* The asked kind, once: SCREENREQ_NONE when none, and the request is gone. */
static int screenreq_take(screenreq *r)
{
    int kind = r->kind1 - 1;
    r->kind1 = 0;
    return kind;
}

#endif
