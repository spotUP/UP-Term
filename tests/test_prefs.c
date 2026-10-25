/* The Prefs editor's model (prefs/prefs_core.c): what a load lets the editor
 * write back, a save staged off the live table, and the file replaced
 * without a moment where the user's profiles are gone. No OS calls: the
 * install runs against a file table in memory. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../prefs/prefs_core.h"
#include "../handler/kcmode.h"
#include "../prefs/prefs_ui.h"
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
        "program-clipboard = read-write\nlink-open = Run >NIL: OpenURL %s\nterm = xterm-256color\ncolors = rgb\n"
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
    CHECK_INT(f.scrollbar, 1); /* the scroll bar shows unless the file says hide */
    f.scrollbar = 0;
    /* the keys the Advanced page added: read as the handler reads them,
     * written back as it reads them */
    CHECK_INT(f.aspect, 1);         /* no key: on */
    f.aspect = 0;
    CHECK_INT(f.backspace_bs, 0);   /* no key: DEL */
    f.backspace_bs = 1;
    CHECK_INT(f.amiga_keys, 1);     /* no key: AmigaShell keys on */
    CHECK_INT(f.hlcat, 1);          /* no key: cat highlights */
    f.amiga_keys = 0;
    f.hlcat = 0;
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
    /* the fallback font survives a save */
    CHECK_STR(g.fallback, "Symbols Nerd Font Mono");
    CHECK_INT(g.screen, PREFS_SCREEN_FULL); /* the screen keys survive a save too */
    CHECK_STR(g.screenmode, "0x29004");
    CHECK_STR(g.screendepth, "4");
    CHECK_INT(g.clipboard, PREFS_CLIP_READ_WRITE); /* OSC 52 access (G3) survives a save */
    CHECK_STR(g.linkopen, "Run >NIL: OpenURL %s"); /* and the OSC 8 link command */
    CHECK_STR(g.term, "xterm-256color");            /* and what programs are told (W46) */
    CHECK_STR(g.colors, "rgb");
    CHECK_INT(g.cursor, PREFS_CURSOR_BAR);
    CHECK_INT(g.blink, 1);
    CHECK_INT(g.completion, PREFS_COMPLETE_KINGCON);
    CHECK_STR(upconf_str(&conf, "default", "completion", "?"), "kingcon");
    CHECK_STR(g.kcmode, "CB");
    CHECK_INT(g.kcinfo, 1);
    CHECK_INT(g.kccache, 0);
    CHECK_INT(g.reflow, 0);
    CHECK_STR(upconf_str(&conf, "default", "font-aspect", "?"), "off");
    CHECK_STR(upconf_str(&conf, "default", "backspace", "?"), "bs");
    CHECK_STR(upconf_str(&conf, "default", "screen", "?"), "fullscreen");
    CHECK_STR(upconf_str(&conf, "default", "program-clipboard", "?"), "read-write");
    CHECK_INT(g.aspect, 0);
    CHECK_INT(g.backspace_bs, 1);
    CHECK_INT(g.amiga_keys, 0);     /* both options survive a save, off ... */
    CHECK_INT(g.hlcat, 0);
    CHECK_STR(upconf_str(&conf, "default", "amiga-keys", "?"), "off");
    CHECK_STR(upconf_str(&conf, "default", "highlight-cat", "?"), "off");
    CHECK_STR(upconf_str(&conf, "default", "reflow", "?"), "off");
    CHECK_INT(g.scrollbar, 0);
    CHECK_STR(upconf_str(&conf, "default", "scrollbar", "?"), "hide");
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

/* A save keeps what the editor does not own. prefs_stage removed the whole
 * profile and wrote back only the editor's fields, so backspace,
 * font-aspect, any hand-typed key and every comment went on each Save,
 * Use and the window's Save settings to profile (which stages the same
 * way: prefs_from_conf, the window's values over it, prefs_stage). */
/* STYLE-SAVED-DEFAULT: a Theme, Font or Background chosen in the window's
 * menu is saved to the profile, and only the look's keys change -- the
 * window's other settings (changed for this window alone) are not saved
 * with it, and keys the file does not have are not added. */
static void style_stage_sets_only_the_look(void)
{
    static upconf conf, work;
    static char buf[UC_MAX_FILE + 1];
    static const char file[] =
        "# mine\n[default]\nfont = topaz.font 8\nscrollback = 500\nbg = 000000\n"
        "link-open = run x %s\nhand-typed = kept\n[other]\nbg = 112233\n";
    prefs_fields f;
    long len;
    CHECK_INT(prefs_load(&conf, file, (long)sizeof(file) - 1, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    strcpy(f.font, "IBMPlexMono.font 12");
    strcpy(f.bg, "1E1E2E");
    strcpy(f.fg, "CDD6F4");
    strcpy(f.pal[1], "F38BA8");
    strcpy(f.sb, "9999");      /* the window's own: not saved by a style change */
    strcpy(f.linkopen, "other %s");
    f.reflow = 0;
    len = prefs_stage_style(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK(strstr(buf, "# mine") != 0);
    CHECK(strstr(buf, "reflow") == 0);    /* absent stays absent */
    CHECK(strstr(buf, "cursor-blink") == 0);
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_STR(upconf_str(&conf, "default", "font", "?"), "IBMPlexMono.font 12");
    CHECK_STR(upconf_str(&conf, "default", "bg", "?"), "1E1E2E");
    CHECK_STR(upconf_str(&conf, "default", "fg", "?"), "CDD6F4");
    CHECK_STR(upconf_str(&conf, "default", "scrollback", "?"), "500");
    CHECK_STR(upconf_str(&conf, "default", "link-open", "?"), "run x %s");
    CHECK_STR(upconf_str(&conf, "default", "hand-typed", "?"), "kept");
    CHECK_STR(upconf_str(&conf, "other", "bg", "?"), "112233");
    prefs_from_conf(&f, &conf, "default");
    CHECK_STR(f.pal[1], "F38BA8");
    /* a table that did not hold the whole file is not written back */
    conf.overflow = 1;
    CHECK_INT(prefs_stage_style(&work, &conf, "default", &f, buf, sizeof(buf)), PREFS_STAGE_LOSSY);
}

/* the two options default on, are written as on / off, and come back on */
static void amiga_keys_and_highlight_cat_round_trip(void)
{
    static upconf conf, work;
    static char buf[UC_MAX_FILE + 1];
    prefs_fields f, g;
    long len;
    CHECK_INT(prefs_load(&conf, "[default]\nfont = x\n", 20, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    CHECK_INT(f.amiga_keys, 1);
    CHECK_INT(f.hlcat, 1);
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_STR(upconf_str(&conf, "default", "amiga-keys", "?"), "on");
    CHECK_STR(upconf_str(&conf, "default", "highlight-cat", "?"), "on");
    f.amiga_keys = 0;
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&g, &conf, "default");
    CHECK_INT(g.amiga_keys, 0);
    CHECK_INT(g.hlcat, 1);          /* the other is untouched */
    /* the switch the vshrc reads */
    CHECK_STR(prefs_hlcat_text(1), "on\n");
    CHECK_STR(prefs_hlcat_text(0), "off\n");
}

/* W-T9 dither (on by default) and W-T14 padding-color (background by default): no key gives the defaults, the
 * editor writes both, and what it wrote reads back -- the words the handler reads (apply_profile) */
/* The brightness slider got a LONG array it read as UWORDs ({0, p0, 0, p1,
 * 0xFFFF} on a 68k): the list is UWORDs ended by 0xFFFF. */
static void the_brightness_slider_gets_a_uword_pen_list_ended_by_ffff(void)
{
    unsigned short g[3];
    CHECK(prefs_grad_pens(g, 5, 9) == 1);
    CHECK(sizeof(g[0]) == 2 && g[0] == 5 && g[1] == 9 && g[2] == 0xFFFF);
    CHECK(prefs_grad_pens(g, -1, 9) == 0 && g[0] == 0xFFFF);
    CHECK(prefs_grad_pens(g, 5, -1) == 0 && g[0] == 0xFFFF);
    CHECK(prefs_grad_pens(g, 5, 0xFFFF) == 0 && g[0] == 0xFFFF);
}

static void dither_and_padding_color_round_trip(void)
{
    static upconf conf, work;
    static char buf[UC_MAX_FILE + 1];
    prefs_fields f, g;
    long len;
    CHECK_INT(prefs_load(&conf, "[default]\nfont = x\n", 20, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    CHECK_INT(f.dither, 1);
    CHECK_INT(f.outline_aa, 1);
    CHECK_INT(f.pad_extend, 0);
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_STR(upconf_str(&conf, "default", "dither", "?"), "on");
    CHECK_STR(upconf_str(&conf, "default", "outline-antialias", "?"), "on");
    CHECK_STR(upconf_str(&conf, "default", "padding-color", "?"), "background");
    f.dither = 0;
    f.outline_aa = 0;
    f.pad_extend = 1;
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_STR(upconf_str(&conf, "default", "dither", "?"), "off");
    CHECK_STR(upconf_str(&conf, "default", "outline-antialias", "?"), "off");
    CHECK_STR(upconf_str(&conf, "default", "padding-color", "?"), "extend");
    prefs_from_conf(&g, &conf, "default");
    CHECK_INT(g.dither, 0);
    CHECK_INT(g.outline_aa, 0);
    CHECK_INT(g.pad_extend, 1);
    CHECK_INT(prefs_load(&conf, "[default]\ndither = junk\noutline-antialias = junk\npadding-color = junk\n", 70, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&g, &conf, "default"); /* anything but off / extend is the default */
    CHECK_INT(g.dither, 1);
    CHECK_INT(g.outline_aa, 1);
    CHECK_INT(g.pad_extend, 0);
}

static void keep_cases(void)
{
    static char buf[UC_MAX_FILE + 1];
    static char again[UC_MAX_FILE + 1];
    static const char file[] =
        "; my settings\n"
        "[profile default]\n"
        "; the font I like\n"
        "font = TOPAZ 8.8.font\n"
        "backspace = bs\n"
        "font-aspect = off\n"
        "my-own-key = 42\n"
        "scrollbar = hide\n"
        "bell = none\n"
        "; the end of default\n"
        "[profile vim]\n"
        "# vim's own\n"
        "fg = C0C0C0\n"
        "unknown = 1\n";
    prefs_fields f;
    long len, len2;

    CHECK_INT(prefs_load(&conf, file, (long)strlen(file), UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    strcpy(f.font, "XEN 11.font");     /* the editor's own keys change */
    f.bell = PREFS_BELL_VISUAL;
    CHECK_INT(f.scrollbar, 0);         /* SB1's key: staged in place like the rest */
    f.scrollbar = 1;
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK_STR(upconf_str(&conf, "default", "font", "?"), "XEN 11.font");
    CHECK_STR(upconf_str(&conf, "default", "bell", "?"), "visual");
    /* the keys only the handler reads, and one nobody reads, are still there */
    CHECK_STR(upconf_str(&conf, "default", "backspace", "?"), "bs");
    CHECK_STR(upconf_str(&conf, "default", "font-aspect", "?"), "off");
    CHECK_STR(upconf_str(&conf, "default", "my-own-key", "?"), "42");
    /* the other profile as it was */
    CHECK_STR(upconf_str(&conf, "vim", "unknown", "?"), "1");
    CHECK_STR(upconf_str(&conf, "vim", "fg", "?"), "C0C0C0");
    /* the comments, and an edited key where it stood, under its comment */
    CHECK(strstr(buf, "; my settings\n") == buf);
    CHECK(strstr(buf, "; the font I like\nfont = XEN 11.font\nbackspace = bs\n") != 0);
    CHECK(strstr(buf, "; the end of default\n") != 0);
    CHECK(strstr(buf, "# vim's own\nfg = C0C0C0\n") != 0);
    CHECK(strstr(buf, "my-own-key = 42\nscrollbar = show\nbell = visual\n") != 0);

    /* a field emptied: that key goes (the handler's built-in stands), its
     * comment and the keys around it stay */
    prefs_from_conf(&f, &conf, "default");
    f.font[0] = 0;
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK(upconf_get(&conf, "default", "font") == 0);
    CHECK(strstr(buf, "; the font I like\nbackspace = bs\n") != 0);
    CHECK_STR(upconf_str(&conf, "default", "my-own-key", "?"), "42");

    /* round trip: the saved file, loaded and saved unchanged, is the same text */
    prefs_from_conf(&f, &conf, "default");
    len2 = prefs_stage(&work, &conf, "default", &f, again, sizeof(again));
    CHECK_INT(len2, len);
    CHECK_STR(again, buf);

    /* a new profile: the editor's keys, the file's other profiles and
     * comments untouched */
    prefs_defaults(&f);
    len = prefs_stage(&work, &conf, "emacs", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK_INT(prefs_load(&conf, buf, len, UC_MAX_FILE), PREFS_LOAD_OK);
    CHECK(prefs_profile_exists(&conf, "emacs"));
    CHECK_STR(upconf_str(&conf, "default", "backspace", "?"), "bs");
    CHECK(strstr(buf, "# vim's own\n") != 0);

    /* a table that did not hold the whole file (the window's own read
     * keeps what fits: a ninth profile, or a comment store full) is not
     * written back: the window menu's Save settings to profile staged it
     * and wrote the file without what had not fit */
    {
        static char nine[512];
        long n = profiles_text(nine, UC_MAX_PROFILES + 1);
        upconf_parse(&conf, nine, n);
        CHECK(conf.overflow);
        CHECK(upconf_get(&conf, "p8", "font") == 0); /* the ninth was not kept */
        prefs_from_conf(&f, &conf, "p0");
        memcpy(&before, &conf, sizeof(conf));
        CHECK_INT(prefs_stage(&work, &conf, "p0", &f, buf, sizeof(buf)), PREFS_STAGE_LOSSY);
        CHECK(!memcmp(&before, &conf, sizeof(conf)));
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

/* W30 (owner 2026-10-04: "the theme selector doesn't open the theme dir"):
 * every theme requester and /theme open the drawer prefs_theme_drawer
 * names. The rig had no ENVARC:up-term/themes (the kit is not installed
 * there), and the requesters named that drawer whatever was on disk. */
static void theme_requester_opens_the_themes_drawer(void)
{
    memfs m;
    prefs_fs fs;
    char d[64];
    /* the kit installed: its drawer, whatever else is there */
    mf_init(&m, &fs);
    mf_put(&m, "ENVARC:up-term/themes", "");
    mf_put(&m, "ENV:up-term/themes", "");
    mf_put(&m, "VTC:themes", "");
    prefs_theme_drawer(&fs, "", "VTC:", d, sizeof(d));
    CHECK_STR(d, "ENVARC:up-term/themes");
    /* a theme already chosen: its drawer first */
    mf_put(&m, "Work:mythemes", "");
    prefs_theme_drawer(&fs, "Work:mythemes/night.conf", "VTC:", d, sizeof(d));
    CHECK_STR(d, "Work:mythemes");
    prefs_theme_drawer(&fs, "Gone:old/night.conf", "VTC:", d, sizeof(d));
    CHECK_STR(d, "ENVARC:up-term/themes");      /* that drawer is gone: the kit's */
    prefs_theme_drawer(&fs, "night.conf", 0, d, sizeof(d));
    CHECK_STR(d, "ENVARC:up-term/themes");      /* a bare name names no drawer */
    /* the kit's drawer missing (the rig): its ENV: copy, then the program's
     * themes drawer, then the program's drawer -- never the requester's
     * own choice while one of them is there */
    mf_init(&m, &fs);
    mf_put(&m, "ENV:up-term/themes", "");
    mf_put(&m, "VTC:themes", "");
    prefs_theme_drawer(&fs, 0, "VTC:", d, sizeof(d));
    CHECK_STR(d, "ENV:up-term/themes");
    mf_init(&m, &fs);
    mf_put(&m, "VTC:themes", "");
    mf_put(&m, "VTC:", "");
    prefs_theme_drawer(&fs, 0, "VTC:", d, sizeof(d));
    CHECK_STR(d, "VTC:themes");
    mf_put(&m, "Work:UP-Term/themes", "");
    prefs_theme_drawer(&fs, 0, "Work:UP-Term", d, sizeof(d));
    CHECK_STR(d, "Work:UP-Term/themes");        /* a drawer, not a volume: a '/' */
    mf_init(&m, &fs);
    mf_put(&m, "VTC:", "");
    prefs_theme_drawer(&fs, 0, "VTC:", d, sizeof(d));
    CHECK_STR(d, "VTC:");
    mf_init(&m, &fs);
    prefs_theme_drawer(&fs, "Work:x/y.conf", "VTC:", d, sizeof(d));
    CHECK_STR(d, "");                           /* nothing there at all */
    /* /theme NAME: a name in the drawer, a path as it is, .conf added once */
    prefs_theme_file("ENVARC:up-term/themes", "nord-default", d, sizeof(d));
    CHECK_STR(d, "ENVARC:up-term/themes/nord-default.conf");
    prefs_theme_file("VTC:themes", "Nord-Default.CONF", d, sizeof(d));
    CHECK_STR(d, "VTC:themes/Nord-Default.CONF");
    prefs_theme_file("ENVARC:up-term/themes", "Work:t/mine", d, sizeof(d));
    CHECK_STR(d, "Work:t/mine.conf");
    prefs_theme_file("", "mine", d, sizeof(d));
    CHECK_STR(d, "mine.conf");
}

/* /theme's list: a drawer's names to the themes in it, sorted. */
static void theme_list_from_a_drawer(void)
{
    char names[64];
    int len = 0;
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "nord-default.conf"), 1);
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "ATTRIBUTION.md"), 0);
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "ayu-dark.conf"), 1);
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "Dracula.CONF"), 1);
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "ayu-dark.yaml"), 0);
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), ".conf"), 0);
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "AYU-DARK.conf"), 0); /* a repeat */
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "ayu.conf"), 1);      /* a prefix sorts first */
    CHECK_INT(len, 34);
    CHECK_STR(names, "ayu");
    CHECK_STR(names + 4, "ayu-dark");
    CHECK_STR(names + 13, "Dracula");
    CHECK_STR(names + 21, "nord-default");
    CHECK_INT(prefs_theme_add(names, &len, sizeof(names), "solarized-light-and-more-than-fits.conf"), -1);
    CHECK_INT(len, 34);
    CHECK_STR(names + 21, "nord-default");
    /* the window's theme marked in it: by path, by name, any case */
    CHECK_INT(prefs_theme_index(names, 4, "ENVARC:up-term/themes/dracula.conf"), 2);
    CHECK_INT(prefs_theme_index(names, 4, "VTC:AYU-DARK.CONF"), 1);
    CHECK_INT(prefs_theme_index(names, 4, "ayu"), 0);
    CHECK_INT(prefs_theme_index(names, 4, "Work:t/ayu-light.conf"), -1);
    CHECK_INT(prefs_theme_index(names, 4, "Work:t/"), -1);
    CHECK_INT(prefs_theme_index(names, 4, ""), -1);
    CHECK_INT(prefs_theme_index(names, 4, 0), -1);
}


/* ---- PX1: KingCON's letters <-> the window's choices ---------------------- */

static void kingcon_style_letters_round_trip_through_the_handlers_function(void)
{
    static const char names[] = "WLBCS";
    char letters[8], enc[8];
    int set, i, n, match, common, silent, mode;
    prefs_kc k, k2;
    /* every subset of the five letters, in both cases: the letters the window writes
     * give the handler the same mode bits as the letters it read */
    for (set = 0; set < 32; set++) {
        for (i = n = 0; i < 5; i++)
            if (set & (1 << i))
                letters[n++] = names[i];
        letters[n] = 0;
        mode = le_kc_fncmode(letters);
        prefs_kc_decode(letters, &k);
        prefs_kc_encode(&k, enc, sizeof(enc));
        CHECK_INT(le_kc_fncmode(enc), mode);
        for (i = 0; letters[i]; i++)
            letters[i] = (char)(letters[i] | 0x20);
        CHECK_INT(le_kc_fncmode(letters), mode);
        prefs_kc_decode(letters, &k2);
        CHECK_INT(k2.match, k.match);
        CHECK_INT(k2.common, k.common);
        CHECK_INT(k2.silent, k.silent);
    }
    /* every choice of the window encodes to letters that decode to the choice */
    for (match = 0; match <= PREFS_KC_COMMON_ONLY; match++)
        for (common = 0; common < 2; common++)
            for (silent = 0; silent < 2; silent++) {
                k.match = match;
                k.common = common;
                k.silent = silent;
                prefs_kc_encode(&k, enc, sizeof(enc));
                prefs_kc_decode(enc, &k2);
                CHECK_INT(k2.match, match);
                CHECK_INT(k2.common, match == PREFS_KC_COMMON_ONLY ? 1 : common);
                CHECK_INT(k2.silent, silent);
            }
    /* the written form is the normalised one */
    k.match = PREFS_KC_LISTCYCLE; k.common = 0; k.silent = 1;
    prefs_kc_encode(&k, enc, sizeof(enc));
    CHECK_STR(enc, "LBS");
    k.match = PREFS_KC_LIST; k.common = 1; k.silent = 0;
    prefs_kc_encode(&k, enc, sizeof(enc));
    CHECK_STR(enc, "LC");
    k.match = PREFS_KC_WINDOW; k.common = 0; k.silent = 0;
    prefs_kc_encode(&k, enc, sizeof(enc));
    CHECK_STR(enc, "");
    prefs_kc_decode("wlb", &k);          /* W clears L and B, as KingCON */
    CHECK_INT(k.match, PREFS_KC_WINDOW);
    prefs_kc_decode("lbs", &k);
    CHECK_INT(k.match, PREFS_KC_LISTCYCLE);
    CHECK_INT(k.silent, 1);
    prefs_kc_decode("C", &k);
    CHECK_INT(k.match, PREFS_KC_COMMON_ONLY);
    prefs_kc_decode("zz9", &k);          /* no letter: the handler's W */
    CHECK_INT(k.match, PREFS_KC_WINDOW);
    /* the profile round trip: LB + S through a save */
    {
        prefs_fields f;
        upconf c, w;
        char buf[2048];
        long len;
        prefs_defaults(&f);
        k.match = PREFS_KC_LISTCYCLE; k.common = 0; k.silent = 1;
        prefs_kc_encode(&k, f.kcmode, sizeof(f.kcmode));
        upconf_clear(&c);
        len = prefs_stage(&w, &c, "default", &f, buf, sizeof(buf));
        CHECK(len > 0);
        CHECK(strstr(buf, "kingcon-mode = LBS") != 0);
    }
}

/* ---- PX2: colours ---------------------------------------------------------- */

static void colour_fields_are_validated_strictly_and_named(void)
{
    prefs_fields f;
    uc_u32 rgb = 7;
    char lab[48], hex[8];
    int i;
    CHECK_INT(prefs_hex_parse("FF8000", &rgb), 1);
    CHECK_INT(rgb, 0xFF8000);
    CHECK_INT(prefs_hex_parse("#00ff7f", &rgb), 1);
    CHECK_INT(rgb, 0x00FF7F);
    CHECK_INT(prefs_hex_parse("0X123abc", &rgb), 1);
    CHECK_INT(rgb, 0x123ABC);
    rgb = 7;
    CHECK_INT(prefs_hex_parse("FF80", &rgb), 0);       /* too short */
    CHECK_INT(prefs_hex_parse("FF80000", &rgb), 0);    /* too long */
    CHECK_INT(prefs_hex_parse("FF800G", &rgb), 0);
    CHECK_INT(prefs_hex_parse(" FF8000", &rgb), 0);    /* no blanks */
    CHECK_INT(prefs_hex_parse("FF8000 ", &rgb), 0);
    CHECK_INT(prefs_hex_parse("#0xFF8000", &rgb), 0);
    CHECK_INT(prefs_hex_parse("", &rgb), 0);
    CHECK_INT(rgb, 7);
    prefs_hex_format(0x0A0B0C, hex);
    CHECK_STR(hex, "0A0B0C");
    CHECK_STR(prefs_pal_name(0), "Black");
    CHECK_STR(prefs_pal_name(1), "Red");
    CHECK_STR(prefs_pal_name(15), "Bright White");
    /* each of the 21 colour fields with bad input names itself */
    for (i = 0; i < 16; i++) {
        prefs_defaults(&f);
        strcpy(f.pal[i], "12345");
        CHECK_INT(prefs_validate(&f), PREFS_BAD_PAL + i);
        prefs_bad_label(prefs_validate(&f), lab, sizeof(lab));
        CHECK(strstr(lab, prefs_pal_name(i)) != 0);
        CHECK(strstr(lab, "Palette") != 0);
    }
    prefs_defaults(&f);
    strcpy(f.fg, "red");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_FG);
    prefs_bad_label(PREFS_BAD_FG, lab, sizeof(lab));
    CHECK_STR(lab, "Text colour");
    prefs_defaults(&f);
    strcpy(f.bg, "12 456");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_BG);
    prefs_bad_label(PREFS_BAD_BG, lab, sizeof(lab));
    CHECK_STR(lab, "Background");
    prefs_defaults(&f);
    strcpy(f.selfg, "x");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_SELFG);
    prefs_defaults(&f);
    strcpy(f.selbg, "x");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_SELBG);
    prefs_bad_label(PREFS_BAD_SELBG, lab, sizeof(lab));
    CHECK_STR(lab, "Selection");
    prefs_defaults(&f);
    strcpy(f.curcol, "bluish");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_CURCOL);
    prefs_bad_label(PREFS_BAD_CURCOL, lab, sizeof(lab));
    CHECK_STR(lab, "Cursor colour");
    /* good input, blanks and "inverse" pass */
    prefs_defaults(&f);
    strcpy(f.fg, "#102030");
    strcpy(f.bg, "0x405060");
    strcpy(f.pal[3], "AABBCC");
    strcpy(f.curcol, "Inverse");
    CHECK_INT(prefs_validate(&f), 0);
    strcpy(f.curcol, "00FF00");
    CHECK_INT(prefs_validate(&f), 0);
    /* W46 (b): the Terminal type field takes what /term takes */
    strcpy(f.term, "xterm-256color");
    CHECK_INT(prefs_validate(&f), 0);
    strcpy(f.term, "xterm 256");
    CHECK_INT(prefs_validate(&f), PREFS_BAD_TERM);
    prefs_bad_label(PREFS_BAD_TERM, lab, sizeof(lab));
    CHECK_STR(lab, "Terminal type");
    /* the Colour mode cycle and the colors key, both ways */
    CHECK_INT(prefs_colors_index(""), 0);
    CHECK_INT(prefs_colors_index("rgb"), 1);
    CHECK_INT(prefs_colors_index("256"), 2);
    CHECK_INT(prefs_colors_index("16"), 3);
    CHECK_INT(prefs_colors_index("8"), 0);
    CHECK_STR(prefs_colors_value(3), "16");
    CHECK_STR(prefs_colors_value(0), "");
    CHECK_STR(prefs_colors_value(PREFS_COLORS_N), "");
}

/* ---- PX3: font strings ------------------------------------------------------ */

static void font_requester_results_become_profile_values(void)
{
    char out[40], name[40], drawer[40];
    long size = 9;
    CHECK_INT(prefs_font_compose("topaz.font", 8, out, sizeof(out)), 1);
    CHECK_STR(out, "topaz:8");
    CHECK_INT(prefs_font_compose("Topaz.FONT", 11, out, sizeof(out)), 1);
    CHECK_STR(out, "Topaz:11");
    CHECK_INT(prefs_font_compose("mono", 12, out, sizeof(out)), 1);   /* no .font to cut */
    CHECK_STR(out, "mono:12");
    CHECK_INT(prefs_font_compose("topaz.font", 0, out, sizeof(out)), 1);
    CHECK_STR(out, "topaz");
    CHECK_INT(prefs_font_compose(".font", 8, out, sizeof(out)), 0);   /* nothing left */
    CHECK_STR(out, "");
    CHECK_INT(prefs_font_compose("", 8, out, sizeof(out)), 0);
    CHECK_INT(prefs_font_compose("My Font.font", 8, out, sizeof(out)), 0); /* the handler splits at blanks */
    CHECK_STR(out, "");
    CHECK_INT(prefs_font_compose("a:b.font", 8, out, sizeof(out)), 0);
    CHECK_INT(prefs_font_compose("topaz.font", 8, out, 7), 0);        /* "topaz:8" needs 8 */
    CHECK_INT(prefs_font_compose("topaz.font", 8, out, 8), 1);
    /* and back: the requester starts from name with .font and the size */
    CHECK_INT(prefs_font_parse("topaz:8", name, sizeof(name), &size), 1);
    CHECK_STR(name, "topaz.font");
    CHECK_INT(size, 8);
    CHECK_INT(prefs_font_parse("topaz.font:11", name, sizeof(name), &size), 1);
    CHECK_STR(name, "topaz.font");
    CHECK_INT(size, 11);
    CHECK_INT(prefs_font_parse("mono", name, sizeof(name), &size), 1);
    CHECK_STR(name, "mono.font");
    CHECK_INT(size, 0);
    CHECK_INT(prefs_font_parse("", name, sizeof(name), &size), 0);
    CHECK_INT(prefs_font_parse(":8", name, sizeof(name), &size), 0);
    CHECK_INT(prefs_font_parse("topaz:8", name, 6, &size), 0);        /* "topaz.font" needs 11 */
    /* the outline fallback: the .otag file's name */
    CHECK_INT(prefs_otag_basename("FONTS:DejaVuSansMono.otag", out, sizeof(out)), 1);
    CHECK_STR(out, "DejaVuSansMono");
    CHECK_INT(prefs_otag_basename("Work:Fonts/Noto Sans.OTAG", out, sizeof(out)), 1);
    CHECK_STR(out, "Noto Sans");
    CHECK_INT(prefs_otag_basename("Hack.otag", out, sizeof(out)), 1);
    CHECK_STR(out, "Hack");
    CHECK_INT(prefs_otag_basename("FONTS:Hack.font", out, sizeof(out)), 0);
    CHECK_STR(out, "");
    CHECK_INT(prefs_otag_basename("FONTS:.otag", out, sizeof(out)), 0);
    CHECK_INT(prefs_otag_basename("FONTS:", out, sizeof(out)), 0);
    CHECK_INT(prefs_otag_basename("FONTS:LongName.otag", out, 5), 0);
    (void)drawer;
}

/* ---- PX4: scrollback, screen mode and depth ---------------------------------- */

static void scrollback_and_screen_values_read_back_sanely(void)
{
    long lines;
    int none;
    unsigned long id = 5;
    char out[16];
    prefs_sb_parse("", &lines, &none);
    CHECK_INT(lines, PREFS_SB_DEFAULT);
    CHECK_INT(none, 0);
    prefs_sb_parse("2000", &lines, &none);
    CHECK_INT(lines, 2000);
    CHECK_INT(none, 0);
    prefs_sb_parse("0", &lines, &none);
    CHECK_INT(none, 1);                         /* the handler: <= 0 is none */
    prefs_sb_parse("-5", &lines, &none);
    CHECK_INT(none, 1);
    prefs_sb_parse("lots", &lines, &none);      /* garbage: the built-in, as a blank */
    CHECK_INT(lines, PREFS_SB_DEFAULT);
    CHECK_INT(none, 0);
    prefs_sb_parse("9999999999", &lines, &none);
    CHECK_INT(lines, PREFS_SB_MAX);
    prefs_sb_format(2000, 0, out);
    CHECK_STR(out, "2000");
    prefs_sb_format(2000, 1, out);
    CHECK_STR(out, "0");
    prefs_sb_format(PREFS_SB_DEFAULT, 0, out);
    CHECK_STR(out, "");
    prefs_sb_format(123456789L, 0, out);
    CHECK_STR(out, "99999");
    prefs_sb_format(-3, 0, out);
    CHECK_STR(out, "");
    /* screen mode: 0x%08lx */
    CHECK_INT(prefs_mode_parse("0x00029000", &id), 1);
    CHECK_INT(id, 0x29000);
    CHECK_INT(prefs_mode_parse("29000", &id), 1);
    CHECK_INT(id, 0x29000);
    CHECK_INT(prefs_mode_parse("0XDEADBEEF", &id), 1);
    CHECK_INT(id == 0xDEADBEEFUL, 1);
    id = 5;
    CHECK_INT(prefs_mode_parse("", &id), 0);
    CHECK_INT(prefs_mode_parse("0x", &id), 0);
    CHECK_INT(prefs_mode_parse("0xzz", &id), 0);
    CHECK_INT(prefs_mode_parse("0x123456789", &id), 0);   /* nine digits */
    CHECK_INT(id, 5);
    prefs_mode_format(0x29000UL, out);
    CHECK_STR(out, "0x00029000");
    prefs_mode_format(0xDEADBEEFUL, out);
    CHECK_STR(out, "0xdeadbeef");
    prefs_mode_format(0, out);
    CHECK_STR(out, "");
    /* depth: 4..8, the handler's clamp; garbage is none */
    CHECK_INT(prefs_depth_parse("8"), 8);
    CHECK_INT(prefs_depth_parse("5"), 5);
    CHECK_INT(prefs_depth_parse("2"), 4);
    CHECK_INT(prefs_depth_parse("24"), 8);
    CHECK_INT(prefs_depth_parse(""), 0);
    CHECK_INT(prefs_depth_parse("deep"), 0);
    CHECK_INT(prefs_depth_parse("0"), 0);
    prefs_depth_format(0, out);
    CHECK_STR(out, "");
    prefs_depth_format(6, out);
    CHECK_STR(out, "6");
    prefs_depth_format(24, out);
    CHECK_STR(out, "8");
    prefs_depth_format(2, out);
    CHECK_STR(out, "4");
}

/* ---- PX5: unsaved changes -------------------------------------------------------- */

static void an_edit_of_any_field_makes_the_profile_differ(void)
{
    prefs_fields a, b;
    int i;
    prefs_defaults(&a);
    prefs_defaults(&b);
    CHECK_INT(prefs_equal(&a, &b), 1);
    /* bytes after the NUL are not part of a string */
    strcpy(a.font, "topaz:8");
    strcpy(b.font, "topaz:8XX");
    b.font[7] = 0;
    CHECK_INT(prefs_equal(&a, &b), 1);
    prefs_defaults(&a);
    {
#define MUT_STR(fld) prefs_defaults(&b); strcpy(b.fld, "x"); CHECK_INT(prefs_equal(&a, &b), 0);
#define MUT_INT(fld) prefs_defaults(&b); b.fld = !b.fld; CHECK_INT(prefs_equal(&a, &b), 0);
        MUT_STR(font) MUT_STR(fallback) MUT_STR(screenmode) MUT_STR(screendepth) MUT_STR(sb)
        MUT_STR(curcol) MUT_STR(fg) MUT_STR(bg) MUT_STR(selfg) MUT_STR(selbg) MUT_STR(term)
        MUT_STR(colors) MUT_STR(linkopen) MUT_STR(kcmode)
        MUT_INT(cursor) MUT_INT(bell) MUT_INT(blink) MUT_INT(bold) MUT_INT(meta_alt)
        MUT_INT(copy_sel) MUT_INT(wheel) MUT_INT(reflow) MUT_INT(scrollbar) MUT_INT(completion)
        MUT_INT(screen) MUT_INT(aspect) MUT_INT(backspace_bs) MUT_INT(clipboard) MUT_INT(kcinfo)
        MUT_INT(kccache) MUT_INT(amiga_keys) MUT_INT(dither) MUT_INT(outline_aa) MUT_INT(pad_extend) MUT_INT(hlcat)
    }
    for (i = 0; i < 16; i++) {
        prefs_defaults(&b);
        strcpy(b.pal[i], "000000");
        CHECK_INT(prefs_equal(&a, &b), 0);
    }
    /* a field edited and edited back is no change */
    prefs_defaults(&b);
    strcpy(b.fg, "FFFFFF");
    strcpy(b.fg, "");
    CHECK_INT(prefs_equal(&a, &b), 1);
}

/* ---- PX7: the layout ---------------------------------------------------------- */

static int tw_prop7(void *ctx, const char *s)   /* a proportional font, 7 pixels a letter */
{
    (void)ctx;
    return 7 * (int)strlen(s);
}

static int tw_fixed(void *ctx, const char *s)   /* topaz 8 */
{
    (void)ctx;
    return 8 * (int)strlen(s);
}

static int boxes_meet(const pl_box *a, const pl_box *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

/* every gadget of the page and the common ones, with the label strip left of it */
static void check_layout(const pl_result *r, int page, const pl_font *f)
{
    int i, j;
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *a = &pg_items[i];
        pl_box ba, la;
        if (a->page != page && a->page != PG_COMMON)
            continue;
        ba = r->box[a->id];
        la.x = ba.x - r->label_w[a->id];
        la.y = ba.y;
        la.w = r->label_w[a->id];
        la.h = ba.h;
        /* inside the window */
        CHECK(la.x >= 0);
        CHECK(ba.x + ba.w <= r->inner_w);
        CHECK(ba.y >= 0 && ba.y + ba.h <= r->inner_h);
        /* a check box's label to its right fits too */
        if (a->kind == PGK_CHECK) {
            char plain[80];
            pg_plain_label(a->label, plain, sizeof(plain));
            CHECK(ba.x + ba.w + 6 + (r->fallback ? tw_fixed(0, plain) : f->text_w(f->ctx, plain)) <= r->inner_w);
        }
        for (j = i + 1; j < pg_nitems; j++) {
            const pg_item *b = &pg_items[j];
            pl_box bb, lb;
            if (b->page != page && b->page != PG_COMMON)
                continue;
            bb = r->box[b->id];
            lb.x = bb.x - r->label_w[b->id];
            lb.y = bb.y;
            lb.w = r->label_w[b->id];
            lb.h = bb.h;
            if (boxes_meet(&ba, &bb) || (la.w && boxes_meet(&la, &bb)) ||
                (lb.w && boxes_meet(&lb, &ba)) || (la.w && lb.w && boxes_meet(&la, &lb))) {
                h_checks++;
                h_failures++;
                printf("  FAIL layout: gadget %d overlaps %d on page %d\n", a->id, b->id, page);
            }
        }
    }
}

static void the_window_fits_an_ntsc_and_a_pal_workbench(void)
{
    pl_font topaz, big;
    pl_result *r = (pl_result *)malloc(sizeof(*r));
    int page, id;
    topaz.w = 8; topaz.h = 8; topaz.text_w = tw_fixed; topaz.ctx = 0;
    big.w = 8; big.h = 11; big.text_w = tw_prop7; big.ctx = 0;
    /* 640x200 NTSC: 11 px screen bar + 11 px title + 2 border; width less the 4 + 18 px borders */
    pl_layout(&topaz, 618, 176, r);
    CHECK_INT(r->fits, 1);
    CHECK_INT(r->fallback, 0);
    for (page = 0; page < PG_NPAGES; page++)
        check_layout(r, page, &topaz);
    /* every gadget id of the table has a box */
    for (id = 0; id < pg_nitems; id++)
        CHECK(r->box[pg_items[id].id].w > 0 && r->box[pg_items[id].id].h > 0);
    /* a gadget lies where pl_hit finds it */
    for (page = 0; page < PG_NPAGES; page++)
        for (id = 0; id < pg_nitems; id++) {
            const pg_item *it = &pg_items[id];
            pl_box b = r->box[it->id];
            if (it->page == page || it->page == PG_COMMON)
                CHECK_INT(pl_hit(r, page, b.x + b.w / 2, b.y + b.h / 2), it->id);
        }
    CHECK_INT(pl_hit(r, 0, r->inner_w + 5, 0), 0);
    /* 640x256 PAL */
    pl_layout(&topaz, 618, 232, r);
    CHECK_INT(r->fits, 1);
    CHECK_INT(r->fallback, 0);
    /* an 11 pixel font on a PAL Workbench (14 px bars): fits or falls back, and fits */
    pl_layout(&big, 618, 256 - 14 - 14 - 2, r);
    CHECK_INT(r->fits, 1);
    for (page = 0; page < PG_NPAGES; page++)
        check_layout(r, page, &big);
    /* the same font on an NTSC one does not: topaz 8 takes over, and fits */
    pl_layout(&big, 618, 200 - 14 - 14 - 2, r);
    CHECK_INT(r->fallback, 1);
    CHECK_INT(r->fits, 1);
    CHECK_INT(r->font_h, 8);
    for (page = 0; page < PG_NPAGES; page++)
        check_layout(r, page, &big);
    /* a screen too small even for topaz 8 is reported, not hidden */
    pl_layout(&topaz, 400, 100, r);
    CHECK_INT(r->fits, 0);
    CHECK_INT(r->fallback, 1);
    free(r);
}

/* ---- PX13, PX14: help lines and shortcuts ------------------------------------------ */

static void every_gadget_has_help_and_a_unique_shortcut(void)
{
    int i, j, page;
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *it = &pg_items[i];
        CHECK(it->help != 0 && it->help[0] != 0);
        /* the status row is 618 pixels of topaz 8: 77 letters */
        CHECK(it->help == 0 || strlen(it->help) <= 76);
        for (j = i + 1; j < pg_nitems; j++)
            CHECK(pg_items[j].id != it->id);
        CHECK(pg_find(it->id) == it);
        CHECK(it->kind != PGK_CHECK || it->label != 0);
    }
    CHECK(pg_find(9999) == 0);
    /* a key reaches one gadget per page, the page's or a common one */
    for (page = 0; page < PG_NPAGES; page++)
        for (i = 0; i < pg_nitems; i++) {
            const pg_item *a = &pg_items[i];
            int ka = pg_shortcut(pg_label(a));
            if ((a->page != page && a->page != PG_COMMON) || !ka)
                continue;
            for (j = i + 1; j < pg_nitems; j++) {
                const pg_item *b = &pg_items[j];
                if (b->page != page && b->page != PG_COMMON)
                    continue;
                if (pg_shortcut(pg_label(b)) == ka) {
                    h_checks++;
                    h_failures++;
                    printf("  FAIL shortcut '%c' on page %d: gadgets %d and %d\n", ka, page, a->id, b->id);
                }
            }
            CHECK(pg_for_key(page, ka) == a);
            CHECK(pg_for_key(page, ka - 'a' + 'A') == a || ka < 'a' || ka > 'z');
        }
    CHECK_INT(pg_shortcut("_Font"), 'f');
    CHECK_INT(pg_shortcut("Scrollbac_k"), 'k');
    CHECK_INT(pg_shortcut("Choose..."), 0);
    CHECK_INT(pg_shortcut(0), 0);
    CHECK_INT(pg_shortcut("trailing_"), 0);
    CHECK(pg_for_key(0, 'S') == pg_find(PG_SAVE));
    CHECK(pg_for_key(2, 'f') == 0);   /* the palette page has no Font */
    {
        char out[20];
        pg_plain_label("Fon_t", out, sizeof(out));
        CHECK_STR(out, "Font");
    }
    /* the save, use and cancel keys work on every page */
    for (page = 0; page < PG_NPAGES; page++) {
        CHECK(pg_for_key(page, 's') == pg_find(PG_SAVE));
        CHECK(pg_for_key(page, 'u') == pg_find(PG_USE));
        CHECK(pg_for_key(page, 'c') == pg_find(PG_CANCEL));
    }
    /* a palette entry is labelled with its ANSI name */
    CHECK_STR(pg_label(pg_find(PG_PAL + 9)), "Bright Red");
    CHECK(pg_find(PG_PAL_HEX + 15) != 0);
    CHECK_INT(pg_find(PG_PAL_HEX + 15)->kind, PGK_STRING);
}

/* ---- PX12: the colour wheel's conversions ---------------------------------------- */

static int chan_off(unsigned long a, unsigned long b)
{
    int d = 0, k;
    for (k = 0; k < 24; k += 8) {
        int x = (int)((a >> k) & 0xFF), y = (int)((b >> k) & 0xFF);
        int e = x > y ? x - y : y - x;
        if (e > d)
            d = e;
    }
    return d;
}

static void wheel_colours_round_trip_within_one_step(void)
{
    prefs_hsb h;
    int i, r, g, b, worst = 0, e;
    unsigned long rgb;
    for (i = 0; i < 256; i++) {            /* all 256 greys */
        rgb = ((unsigned long)i << 16) | ((unsigned long)i << 8) | (unsigned long)i;
        prefs_rgb_to_hsb(rgb, &h);
        CHECK_INT(h.sat, 0);
        e = chan_off(rgb, prefs_hsb_to_rgb(&h));
        if (e > worst)
            worst = e;
    }
    for (r = 0; r < 6; r++)                /* the 6 x 6 x 6 cube */
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++) {
                rgb = ((unsigned long)(r * 51) << 16) | ((unsigned long)(g * 51) << 8) | (unsigned long)(b * 51);
                prefs_rgb_to_hsb(rgb, &h);
                e = chan_off(rgb, prefs_hsb_to_rgb(&h));
                if (e > worst)
                    worst = e;
            }
    CHECK(worst <= 1);
    /* the primaries sit at the wheel's sixths */
    prefs_rgb_to_hsb(0xFF0000UL, &h);
    CHECK_INT(h.hue, 0);
    CHECK_INT(h.sat == 0xFFFFFFFFUL && h.bri == 0xFFFFFFFFUL, 1);
    prefs_rgb_to_hsb(0x00FF00UL, &h);
    CHECK(h.hue > 0x54000000UL && h.hue < 0x56000000UL);   /* a third round */
    prefs_rgb_to_hsb(0x0000FFUL, &h);
    CHECK(h.hue > 0xA9000000UL && h.hue < 0xAB000000UL);   /* two thirds */
    /* a colour set by the wheel reaches the hex field as itself */
    h.hue = 0; h.sat = 0xFFFFFFFFUL; h.bri = 0x80000000UL;
    CHECK(chan_off(prefs_hsb_to_rgb(&h), 0x800000UL) <= 1);
    h.hue = 0x2AAAAAAAUL; h.sat = 0; h.bri = 0xFFFFFFFFUL;
    CHECK_INT(prefs_hsb_to_rgb(&h), 0xFFFFFF);
}

/* ---- PX11: renaming a profile ------------------------------------------------------ */

static void a_renamed_profile_keeps_its_keys_and_comments(void)
{
    static upconf c, w;
    static char buf[1024];
    const char *text = "; mine\n[profile alpha]\nfont = topaz:8\n; why\nbell = none\n\n[profile beta]\nfont = mono:9\n";
    long len;
    upconf_parse(&c, text, (long)strlen(text));
    CHECK_INT(prefs_rename_profile(&c, "ALPHA", "  [gamma] "), PREFS_RENAME_OK);
    CHECK_STR(upconf_str(&c, "gamma", "font", ""), "topaz:8");
    CHECK_STR(upconf_str(&c, "gamma", "bell", ""), "none");
    CHECK_INT(prefs_profile_exists(&c, "alpha"), 0);
    CHECK_INT(c.nprof, 2);
    len = upconf_save(&c, buf, sizeof(buf) - 1);
    buf[len] = 0;
    CHECK(strstr(buf, "[profile gamma]\nfont = topaz:8\n; why\nbell = none") != 0);
    CHECK(strstr(buf, "; mine") != 0);
    CHECK(strstr(buf, "alpha") == 0);
    /* the refusals leave the table as it was */
    CHECK_INT(prefs_rename_profile(&c, "nosuch", "x"), PREFS_RENAME_MISSING);
    CHECK_INT(prefs_rename_profile(&c, "gamma", "  [] "), PREFS_RENAME_EMPTY);
    CHECK_INT(prefs_rename_profile(&c, "gamma", "BETA"), PREFS_RENAME_EXISTS);
    CHECK_INT(prefs_rename_profile(&c, "gamma", "Gamma"), PREFS_RENAME_OK);   /* its own name, other case */
    CHECK_STR(c.prof[0], "Gamma");
    CHECK_STR(upconf_str(&c, "gamma", "font", ""), "topaz:8");
    (void)w;
}

/* Alt is Meta unless the file says meta = amiga (Left Amiga is Super) */
static void meta_defaults_to_alt(void)
{
    static upconf conf;
    prefs_fields f;
    static const char empty[] = "[profile default]\nfont = TOPAZ 8.8.font\n";
    static const char amiga[] = "[profile default]\nmeta = amiga\n";
    static const char alt[] = "[profile default]\nmeta = ALT\n";
    prefs_defaults(&f);
    CHECK_INT(f.meta_alt, 1);
    CHECK_INT(prefs_load(&conf, empty, (long)strlen(empty), UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    CHECK_INT(f.meta_alt, 1);
    CHECK_INT(prefs_load(&conf, amiga, (long)strlen(amiga), UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    CHECK_INT(f.meta_alt, 0);
    CHECK_INT(prefs_load(&conf, alt, (long)strlen(alt), UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    CHECK_INT(f.meta_alt, 1);
}

void suite_prefs(void)
{
    meta_defaults_to_alt();
    a_renamed_profile_keeps_its_keys_and_comments();
    wheel_colours_round_trip_within_one_step();
    the_window_fits_an_ntsc_and_a_pal_workbench();
    every_gadget_has_help_and_a_unique_shortcut();
    kingcon_style_letters_round_trip_through_the_handlers_function();
    colour_fields_are_validated_strictly_and_named();
    font_requester_results_become_profile_values();
    scrollback_and_screen_values_read_back_sanely();
    an_edit_of_any_field_makes_the_profile_differ();
    theme_requester_opens_the_themes_drawer();
    theme_list_from_a_drawer();
    theme_cases();
    install_cases();
    install_restore_case();
    load_cases();
    stage_cases();
    keep_cases();
    amiga_keys_and_highlight_cat_round_trip();
    style_stage_sets_only_the_look();
    dither_and_padding_color_round_trip();
    the_brightness_slider_gets_a_uword_pen_list_ended_by_ffff();
}
