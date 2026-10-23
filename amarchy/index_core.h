/* Amarchy app index, the portable part (plan 2026-10-23-amarchy-app-index.md, X1). No OS calls, so
 * tests/test_keymap.c covers it. The scanner (amarchy/index.c, tool amarchy-index) writes the cache
 * rows through these; Amarchy reads them back. A cache row is `kind<TAB>label<TAB>path`. */
#ifndef AMARCHY_INDEX_CORE_H
#define AMARCHY_INDEX_CORE_H

#include "../config/keymap.h"
#include "menu_core.h"

#define IX_LABEL 48
#define IX_PATH  256
#define IX_VOL   32
#define IX_KIND_APP  0
#define IX_KIND_GAME 1
#define IX_CACHE_PATH "ENVARC:Amarchy/apps"   /* the scanner writes it, the menus read it */

typedef struct ix_row {
    int  kind;                  /* IX_KIND_APP or IX_KIND_GAME */
    char label[IX_LABEL];
    char path[IX_PATH];
    char suffix[IX_VOL];        /* the volume name, set only on a label shared with another row */
} ix_row;

/* One cache line (no newline needed; a trailing CR/LF is ignored). 1 and *out filled, or 0 for a bad
 * row: not three tab-separated fields, a kind other than app or game, an empty label or path, a field
 * that does not fit. */
int ix_parse_row(const char *line, ix_row *out);

/* `kind<TAB>label<TAB>path<LF>` into out; the length, or -1 when it does not fit n. */
int ix_format_row(const ix_row *r, char *out, int n);

/* The menu label of an icon file name: "Foo Bar.info" is "Foo Bar" (the suffix is any case);
 * a name without .info is kept whole. */
void ix_label_from_name(const char *name, char *out, int n);

/* Does path fall under one of the comma-separated prefixes in skip (the upconf key
 * amarchy-index-skip)? Case-insensitive, blanks around a prefix ignored, an empty prefix ignored.
 * A prefix matches whole path elements only: "Work:Games" skips "Work:Games/x" and "Work:Games",
 * not "Work:Games2". */
int ix_skip_match(const char *skip, const char *path);

/* The volume of a path: "Work:Tools/x" gives "Work" (no colon: empty). */
void ix_volume_of(const char *path, char *out, int n);

/* Sort rows by label (case-insensitive, then path) and give every row whose label another row
 * shares its volume as suffix. */
void ix_sort_dedupe(ix_row *rows, int n);

/* Drop the rows whose label equals (case-insensitive) a [menu] entry one level below node: the
 * keymap row wins. Keeps the order; the new count. */
int ix_drop_shadowed(const keymap *k, const char *node, ix_row *rows, int n);

/* The cache as the menu shows it (plan X3): apps first, then games, each sorted with volume suffixes,
 * the rows a [menu] entry shadows already dropped. */
#define IX_MAX_ROWS 256
#define IX_LEVEL_MAX (MN_MAX_ITEMS + IX_MAX_ROWS)   /* the most items a level can hold */
typedef struct ix_index {
    int    n, napps;            /* rows[0..napps) are apps, rows[napps..n) are games */
    ix_row rows[IX_MAX_ROWS];
} ix_index;

/* The cache text (rows `kind<TAB>label<TAB>path`, bad rows skipped) into x. Empty or NULL text gives
 * no rows. The [menu] rows of the "apps" and "games" nodes in k win over a row with their label. */
void ix_load_text(const keymap *k, ix_index *x, const char *text);
/* The cache file at path, read whole and given to ix_load_text; a missing file gives no rows. */
void ix_load_file(const keymap *k, ix_index *x, const char *path);

/* The entries of one level, as mn_children, extended with the index: the "apps" node lists the app
 * rows after the [menu] ones, the "games" node lists the game rows, and the root gets an Apps or
 * Games group when only the index fills it. With a non-empty filter the root also lists every index
 * row, so typing there finds apps and games at any depth. Index items have row MN_ROW_INDEX and ix
 * set. Count; at most max. */
int ix_level(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
             int max);

/* The path an index item launches (the tool or project, no .info), copied into out. 1, or 0 when it is
 * not an index item or its row is not in x. */
int ix_launch_path(const ix_index *x, const mn_item *it, char *out, int n);

#endif
