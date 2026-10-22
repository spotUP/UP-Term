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

#endif
