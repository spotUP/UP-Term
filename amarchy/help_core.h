/* help_core: the pure part of the Amarchy welcome window and Help view (AMARCHY-WELCOME, RL16). The text lives in
 * dist/amarchy.help as sections ([welcome], [help]); placeholders in it are filled from the live keymap so a
 * rebound key reads right: {key:<action>} is the key bound to an action ("menu:root", "win:snap left", "viewer"),
 * {menu:<id>} is the menu path of a tree id ("style.theme" -> "Style > Theme"). Host-tested (tests/test_help_core.c). */
#ifndef AMARCHY_HELP_CORE_H
#define AMARCHY_HELP_CORE_H

#include "../config/keymap.h"
#include "amconf_core.h"
#include "menu_theme.h"

#define HC_UNBOUND "(unbound)"

/* Expand tmpl into out (cap bytes, always NUL-terminated, cut at cap). Key text is mn_spec's ("Super+Shift+Space");
 * an action with no binding reads HC_UNBOUND; an empty menu id reads HC_UNBOUND too. Text that is not a complete
 * placeholder is copied as it is. Returns the length written. */
int hc_expand(const keymap *k, const char *tmpl, char *out, int cap);

/* The body of section [name] in text (len bytes): the lines after its header up to the next header. Returns a pointer
 * into text and sets *blen, or NULL when there is no such section. */
const char *hc_section(const char *text, long len, const char *name, long *blen);

typedef struct hc_line {
    const char *s;
    int len; /* without the newline */
} hc_line;
/* Split body (len bytes) into lines, at most max; returns the count. A final line without newline counts. */
int hc_lines(const char *body, long len, hc_line *out, int max);

/* 1 when the welcome window is due: the key welcome-shown is not "on". */
int hc_should_welcome(const am_prefs *p);
/* Record that the welcome was shown and ticked: welcome-shown = on. */
void hc_mark_shown(am_prefs *p);

/* The welcome window's checkbox row. Space toggles *ticked; Return, Enter or Escape sets *done. Returns 1 when ch was
 * one of those keys, 0 otherwise (the caller keeps it). */
int hc_welcome_key(int ch, int *ticked, int *done);

/* The theme role the inside of the checkbox is filled with: MT_BG when empty, MT_THEME_ACCENT when ticked. The tick was
 * once drawn in MT_SELTEXT, which is the background colour, so a ticked box looked empty (WELCOME-TICK-REDRAW). */
int hc_tick_role(int ticked);

#endif
