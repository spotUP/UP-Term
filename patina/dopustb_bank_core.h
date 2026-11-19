/* dopustb_bank_core: flatten a user's own Directory Opus 5 button bank (plan s91 custom bank). Pure: no Amiga calls, no malloc.
 * A button whose first function is an internal command (instruction type 0) that we have a flat brush for gets its state-0
 * image path pointed at dopus5:images/Patina/<set>/<Name>.small, whatever custom .info it showed before. Every image path that
 * still points at the stock drawer (dopus5:images/, any case) gets the same drawer rewrite the build-time generator
 * (tools/gen_dopus_toolbar.py rewrite_bank) applies. Everything else is copied byte for byte.
 * Layout (FORM EPUS, big endian, chunks padded to even): BTNW (flags u32 at body+128), then per button BUTN + N x FUNC.
 * BUTN body = 14 byte header (byte 7 = N states) + N pairs of NUL strings (image, label). FUNC body = 22 byte header, then
 * instructions string NUL + u16 type (0 = internal command, 1 = AmigaDOS ...). Type codes are inferred from banks, not from
 * the DOpus source. */
#ifndef DOPUSTB_BANK_CORE_H
#define DOPUSTB_BANK_CORE_H
#include <stddef.h>

enum { DTB_BAD = -1 /* not FORM EPUS, truncated, chunk past the end, empty set */, DTB_FULL = -2 /* out too small */ };

/* Writes the flattened bank to out (at most cap bytes). Returns the new length (> 0) or DTB_BAD / DTB_FULL; out is untouched on
 * failure. A button with a bad layout is copied unchanged. BTNW gets Borderless only when at least one button was rewritten. */
long dt_bank_flatten(const unsigned char *in, long len, const char *set, unsigned char *out, long cap);

/* "DOpus5:Buttons/Patina-bank-<basename of tbar>-<set>" into out (NUL ended). The basename (after the last ':' or '/') is cut to
 * 40 characters. 0, or -1 when tbar/set is empty or out is too small. de_is_ours (dopusenv_core) accepts the result. */
int dt_bank_name(const char *tbar, const char *set, char *out, int cap);

/* The stock image base name (no ".small") for a lowercase command word, or NULL. */
const char *dt_bank_image(const char *cmd);
/* Table access for tests: entry i (0 .. count-1) command and image; NULL past the end. */
const char *dt_bank_entry(int i, const char **image);

#endif
