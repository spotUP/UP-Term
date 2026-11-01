/* Amarchy commodity, the portable part: a keymap row -> the commodities.library
 * input-description string, and an Intuition qualifier word -> KM_Q_* mask.
 * No OS calls, so tests/test_keymap.c covers it (plan 2026-10-22-amarchy-commodity.md,
 * section 2, Hotkeys). */
#ifndef CXDESC_H
#define CXDESC_H

#include "../config/keymap.h"

/* The filter's description string: the class and the one qualifier every row needs. The raw code
 * and the qualifier masks come from cx_ixspec through SetFilterIX, so a national keymap does not
 * move a binding. */
#define CX_FILTER_DESC "rawkey lcommand"

/* The commodities IX fields of a row: ix_Code, ix_CodeMask, ix_Qualifier, ix_QualMask. */
typedef struct {
    unsigned short code, code_mask, qual, qual_mask;
} cx_ixspec_t;
void cx_ixspec(const km_bind *b, cx_ixspec_t *x);

/* The KM_Q_* mask of an InputEvent qualifier word, built exactly as the
 * handler's keymap_key builds it (so both sides agree on what a press is).
 * 0 when Left Amiga is not down or Right Amiga / repeat is. */
unsigned cx_qual_mask(unsigned ie_qual);

/* 1 when the row (its raw code, its KM_Q_* mask) takes this key event: the exact code, key
 * down or up, and the row's qualifier mask equal to cx_qual_mask of the event's qualifiers. */
int cx_row_takes(unsigned short row_code, unsigned row_qual, unsigned ie_code, unsigned ie_qual);

/* The command line that runs the amarchy-menu helper from Amarchy's own directory: a quoted full
 * path ('"<dir>amarchy-menu" <args>'; a '/' is added unless dir ends in ':' or '/'). 0 when it
 * does not fit in cap bytes or dir is empty. The bare name is not on every command path. */
int cx_helper_cmd(char *out, int cap, const char *dir, const char *prog, const char *args);

/* The menu style (UC_MENU_*) a menu hotkey opens: the general menu-style setting, except while the active
 * window is an UP-Term terminal, where the menu-in-terminal setting (UC_TERMMENU_*) decides; a full-screen
 * terminal always gets the text menu. */
int cx_menu_style(int general, int term_setting, int in_terminal, int fullscreen);

/* The command for one of the two menu actions (KA_MENU, KA_VIEWER) under a style (UC_MENU_*,
 * from cx_menu_style): the graphical program (amarchy-gui) or the text one
 * (amarchy-menu). args is "menu <node>" (node empty =
 * root) or "viewer". *text is set to 1 when the program is the text one (it needs an XCON: window),
 * 0 for the graphical one (it opens its own). 0 when the kind is neither or it does not fit. */
int cx_menu_cmd(char *out, int cap, const char *dir, int kind, const char *node, int style, int *text);

/* 1 when the commodity installs a hotkey filter for this row. The terminal's own
 * actions (scope T: tab:, menuid:, slash:) are the handler's: the commodity has
 * no runner for them, so a filter would only swallow the key when no window of
 * ours has focus. Those rows are left to the application. */
int cx_row_installed(const km_bind *b);

#endif
