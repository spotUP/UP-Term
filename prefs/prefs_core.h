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
    char fallback[UC_MAX_VALUE]; /* font-fallback: outline font for glyphs the font lacks */
    char screenmode[UC_MAX_VALUE];  /* screen-mode: 0xID ("" = the Workbench's) */
    char screendepth[UC_MAX_VALUE]; /* screen-depth: n ("" = 4, or 8 on a card) */
    char sb[UC_MAX_VALUE];
    char curcol[UC_MAX_VALUE];
    char fg[UC_MAX_VALUE];
    char bg[UC_MAX_VALUE];
    char selfg[UC_MAX_VALUE];
    char selbg[UC_MAX_VALUE];
    char term[32];                  /* term: TERM for programs vsh starts in the window ("": vtcon) */
    char colors[8];                 /* colors: rgb | 256 ("": not told) -- COLORTERM for those programs */
    char linkopen[UC_MAX_VALUE];    /* link-open: the command for an OSC 8 link, %s the URL */
    char pal[16][16];
    int cursor;     /* PREFS_CURSOR_* */
    int bell;       /* PREFS_BELL_* */
    int blink, bold, meta_alt, copy_sel, wheel, reflow; /* 0 / 1 */
    int scrollbar;  /* the scroll bar in a sizable window's border (default on) */
    int completion; /* PREFS_COMPLETE_* */
    int screen;     /* PREFS_SCREEN_*: screen = workbench | own | fullscreen */
    int aspect;     /* font-aspect: the font fitted to the screen's aspect (on by default) */
    int backspace_bs; /* backspace = bs: the key sends ^H; 0: del, ^? (the default) */
    int clipboard;  /* PREFS_CLIP_*: program-clipboard, what OSC 52 may do */
    char kcmode[UC_MAX_VALUE]; /* KingCON's FNCMODE letters (W L B C S); blank: W */
    int kcinfo;     /* KingCON completion lists .info files too */
    int kccache;    /* KingCON's directory cache (DIRCACHE; on by default) */
    int amiga_keys; /* amiga-keys: Shift+Left / Right send Home / End (on by default) */
    int hlcat;      /* highlight-cat: vsh's cat is hl -p (on by default; see prefs_hlcat_text) */
} prefs_fields;

/* the choices, in the order the window's cycle gadgets list them */
enum { PREFS_CURSOR_BLOCK, PREFS_CURSOR_UNDERLINE, PREFS_CURSOR_BAR };
enum { PREFS_BELL_NONE, PREFS_BELL_BEEP, PREFS_BELL_VISUAL };
/* Tab completion: unix (common prefix, a list under the line, cycling) or
 * kingcon (KingCON's keys and its selection window;
 * thoughts/shared/research/2026-10-02_kingcon-completion.md) */
enum { PREFS_COMPLETE_UNIX, PREFS_COMPLETE_KINGCON };
enum { PREFS_SCREEN_WORKBENCH, PREFS_SCREEN_OWN, PREFS_SCREEN_FULL };
/* OSC 52: programs may set the clipboard (the default), set and read it,
 * or neither */
enum { PREFS_CLIP_WRITE, PREFS_CLIP_READ_WRITE, PREFS_CLIP_OFF };

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

/* Stage a save: *work becomes *cur with the editor's keys of the named
 * profile set from f, each in place (an empty string field deletes its key);
 * every other key of the profile, the other profiles and the comments stay
 * as read. *work is written into buf (cap bytes, NUL-terminated on
 * success). The length, or PREFS_STAGE_LOSSY (*cur is not the whole
 * file), PREFS_STAGE_FULL (the table has no room: a key or the profile would be
 * dropped) or PREFS_STAGE_SIZE (the file would be over cap). *cur is never
 * touched; the caller copies *work over it once the file is in place.
 * f must have passed prefs_validate. */
#define PREFS_STAGE_FULL (-1L)
#define PREFS_STAGE_SIZE (-2L)
/* *cur did not hold the whole file (its overflow is set: the window's own
 * read keeps what fits): writing it back would lose the rest */
#define PREFS_STAGE_LOSSY (-3L)
long prefs_stage(upconf *work, const upconf *cur, const char *profile,
                 const prefs_fields *f, char *buf, long cap);

/* The switch vsh's startup file reads (dist/vshrc): a variable of the
 * profile drawers (ENV:up-term/highlight-cat, and ENVARC:'s copy) holding
 * "on" or "off", written beside the profile file by every save; absent
 * means on. */
#define PREFS_HLCAT_VAR "highlight-cat"
const char *prefs_hlcat_text(int on);

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

/* ---- W30: the themes drawer ------------------------------------------------
 *
 * The kit installs the themes in PREFS_THEMES_DIR (dist/install.dos) and
 * copies it to PREFS_THEMES_ENV for the boot. Every theme requester (UP-Term
 * Prefs' Theme..., the window's Settings > Theme...) and every theme looked
 * up by name (/theme NAME, /theme's list) asks prefs_theme_drawer where to
 * look, so none of them names the drawer itself. */
#define PREFS_THEMES_DIR "ENVARC:up-term/themes"
#define PREFS_THEMES_ENV "ENV:up-term/themes"

/* The drawer to open: the first that fs->exists finds of
 *   1. the drawer of `given` -- the theme file the window or the editor has
 *      now (0 or "": none; a bare name has no drawer and is skipped),
 *   2. PREFS_THEMES_DIR, 3. PREFS_THEMES_ENV,
 *   4. `home`'s "themes" -- the program's own drawer (0 or "": none): the
 *      unpacked kit's Files/themes, the rig's VTC:themes,
 *   5. `home` itself;
 * "" when none is there (a requester then opens where it likes: the
 * symptom of W30, a missing kit drawer). out: cap bytes. */
void prefs_theme_drawer(const prefs_fs *fs, const char *given, const char *home, char *out, int cap);

/* The theme file /theme NAME means: name itself when it is a path (holds a
 * ':' or a '/'), else name in drawer; ".conf" added when it does not end
 * so (any case). out: cap bytes, cut to fit. */
void prefs_theme_file(const char *drawer, const char *name, char *out, int cap);

/* A drawer's listing to theme names: file (a name from the listing) goes
 * into names (*len bytes used of cap, NUL after each name) when it ends in
 * ".conf" (any case), without it, in order ignoring case; a name already
 * there (ignoring case) is not added again. 1 added, 0 not a theme or a
 * repeat, -1 no room (names unchanged). */
int prefs_theme_add(char *names, int *len, int cap, const char *file);

/* Which of names (n theme names, NUL-separated, as prefs_theme_add makes
 * them) file is -- a path or a name, with ".conf" or without, any case:
 * its index, or -1 (0 or "": -1). /theme's list marks the window's theme. */
int prefs_theme_index(const char *names, int n, const char *file);

#endif
