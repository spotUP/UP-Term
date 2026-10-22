/* Amarchy commodity, the portable part: a keymap row -> the commodities.library
 * input-description string, and an Intuition qualifier word -> KM_Q_* mask.
 * No OS calls, so tests/test_keymap.c covers it (plan 2026-10-22-amarchy-commodity.md,
 * section 2, Hotkeys). */
#ifndef CXDESC_H
#define CXDESC_H

#include "../config/keymap.h"

/* "rawkey lcommand [rshift] [shift] [alt] 0x35": the raw key code is numeric so
 * a national keymap does not move a binding. 1 on success, 0 when out does not
 * fit. The description only demands the qualifiers the row names; the exact
 * match (a row without shift must not fire on Shift) is cx_qual_mask below. */
int cx_describe(const km_bind *b, char *out, int n);

/* The KM_Q_* mask of an InputEvent qualifier word, built exactly as the
 * handler's keymap_key builds it (so both sides agree on what a press is).
 * 0 when Left Amiga is not down or Control / Right Amiga / repeat is. */
unsigned cx_qual_mask(unsigned ie_qual);

/* 1 when the commodity installs a hotkey filter for this row. The terminal's own
 * actions (scope T: tab:, menuid:, slash:) are the handler's: the commodity has
 * no runner for them, so a filter would only swallow the key when no window of
 * ours has focus. Those rows are left to the application. */
int cx_row_installed(const km_bind *b);

#endif
