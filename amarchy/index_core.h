/* Amarchy app index, the portable part (plan 2026-10-23-amarchy-app-index.md, X1). No OS calls, so
 * tests/test_keymap.c covers it. The scanner (amarchy/index.c, tool amarchy-index) writes the cache
 * rows through these; Amarchy reads them back. A cache row is `kind<TAB>label<TAB>path`. */
#ifndef AMARCHY_INDEX_CORE_H
#define AMARCHY_INDEX_CORE_H

#include "../config/keymap.h"

#define IX_LABEL 48
#define IX_PATH  256
#define IX_VOL   32
#define IX_KIND_APP  0
#define IX_KIND_GAME 1

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

#endif
