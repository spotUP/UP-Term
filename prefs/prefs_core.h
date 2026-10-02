/* prefs_core: the UP-Term Prefs editor's model, with no OS calls, so the
 * rules that decide what reaches the user's file are host-tested
 * (tests/test_prefs.c) and prefs/upprefs.c is only the window.
 *
 * Three rules live here:
 *   - a file the editor cannot hold whole (over UC_MAX_FILE, or more
 *     profiles / keys than upconf has room for) is loaded read-only: a Save
 *     would write back less than it read;
 *   - an edit is staged on a copy of the table and serialised there; the
 *     table the editor holds changes only after the file is in place;
 *   - the file is replaced through a temporary file and renames, so until
 *     the new file is complete and installed the old one stays where the
 *     handler reads it.
 *
 * Portable C89, as config/upconf.c.
 */
#ifndef PREFS_CORE_H
#define PREFS_CORE_H

#include "../config/upconf.h"

/* The fields of one profile as the editor shows them. The strings are the
 * file's own text (an empty one: the key is not written, the handler's
 * built-in stands); the palette is one six-digit hex field per entry. */
typedef struct prefs_fields {
    char font[UC_MAX_VALUE];
    char sb[UC_MAX_VALUE];
    char curcol[UC_MAX_VALUE];
    char fg[UC_MAX_VALUE];
    char bg[UC_MAX_VALUE];
    char selfg[UC_MAX_VALUE];
    char selbg[UC_MAX_VALUE];
    char pal[16][16];
    int cursor;     /* PREFS_CURSOR_* */
    int bell;       /* PREFS_BELL_* */
    int blink, bold, meta_alt, copy_sel, wheel; /* 0 / 1 */
    int completion; /* PREFS_COMPLETE_* */
    char kcmode[UC_MAX_VALUE]; /* KingCON's FNCMODE letters (W L B C S); blank: W */
    int kcinfo;     /* KingCON completion lists .info files too */
} prefs_fields;

/* the choices, in the order the window's cycle gadgets list them */
enum { PREFS_CURSOR_BLOCK, PREFS_CURSOR_UNDERLINE, PREFS_CURSOR_BAR };
enum { PREFS_BELL_NONE, PREFS_BELL_BEEP, PREFS_BELL_VISUAL };
/* Tab completion: unix (common prefix, a list under the line, cycling) or
 * kingcon (KingCON's keys and its selection window;
 * thoughts/shared/research/2026-10-02_kingcon-completion.md) */
enum { PREFS_COMPLETE_UNIX, PREFS_COMPLETE_KINGCON };

/* The built-in values (what a window with no file shows). */
void prefs_defaults(prefs_fields *f);
/* The profile's values; an absent key takes its built-in. */
void prefs_from_conf(prefs_fields *f, const upconf *c, const char *profile);

/* The fields the file cannot hold as typed: 0 when all are good, else
 * PREFS_BAD_PAL + i (palette entry i), PREFS_BAD_SELFG or PREFS_BAD_SELBG. */
#define PREFS_BAD_PAL   1
#define PREFS_BAD_SELFG 17
#define PREFS_BAD_SELBG 18
int prefs_validate(const prefs_fields *f);

/* A theme file (UP-Term's themes drawer, one .conf per theme: one profile section) applied to
 * f: its colours -- fg, bg, cursor-color, selection-fg/bg, palette --
 * replace f's, a colour it does not set is cleared, everything else in f
 * stays. work is scratch for the parse (an upconf is too big for a 68k
 * stack). 1 when the file held a colour, else 0 and f is untouched. */
int prefs_apply_theme(prefs_fields *f, upconf *work, const char *text, long len);

/* 1 when the table has a profile of that name (case-insensitive). */
int prefs_profile_exists(const upconf *c, const char *profile);

/* The profile name as typed: blanks trimmed, [ and ] dropped, cut to cap. */
void prefs_clean_name(const char *in, char *out, int cap);

/* The file as read. got is what a read of up to cap + 1 bytes returned
 * (< 0: the read failed, 0: no file or empty), buf holds it NUL-terminated.
 * PREFS_LOAD_OK and PREFS_LOAD_NONE leave c holding the whole file (empty
 * for NONE) and allow a save; the others leave c empty and forbid one. */
enum {
    PREFS_LOAD_OK,      /* parsed whole */
    PREFS_LOAD_NONE,    /* no file: the defaults stand, a save creates it */
    PREFS_LOAD_TOOBIG,  /* longer than cap: the editor would cut it */
    PREFS_LOAD_LOSSY,   /* more profiles / keys / longer values than upconf holds */
    PREFS_LOAD_ERROR    /* the read failed */
};
int prefs_load(upconf *c, const char *buf, long got, long cap);
/* 1 when a load result lets the editor write the file back. */
int prefs_load_writable(int load_result);

/* Stage a save: *work becomes *cur with the named profile replaced by f, and
 * is written into buf (cap bytes, NUL-terminated on success). The length,
 * or PREFS_STAGE_FULL (the table has no room: a key or the profile would be
 * dropped) or PREFS_STAGE_SIZE (the file would be over cap). *cur is never
 * touched; the caller copies *work over it once the file is in place.
 * f must have passed prefs_validate. */
#define PREFS_STAGE_FULL (-1L)
#define PREFS_STAGE_SIZE (-2L)
long prefs_stage(upconf *work, const upconf *cur, const char *profile,
                 const prefs_fields *f, char *buf, long cap);

/* The file operations the install needs; the Amiga's are DOS calls, the
 * tests' a table in memory. Each answers 1 on success. */
typedef struct prefs_fs {
    void *ctx;
    /* create (or truncate) path and write all len bytes, closed after */
    int (*write_file)(void *ctx, const char *path, const char *buf, long len);
    int (*exists)(void *ctx, const char *path);
    /* path is gone afterwards (1 also when it was not there) */
    int (*remove)(void *ctx, const char *path);
    /* from becomes to; fails when to exists (AmigaDOS Rename) */
    int (*rename)(void *ctx, const char *from, const char *to);
} prefs_fs;

enum {
    PREFS_INSTALL_OK,
    PREFS_INSTALL_WRITE,   /* the new file could not be written: old untouched */
    PREFS_INSTALL_BACKUP,  /* the old file could not be moved to orig: old untouched */
    PREFS_INSTALL_PLACE,   /* the new file could not be renamed in: old put back */
    PREFS_INSTALL_STRANDED /* that, and the old one could not be put back: it is
                            * in orig, the new one in tmp (both kept) */
};
/* Replace path with buf: write tmp, move the old file to orig (its previous
 * backup goes), rename tmp to path. The old file is never deleted, only
 * moved, and only after the new one is complete. */
int prefs_install(const prefs_fs *fs, const char *path, const char *tmp,
                  const char *orig, const char *buf, long len);

#endif
