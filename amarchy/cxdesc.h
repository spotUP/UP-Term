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
 * 0 when Left Amiga is not down or Control / Right Amiga / repeat is. */
unsigned cx_qual_mask(unsigned ie_qual);

/* 1 when the commodity installs a hotkey filter for this row. The terminal's own
 * actions (scope T: tab:, menuid:, slash:) are the handler's: the commodity has
 * no runner for them, so a filter would only swallow the key when no window of
 * ours has focus. Those rows are left to the application. */
int cx_row_installed(const km_bind *b);

#endif
