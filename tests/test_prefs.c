/* The Prefs editor's model (prefs/prefs_core.c): what a load lets the editor
 * write back, a save staged off the live table, and the file replaced
 * without a moment where the user's profiles are gone. No OS calls: the
 * install runs against a file table in memory. */
#include <stdio.h>
#include <string.h>
#include "../prefs/prefs_core.h"
#include "harness.h"

/* ---- a file system in memory, with one operation made to fail ---------------- */

#define FS_FILES 6
typedef struct {
    char name[FS_FILES][48];
    char data[FS_FILES][64];
    int used[FS_FILES];
    const char *fail_write;   /* writing this path fails */
    const char *fail_rename;  /* renaming onto this path fails */
    const char *fail_remove;  /* removing this path fails */
} memfs;

static int mf_find(memfs *m, const char *path)
{
    int i;
    for (i = 0; i < FS_FILES; i++)
        if (m->used[i] && !strcmp(m->name[i], path))
            return i;
    return -1;
}

static const char *mf_get(memfs *m, const char *path)
{
    int i = mf_find(m, path);
    return i < 0 ? "<absent>" : m->data[i];
}

static void mf_put(memfs *m, const char *path, const char *data)
{
    int i = mf_find(m, path);
    if (i < 0)
        for (i = 0; i < FS_FILES && m->used[i]; i++)
            ;
    m->used[i] = 1;
    strcpy(m->name[i], path);
    strcpy(m->data[i], data);
}

static int mf_write(void *ctx, const char *path, const char *buf, long len)
{
    memfs *m = (memfs *)ctx;
    char tmp[64];
    if (m->fail_write && !strcmp(m->fail_write, path)) {
        mf_put(m, path, "<partial>"); /* a short write leaves a stub behind */
        return 0;
    }
    memcpy(tmp, buf, (size_t)len);
    tmp[len] = 0;
    mf_put(m, path, tmp);
    return 1;
}

static int mf_exists(void *ctx, const char *path)
{
    return mf_find((memfs *)ctx, path) >= 0;
}

static int mf_remove(void *ctx, const char *path)
{
    memfs *m = (memfs *)ctx;
    int i = mf_find(m, path);
    if (m->fail_remove && !strcmp(m->fail_remove, path))
        return i < 0;
    if (i >= 0)
        m->used[i] = 0;
    return 1;
}

static int mf_rename(void *ctx, const char *from, const char *to)
{
    memfs *m = (memfs *)ctx;
    int i = mf_find(m, from);
    if (i < 0 || mf_find(m, to) >= 0)
        return 0; /* AmigaDOS: no source, or the target exists */
    if (m->fail_rename && !strcmp(m->fail_rename, to))
        return 0;
    strcpy(m->name[i], to);
    return 1;
}

static void mf_init(memfs *m, prefs_fs *fs)
{
    memset(m, 0, sizeof(*m));
    fs->ctx = m;
    fs->write_file = mf_write;
    fs->exists = mf_exists;
    fs->remove = mf_remove;
    fs->rename = mf_rename;
}

#define P "ENV:up-term/up-term"
#define T "ENV:up-term/up-term.new"
#define O "ENV:up-term/up-term.orig"

static void install_cases(void)
{
    memfs m;
    prefs_fs fs;

    /* first save: no old file, nothing to back up */
    mf_init(&m, &fs);
    CHECK_INT(prefs_install(&fs, P, T, O, "new", 3), PREFS_INSTALL_OK);
    CHECK_STR(mf_get(&m, P), "new");
    CHECK(!mf_exists(&m, T));
    CHECK(!mf_exists(&m, O));

    /* a save over a file: the old one becomes orig, its old backup goes */
    mf_init(&m, &fs);
    mf_put(&m, P, "old");
    mf_put(&m, O, "older");
    mf_put(&m, T, "stale"); /* left by an interrupted save */
    CHECK_INT(prefs_install(&fs, P, T, O, "new", 3), PREFS_INSTALL_OK);
    CHECK_STR(mf_get(&m, P), "new");
    CHECK_STR(mf_get(&m, O), "old");
    CHECK(!mf_exists(&m, T));

    /* a failed write leaves the user's file exactly as it was */
    mf_init(&m, &fs);
    mf_put(&m, P, "old");
    m.fail_write = T;
    CHECK_INT(prefs_install(&fs, P, T, O, "new", 3), PREFS_INSTALL_WRITE);
    CHECK_STR(mf_get(&m, P), "old");
    CHECK(!mf_exists(&m, T));

    /* the old backup cannot be removed (protected): the file is not touched,
     * where the old code deleted the file and then failed the rename */
    mf_init(&m, &fs);
    mf_put(&m, P, "old");
    mf_put(&m, O, "older");
    m.fail_remove = O;
    CHECK_INT(prefs_install(&fs, P, T, O, "new", 3), PREFS_INSTALL_BACKUP);
    CHECK_STR(mf_get(&m, P), "old");
    CHECK_STR(mf_get(&m, O), "older");
    CHECK(!mf_exists(&m, T));

    /* nothing can be renamed onto the file (neither the new one nor the old
     * one back): both versions survive, the old in orig, the new in tmp --
     * where the old code had already deleted the file and then deleted tmp */
    mf_init(&m, &fs);
    mf_put(&m, P, "old");
    m.fail_rename = P;
    CHECK_INT(prefs_install(&fs, P, T, O, "new", 3), PREFS_INSTALL_STRANDED);
    CHECK_STR(mf_get(&m, O), "old");
    CHECK_STR(mf_get(&m, T), "new");
    CHECK(!mf_exists(&m, P));
}

/* a one-shot rename failure: the install's rename of tmp fails, the restore
 * of the old file succeeds */
static int oneshot_fired;
static int mf_rename_once(void *ctx, const char *from, const char *to)
{
    if (!oneshot_fired && !strcmp(from, T)) {
        oneshot_fired = 1;
        return 0;
    }
    return mf_rename(ctx, from, to);
}

static void install_restore_case(void)
{
    memfs m;
    prefs_fs fs;
    mf_init(&m, &fs);
    fs.rename = mf_rename_once;
    oneshot_fired = 0;
    mf_put(&m, P, "old");
    CHECK_INT(prefs_install(&fs, P, T, O, "new", 3), PREFS_INSTALL_PLACE);
    CHECK_STR(mf_get(&m, P), "old");
    CHECK(!mf_exists(&m, T));
}

/* ---- load ------------------------------------------------------------------------ */

static upconf conf, work, before;

/* "[profile pN]\nfont = fN\n" for N = 0..count-1 (count <= 10) into buf */
static long profiles_text(char *buf, int count)
{
    static const char tmpl[] = "[profile pN]\nfont = fN\n";
    long n = 0;
    int i, k;
    for (i = 0; i < count; i++)
        for (k = 0; tmpl[k]; k++)
            buf[n++] = tmpl[k] == 'N' ? (char)('0' + i) : tmpl[k];
    buf[n] = 0;
    return n;
}

static void load_cases(void)
{
    static char buf[UC_MAX_FILE + 2];
    long n;

    /* no file: the defaults stand and a save may create it */
    CHECK_INT(prefs_load(&conf, 0, 0, UC_MAX_FILE), PREFS_LOAD_NONE);
    CHECK(prefs_load_writable(PREFS_LOAD_NONE));

    /* a file over the cap: read-only, where it used to load empty and be
     * overwritten by the next Save */
    memset(buf, ';', UC_MAX_FILE + 1);
    buf[UC_MAX_FILE + 1] = 0;
    CHECK_INT(prefs_load(&conf, buf, UC_MAX_FILE + 1, UC_MAX_FILE), PREFS_LOAD_TOOBIG);
    CHECK(!prefs_load_writable(PREFS_LOAD_TOOBIG));
    CHECK_INT(conf.nprof, 0);

    /* a file of exactly the cap is read whole (blank lines fill it: a
     * 16 KB comment is more than the table keeps of comments, UC_NOTE_BYTES) */
    memset(buf, '\n', UC_MAX_FILE);
    memcpy(buf, "[profile a]\nx = 1\n", 18);
    buf[UC_MAX_FILE] = 0;
    CHECK_INT(prefs_load(&conf, buf, UC_MAX_FILE, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_STR(upconf_str(&conf, "a", "x", "?"), "1");

    /* nine profiles: upconf holds eight, so the ninth would be lost on save */
    n = profiles_text(buf, UC_MAX_PROFILES + 1);
    CHECK_INT(prefs_load(&conf, buf, n, UC_MAX_FILE), PREFS_LOAD_LOSSY);
    CHECK(!prefs_load_writable(PREFS_LOAD_LOSSY));
    CHECK_INT(conf.nprof, 0);

    /* eight is the table's room: writable */
    n = profiles_text(buf, UC_MAX_PROFILES);
    CHECK_INT(prefs_load(&conf, buf, n, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_INT(conf.nprof, UC_MAX_PROFILES);

    /* a failed read is not "no file" */
    CHECK_INT(prefs_load(&conf, 0, -1, UC_MAX_FILE), PREFS_LOAD_ERROR);
    CHECK(!prefs_load_writable(PREFS_LOAD_ERROR));
}

/* ---- stage ----------------------------------------------------------------------- */

static void stage_cases(void)
{
    static char buf[UC_MAX_FILE + 1];
    static const char file[] =
        "[profile default]\nfont = TOPAZ 8.8.font\nfont-fallback = Symbols Nerd Font Mono\n"
        "screen = fullscreen\nscreen-mode = 0x29004\nscreen-depth = 4\nbell = none\n"
        "program-clipboard = read-write\nlink-open = Run >NIL: OpenURL %s\n"
        "[profile vim]\nfg = C0C0C0\n";
    prefs_fields f, g;
    long len;
    int i;

    /* a staged edit round-trips through the file text */
    CHECK_INT(prefs_load(&conf, file, (long)strlen(file), UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    CHECK_STR(f.font, "TOPAZ 8.8.font");
    CHECK_INT(f.bell, PREFS_BELL_NONE);
    f.cursor = PREFS_CURSOR_BAR;
    f.blink = 1;
    CHECK_INT(f.completion, PREFS_COMPLETE_UNIX);   /* no key: today's Tab */
    f.completion = PREFS_COMPLETE_KINGCON;
    strcpy(f.kcmode, "CB");
    f.kcinfo = 1;
    CHECK_INT(f.kccache, 1);   /* on unless the file says off */
    f.kccache = 0;
    CHECK_INT(f.reflow, 1);    /* on unless the file says off (gaps #11) */
    f.reflow = 0;
    strcpy(f.pal[3], "#FFAA00");
    strcpy(f.selbg, "203040");
    CHECK_INT(prefs_validate(&f), 0);
    memcpy(&before, &conf, sizeof(conf));
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK_INT((long)strlen(buf), len);
    CHECK(!memcmp(&before, &conf, sizeof(conf))); /* staged, not applied */
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&g, &conf, "default");
    CHECK_STR(g.font, "TOPAZ 8.8.font");
    /* the fallback font survives a save (the stage writes the profile afresh) */
    CHECK_STR(g.fallback, "Symbols Nerd Font Mono");
    CHECK_STR(g.screen, "fullscreen");      /* the screen keys survive a save too */
    CHECK_STR(g.screenmode, "0x29004");
    CHECK_STR(g.screendepth, "4");
    CHECK_STR(g.clipboard, "read-write");   /* OSC 52 access (G3) survives a save */
    CHECK_STR(g.linkopen, "Run >NIL: OpenURL %s"); /* and the OSC 8 link command */
    CHECK_INT(g.cursor, PREFS_CURSOR_BAR);
    CHECK_INT(g.blink, 1);
    CHECK_INT(g.completion, PREFS_COMPLETE_KINGCON);
    CHECK_STR(upconf_str(&conf, "default", "completion", "?"), "kingcon");
    CHECK_STR(g.kcmode, "CB");
    CHECK_INT(g.kcinfo, 1);
    CHECK_INT(g.kccache, 0);
    CHECK_INT(g.reflow, 0);
    CHECK_STR(upconf_str(&conf, "default", "reflow", "?"), "off");
    CHECK_STR(g.pal[3], "FFAA00");
    CHECK_STR(g.selbg, "203040");
    CHECK_STR(upconf_str(&conf, "vim", "fg", "?"), "C0C0C0"); /* other profiles kept */
    CHECK(prefs_profile_exists(&conf, "VIM"));
    CHECK(!prefs_profile_exists(&conf, "emacs"));

    /* a file that would not fit: refused, the editor's table untouched (the
     * old save removed the profile from the table before finding out) */
    memcpy(&before, &conf, sizeof(conf));
    len = prefs_stage(&work, &conf, "default", &f, buf, 40);
    CHECK_INT(len, PREFS_STAGE_SIZE);
    CHECK(!memcmp(&before, &conf, sizeof(conf)));
    CHECK(upconf_get(&conf, "default", "font") != 0);

    /* a ninth profile: refused rather than saved without it */
    upconf_clear(&conf);
    for (i = 0; i < UC_MAX_PROFILES; i++) {
        char name[3];
        name[0] = 'p';
        name[1] = (char)('0' + i);
        name[2] = 0;
        upconf_set(&conf, name, "font", "x");
    }
    memcpy(&before, &conf, sizeof(conf));
    prefs_defaults(&f);
    CHECK_INT(prefs_stage(&work, &conf, "ninth", &f, buf, sizeof(buf)), PREFS_STAGE_FULL);
    CHECK(!memcmp(&before, &conf, sizeof(conf)));
    /* an existing one of the eight still saves */
    CHECK(prefs_stage(&work, &conf, "p3", &f, buf, sizeof(buf)) > 0);

    /* validation names the field */
    prefs_defaults(&f);
    strcpy(f.pal[5], "12345");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_PAL + 5);
    prefs_defaults(&f);
    strcpy(f.selfg, "12345G");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_SELFG);

    /* the typed name is trimmed and loses its brackets */
    {
        char out[UC_NAME];
        prefs_clean_name("  [vim] ", out, sizeof(out));
        CHECK_STR(out, "vim");
    }
}

/* A theme file (one .conf in themes/: a profile section of colours) applied to
 * the fields: its colours replace the profile's, the rest of the profile
 * (font, bell, cursor shape...) stays; a colour the theme does not set is
 * cleared, so nothing of an earlier theme lingers. */
static void theme_cases(void)
{
    static upconf work;
    prefs_fields f;
    static const char theme[] =
        "[profile Apprentice Default]\nfg = BCBCBC\nbg = 262626\ncursor-color = 5F875F\n"
        "selection-bg = 87AFD7\nselection-fg = 262626\n"
        "palette = 0,1C1C1C,1,AF5F5F,15,FFFFFF\n";
    prefs_defaults(&f);
    strcpy(f.font, "TOPAZ:11.font");
    f.bell = PREFS_BELL_NONE;
    strcpy(f.pal[3], "123456");                 /* an earlier theme's entry */
    CHECK_INT(prefs_apply_theme(&f, &work, theme, (long)strlen(theme)), 1);
    CHECK_STR(f.fg, "BCBCBC");
    CHECK_STR(f.bg, "262626");
    CHECK_STR(f.curcol, "5F875F");
    CHECK_STR(f.selbg, "87AFD7");
    CHECK_STR(f.selfg, "262626");
    CHECK_STR(f.pal[0], "1C1C1C");
    CHECK_STR(f.pal[15], "FFFFFF");
    CHECK_STR(f.pal[3], "");                    /* not in the theme: cleared */
    CHECK_STR(f.font, "TOPAZ:11.font");         /* not a colour: kept */
    CHECK_INT(f.bell, PREFS_BELL_NONE);
    /* a file with no colours is no theme: the fields stay as they were */
    {
        static const char notheme[] = "[profile x]\nbell = none\n";
        CHECK_INT(prefs_apply_theme(&f, &work, notheme, (long)strlen(notheme)), 0);
    }
    CHECK_STR(f.fg, "BCBCBC");
    CHECK_INT(prefs_apply_theme(&f, &work, "", 0), 0);
}

void suite_prefs(void)
{
    theme_cases();
    install_cases();
    install_restore_case();
    load_cases();
    stage_cases();
}
