/* The handler request port (decision 2026-10-22; portable half and codes in keymap_act.h).
 * Each UP-Term window that has had focus owns a public message port "UPTERM.req.<hex>"; the
 * public semaphore UPTERM.active records the name of the focused window and of the one focused
 * before it. The quick menu runs in an XCON: window of its own, so the terminal it means is the
 * PREVIOUS one: upreq_send puts the request to that port and waits for the answer.
 * Lifetime: the port is added when the window first gains focus and removed (RemPort under
 * Forbid, queued requests answered UPREQ_NOPORT) when the window's process ends. A sender
 * finds and puts under one Forbid, so it never writes to a port that is going. Amiga only. */
#ifndef UPREQ_H
#define UPREQ_H

#include <exec/ports.h>
#include "keymap_act.h"

#define UPREQ_NAMELEN 24

typedef struct upreq_msg {
    struct Message msg;
    upreq_body     b;
} upreq_msg;

/* Window side. open: create and publish the port, name = a buffer that lives as long as the
 * port; 0 when out of memory. */
struct MsgPort *upreq_open(char name[UPREQ_NAMELEN]);
upreq_msg *upreq_get(struct MsgPort *mp);
void upreq_reply(upreq_msg *m, long rc);
void upreq_close(struct MsgPort *mp);

/* Sender side: the answer (UPREQ_*); UPREQ_NOPORT when no terminal window is there to ask. */
long upreq_send(const upreq_body *r);

#endif
