/* dopusenv_core: the toolbar path in a Directory Opus 5.104 environment file (DOpus5:Environment/default and workbench), plan
 * s91 toolbar. Pure: no Amiga calls, no malloc; the caller reads and writes the file.
 * Format (measured on the stock files): IFF "FORM" <be32 size> "EPUS", then chunks <id> <be32 size> <body> + a pad byte when size is odd.
 * TBAR's body is a bare NUL ended C string (stock "PROGDIR:Buttons/toolbar", size 24 incl. NUL), no fixed field, no struct.
 * New listers take their toolbar bank from it. */
#ifndef DOPUSENV_CORE_H
#define DOPUSENV_CORE_H
#include <stddef.h>
#include <stdint.h>

#define DE_PATHMAX   160                         /* longest toolbar path handled, NUL included */
#define DE_STOCK     "PROGDIR:Buttons/toolbar"   /* what upstream ships */
#define DE_BANK_A    "DOpus5:Buttons/Patina-toolbar-"
#define DE_BANK_B    "PROGDIR:Buttons/Patina-toolbar-"
#define DE_BANK_C    "DOpus5:Buttons/Patina-"   /* flattened copy of a user bank: Patina-<bank>-<set> (dt_bank_name) */

enum { DE_OK = 0, DE_BAD = -1 /* not FORM EPUS, truncated, chunk past the end */, DE_NOTBAR = -2, DE_FULL = -3 /* output or path too small */ };

/* Copies TBAR's string to out (NUL ended, at most cap bytes). DE_OK, DE_BAD, DE_NOTBAR, DE_FULL (string does not fit or is not NUL ended). */
int de_tbar_get(const uint8_t *buf, size_t len, char *out, size_t cap);

/* Writes buf with TBAR's body replaced by path + NUL (+ pad to even) to out, FORM size fixed, every other byte unchanged.
 * Returns the new length (>= 0), or DE_BAD / DE_NOTBAR / DE_FULL; out is untouched on failure. A missing TBAR is not added. */
long de_tbar_set(const uint8_t *buf, size_t len, const char *path, uint8_t *out, size_t outcap);

/* 1 when cur is our bank (DE_BANK_A / B / C prefix, any case). */
int de_is_ours(const char *cur);
/* APPLY: 1 only when cur is the upstream default or one of our banks; a user's own toolbar path is left alone. */
int de_may_replace(const char *cur);
/* RESTORE: 1 only when cur is one of our banks (then write DE_STOCK back). */
int de_may_restore(const char *cur);
/* The user's bank to flatten from (cur when custom, saved when cur is our flattened copy), or NULL: stock flow. */
const char *de_orig_pick(const char *cur, const char *saved);

#endif
