/* From a keymap binding to what the terminal runs (plan 2026-10-22-amarchy-commodity,
 * section 3). Portable, no OS calls: the handler (vtcon_handler.c) does the running,
 * this file only decides WHAT a binding means, so a host suite can test it.
 *
 *   tab:new|close|next|prev, tab:goto N, menuid:MENU_NAME -> a MENU_* id, run by
 *                    the same menu_run the menu bar uses (one code path)
 *   slash:/cmd args  -> the typed command, run by the same slash_run as a typed line
 *   the commodity's own actions (menu:, viewer, run:, rexx:, open:) -> KACT_DEFER:
 *                    not the terminal's to run, the key goes on to the program */
#ifndef KEYMAP_ACT_H
#define KEYMAP_ACT_H

#include "../config/keymap.h"

enum { KACT_NONE = 0, KACT_MENU, KACT_SLASH, KACT_DEFER };

typedef struct kact {
    int  kind;        /* KACT_* */
    long id;          /* KACT_MENU: the MENU_* id */
    const char *line; /* KACT_SLASH: the command line (points into the binding) */
} kact;

/* The id of a MENU_* name ("MENU_COPY"), 0 when there is none. */
long kact_menu_id(const char *name);
/* The name of an id (not a profile or goto id), 0 when there is none. */
const char *kact_menu_name(long id);
/* What a binding means. KACT_NONE: refused (an unknown menuid name, a goto
 * beyond 9, a slash that is not a command). */
int  kact_resolve(const km_bind *b, kact *out);
/* The keyboard shortcut the menu bar shows for an item: the one letter of a
 * plain Super binding (no other qualifier) whose action is menu id `id`.
 * out = that character and a NUL; 1 when there is one. None when no such
 * binding exists, or when the same key has a Shift twin: Intuition ignores
 * Shift in a command key, so the menu would take the Shift key's press. */
int  kact_comm_key(const keymap *k, long id, char out[2]);

/* The handler request port (decision 2026-10-22): the quick menu (amarchy-menu, its own XCON
 * window) hands a tab:, menuid: or slash: leaf to the terminal by NAME, and the handler runs it
 * through the same kact_resolve + menu_do / slash_run a key binding uses, so it works with the
 * chord unbound. The body below is the portable half of the message (handler/upreq.c wraps it
 * in an exec Message). A request is a version and the action as the keymap file writes it. */
#define UPREQ_VERSION 2
#define UPREQ_ACTION  (KM_ARG + 8)
#define UPREQ_ANSWER  80
/* FAILED: the action ran and refused (a slash command's error), the reason in answer.
 * NEEDARG: a slash command that takes an argument came without one; answer holds its usage
 * ("bg: RRGGBB | none"), the sender asks for the value and sends the action again with it. */
enum { UPREQ_OK = 0, UPREQ_UNKNOWN = 5, UPREQ_BADVERSION = 10, UPREQ_NOTMINE = 15,
       UPREQ_NOWINDOW = 20, UPREQ_NOPORT = 25, UPREQ_FAILED = 30, UPREQ_NEEDARG = 35 };
typedef struct upreq_body {
    long version;               /* UPREQ_VERSION */
    char action[UPREQ_ACTION];  /* "tab:new", "menuid:MENU_COPY", "slash:/theme" */
    long rc;                    /* the answer: UPREQ_* */
    char answer[UPREQ_ANSWER];  /* why, for FAILED and NEEDARG: the first line, no newline */
} upreq_body;

/* Validate a request and turn it into what a binding of that action resolves to. UPREQ_OK with
 * *out set; otherwise the rc to reply (junk never crashes: no NUL, empty, unknown prefix or
 * argument is UPREQ_UNKNOWN, the commodity's own kinds are UPREQ_NOTMINE). `tmp` is scratch
 * the slash line points into and must outlive *out. */
int upreq_resolve(const upreq_body *r, kact *out, km_bind *tmp);

/* The sender's half: a request for a [menu] or [bind] leaf of kind tab:, menuid: or slash:.
 * 1 when it fits and the kind goes to a terminal; 0 otherwise (nothing to send). */
int upreq_fill(upreq_body *r, int kind, const char *arg);

/* The window's verdict on a slash request it ran: slash_run's result (1 ran, 2 refused), its
 * answer text and whether the line lacked a required argument. Sets r->answer and returns the
 * rc to reply: a refusal is never UPREQ_OK, so the sender can say why instead of exiting. */
long upreq_slash_answer(upreq_body *r, int ran, const char *ans, int missing_arg);

/* The sender's half again: the request that runs action `act` ("slash:/bg") with `value`
 * appended as its argument. 0 when it does not fit or the value is empty. */
int upreq_with_arg(upreq_body *r, const char *act, const char *value);

/* The focus record behind upreq_send (handler/upactive.c keeps it in UPTERM.active): the
 * request port ids of the live UP-Term windows, most recently focused first, 0 after the last.
 * Focus moves a window to the front; a window that goes is taken out and the rest close up, so
 * the window focused before stays known however many windows came and went in between. */
#define UPMRU_N 12
void upmru_focus(unsigned long l[UPMRU_N], unsigned long id);
void upmru_forget(unsigned long l[UPMRU_N], unsigned long id);
/* The window the quick menu means: the one focused before the front one (the menu); 0 none. */
unsigned long upmru_prev(const unsigned long l[UPMRU_N]);
/* The public port name of request port id: "UPTERM.req." and eight hex digits. */
void upreq_portname(unsigned long id, char out[24]);

#endif
