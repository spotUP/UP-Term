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
 * the rows a [menu] entry shadows already dropped. Every row of the cache is kept, however many: the
 * rows are on the heap. An ix_index starts zeroed (static, or {0}); ix_free releases its rows. */
typedef struct ix_index {
    int    n, napps;            /* rows[0..napps) are apps, rows[napps..n) are games */
    ix_row *rows;
} ix_index;

/* The cache text (rows `kind<TAB>label<TAB>path`, bad rows skipped) into x, replacing the rows it
 * held. Empty or NULL text, or no memory, gives no rows. The [menu] rows of the "apps" and "games"
 * nodes in k win over a row with their label. */
void ix_load_text(const keymap *k, ix_index *x, const char *text);
/* The cache file at path, read whole and given to ix_load_text; a missing file gives no rows. */
void ix_load_file(const keymap *k, ix_index *x, const char *path);
void ix_free(ix_index *x);

/* The most items one level can hold with x loaded: the [menu] rows, every index row, and the Apps and
 * Games groups the root may get. The size of the item and filter arrays given to ix_level. */
int ix_level_cap(const ix_index *x);

/* The entries of one level, as mn_children, extended with the index: the "apps" node lists the app
 * rows after the [menu] ones, the "games" node lists the game rows, and the root gets an Apps or
 * Games group when only the index fills it. With a non-empty filter the root also lists every index
 * row, so typing there finds apps and games at any depth. Index items have row MN_ROW_INDEX and ix
 * set. Count; at most max. */
int ix_level(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
             int max);
/* ix_level with the terminal-rows filter of menu_core.h (mn_children_t): term = 0 hides the rows
 * and groups that need a terminal window. */
int ix_level_t(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
               int max, int term);

/* The path an index item launches (the tool or project, no .info), copied into out. 1, or 0 when it is
 * not an index item or its row is not in x. */
int ix_launch_path(const ix_index *x, const mn_item *it, char *out, int n);

/* Scan progress record (RESCAN-FEEDBACK, AMARCHY-INDEX-WHERE). amarchy-index keeps one of these in
 * public memory (a semaphore named IX_PROG_NAME, amarchy/index.c) that outlives the scanner task, so
 * a scan that dies at a Software Failure still names the path it was on. The scanner copies the path
 * in before each drawer it enters and each icon it opens; no disk writes. Pure data and text here; the
 * OS side only allocates and finds it. */
/* The record is never freed, so one left by an older build lives until reboot. The layout version is in
 * the semaphore name (IX_PROG_NAME) and in the record (version): a build never writes into a record of
 * another layout, it adds its own under its own name; the older name is IX_PROG_NAME_OLD (layout 1, no
 * version word). */
#define IX_PROG_VERSION  2
#define IX_PROG_NAME     "Amarchy.index.2"
#define IX_PROG_NAME_OLD "Amarchy.index"
enum { IX_ST_IDLE = 0, IX_ST_VOLUMES, IX_ST_WALK, IX_ST_ICON, IX_ST_WRITE, IX_ST_DONE, IX_ST_STOPPED,
       IX_ST_WRITE_FAILED };
typedef struct ix_prog {
    long version;               /* IX_PROG_VERSION; first, so a reader checks it before any other field */
    long seq;                   /* counts every update: a changing seq means the scanner is moving */
    int  stage;                 /* IX_ST_* */
    long dirs, icons;           /* drawers entered, icons opened so far */
    long napps, ngames, nvols;  /* set by ix_prog_done */
    char path[IX_PATH];         /* the drawer or icon file being handled; no slash added for a volume root */
} ix_prog;

/* A new scan: everything cleared, stage set to IX_ST_VOLUMES. */
void ix_prog_begin(ix_prog *p);
/* Note stage and path (cut to fit); a WALK counts a drawer, an ICON counts an icon. */
void ix_prog_note(ix_prog *p, int stage, const char *path);
/* The scan finished: stage DONE with the counts. */
void ix_prog_done(ix_prog *p, long napps, long ngames, long nvols);
/* The scan was stopped (Ctrl-C): stage STOPPED, the path where it stopped is kept. */
void ix_prog_stopped(ix_prog *p);
/* The cache write failed (temp file, write or rename): stage WRITE_FAILED, the path is kept. */
void ix_prog_write_failed(ix_prog *p);
/* The line --where prints when it finds only a record of another layout (an older build's, left in
 * memory until reboot); its fields are not read. Length, or -1 when it does not fit n. */
int ix_prog_other_text(char *out, int n);
/* The stage as a word: volumes, walk, icon, write, done, stopped, idle. */
const char *ix_stage_name(int stage);
/* The text `amarchy-index --where` prints (several lines, each ending in LF), into out; the length,
 * or -1 when it does not fit n. task: 1 the scanner task still exists, 0 it is gone. moving: 1 seq
 * changed while --where watched, 0 it did not. A task that exists, is not done and does not move is
 * reported as stuck or stopped (a requester waits for a click); one gone and not done as died. */
int ix_prog_text(const ix_prog *p, int task, int moving, char *out, int n);

/* Is handler (the DOS device's handler name, e.g. "L:smb2-handler", path allowed) a network file system?
 * Measured on the A1200 (thoughts/spot/term-findings/index-skip-network.md): the SMB shares have handler
 * L:smb2-handler, no startup message and no exec device, disk type "SMB\2"; a hard disk has a startup
 * message naming scsi.device. The handler name is the reliable signal, so a short list of known network
 * handlers (prefix of the file name: smb, nfs, ftp) decides. NULL or empty: 0. */
int ix_net_handler(const char *handler);
/* Does a device run the same handler task as a volume? A volume with no task (0, not mounted) matches
 * nothing: 0 is also the task of every device that has not started. */
int ix_same_handler_task(const void *vol_task, const void *dev_task);

/* Is the volume scanned? A local volume always; a network one only when its name is in volumes, the
 * comma-separated upconf key amarchy-index-volumes (case-insensitive, blanks and a trailing colon
 * around a name ignored). */
int ix_volume_wanted(const char *vol, int network, const char *volumes);

#endif
