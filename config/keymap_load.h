/* Read the Amarchy keymap from disk (Amiga only; the parser itself is keymap.c).
 * Shared by the XCON: handler and the Amarchy commodity: one loader. */
#ifndef KEYMAP_LOAD_H
#define KEYMAP_LOAD_H
#include "keymap.h"

#define KM_FILE_MAX 16384

/* ENV:up-term/keymap, ENVARC:up-term/keymap when ENV: has none, else def_text
 * (the compiled-in copy); then keymap.user over it. 1 when a file was read and
 * parsed (0: the compiled-in copy stands). */
int keymap_load(keymap *km, const char *def_text);
/* Parse only a file (the commodity's reload): 1 when the table is usable; km
 * is untouched on failure. Tries ENV: then ENVARC:, then keymap.user. */
int keymap_reload(keymap *km);
/* The same two for a holder of the result only (the Amarchy commodity): the table is rebuilt at
 * its exact size from the binds `keep` accepts, with no [menu] rows (keymap_compact). */
int keymap_load_compact(keymap *km, const char *def_text, int (*keep)(const km_bind *));
int keymap_reload_compact(keymap *km, int (*keep)(const km_bind *));

#endif
