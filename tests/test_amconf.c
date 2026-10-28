/* Amarchy's own settings file (amarchy/amconf_core.c): the key table, parse, serialize, the UP-Term file as the
 * legacy source, the load chain, the migration and the shared install path, against a file table in memory. */
#include <stdio.h>
#include <string.h>
#include "../amarchy/amconf_core.h"
#include "../amarchy/leaf.h"
#include "harness.h"
#include "memfs.h"

#define AP "ENVARC:Amarchy/prefs"
#define EP "ENV:Amarchy/prefs"
#define UP "ENV:up-term/up-term"
#define UA "ENVARC:up-term/up-term"

static amconf_io io_of(memfs *m, prefs_fs *fs)
{
    amconf_io io;
    mf_init(m, fs);
    io.fs = fs;
    io.read = mf_read;
    io.dir = mf_dir;
    return io;
}

static void test_amconf_parse_defaults(void)
{
    am_prefs p;
    int k;
    /* an empty file, and a file with other keys only, give every default */
    amconf_parse("", 0, &p);
    for (k = 0; k < AM_NKEYS; k++)
        CHECK_STR(p.val[k], am_keys[k].def);
    amconf_parse("[profile default]\nfont = x\n", 26, &p);
    for (k = 0; k < AM_NKEYS; k++)
        CHECK_STR(p.val[k], am_keys[k].def);
    CHECK_INT(amconf_icons(&p), UC_ICONS_GENERIC);
    CHECK_INT(amconf_style(&p), UC_MENU_GRAPHICAL);
    /* the table lists fourteen keys, each named once */
    CHECK_INT(AM_NKEYS, 14);
    for (k = 0; k < AM_NKEYS; k++) {
        int j;
        for (j = k + 1; j < AM_NKEYS; j++)
            CHECK(strcmp(am_keys[k].name, am_keys[j].name) != 0);
    }
    {   /* every key is read by its own name */
        static const char txt[] =
            "[profile default]\nmenu-style = Text\namarchy-app-icons = none\namarchy-menu-font = topaz.font:11\n"
            "amarchy-music-drawers = Work:Music\namarchy-demo-drawers = Work:Demos\namarchy-hide-drawers = Sys:Tools\n"
            "amarchy-index-skip = Work:Tmp\namarchy-index-volumes = Net:\n";
        amconf_parse(txt, (long)strlen(txt), &p);
        CHECK_STR(p.val[AM_STYLE], "Text");
        CHECK_INT(amconf_style(&p), UC_MENU_TEXT);
        CHECK_INT(amconf_icons(&p), UC_ICONS_NONE);
        CHECK_STR(p.val[AM_FONT], "topaz.font:11");
        CHECK_STR(p.val[AM_MUSIC], "Work:Music");
        CHECK_STR(p.val[AM_DEMO], "Work:Demos");
        CHECK_STR(p.val[AM_HIDE], "Sys:Tools");
        CHECK_STR(p.val[AM_SKIP], "Work:Tmp");
        CHECK_STR(p.val[AM_VOLUMES], "Net:");
    }
}

static int same_prefs(const am_prefs *a, const am_prefs *b)
{
    int i;
    for (i = 0; i < AM_NKEYS; i++)
        if (strcmp(a->val[i], b->val[i]) != 0)
            return 0;
    return 1;
}

/* tile-gap (AmTile's gap): default 4, parsed by its name, 0..64 accepted, anything else the default */
static void test_amconf_tile_gap(void)
{
    static const struct { const char *val; int want; } c[] = {
        { "0", 0 }, { "12", 12 }, { "64", 64 }, { "65", 4 }, { "-1", 4 }, { "x", 4 }, { "4x", 4 }, { "", 4 },
        { "99999999999999999999", 4 },
    };
    static const char txt9[] = "[profile default]\ntile-gap = 9\n";
    am_prefs p;
    char txt[256];
    unsigned i;
    amconf_parse("", 0, &p);
    CHECK_STR(am_keys[AM_TILEGAP].name, "tile-gap");
    CHECK_STR(p.val[AM_TILEGAP], "4");
    CHECK_INT(amconf_tile_gap(&p), 4);
    amconf_parse(txt9, (long)strlen(txt9), &p);
    CHECK_STR(p.val[AM_TILEGAP], "9");
    CHECK_INT(amconf_tile_gap(&p), 9);
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        amconf_defaults(&p);
        strcpy(p.val[AM_TILEGAP], c[i].val);
        CHECK_INT(amconf_tile_gap(&p), c[i].want);
    }
    /* a value other than the default round-trips through the file */
    amconf_defaults(&p);
    strcpy(p.val[AM_TILEGAP], "8");
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);
    amconf_parse(txt, (long)strlen(txt), &p);
    CHECK_INT(amconf_tile_gap(&p), 8);
}

/* tile-layout / tile-ratio / tile-nmaster: defaults, names, limits, round trip, default not written */
static void test_amconf_tile_layout_keys(void)
{
    static const struct { const char *val; int want; } r[] = {
        { "20", 20 }, { "80", 80 }, { "65", 65 }, { "19", 55 }, { "81", 55 }, { "x", 55 }, { "", 55 }, { "-1", 55 },
        { "99999999999999999999", 55 },
    };
    static const struct { const char *val; int want; } n[] = {
        { "1", 1 }, { "8", 8 }, { "3", 3 }, { "0", 1 }, { "9", 1 }, { "x", 1 }, { "", 1 }, { "-1", 1 },
    };
    am_prefs p, q;
    char txt[256];
    unsigned i;
    amconf_parse("", 0, &p);
    CHECK_STR(am_keys[AM_TILELAYOUT].name, "tile-layout");
    CHECK_STR(am_keys[AM_TILERATIO].name, "tile-ratio");
    CHECK_STR(am_keys[AM_TILENMASTER].name, "tile-nmaster");
    CHECK_STR(amconf_tile_layout(&p), "dwindle");
    CHECK_INT(amconf_tile_ratio(&p), 55);
    CHECK_INT(amconf_tile_nmaster(&p), 1);
    for (i = 0; i < sizeof r / sizeof r[0]; i++) {
        amconf_defaults(&p);
        strcpy(p.val[AM_TILERATIO], r[i].val);
        CHECK_INT(amconf_tile_ratio(&p), r[i].want);
    }
    for (i = 0; i < sizeof n / sizeof n[0]; i++) {
        amconf_defaults(&p);
        strcpy(p.val[AM_TILENMASTER], n[i].val);
        CHECK_INT(amconf_tile_nmaster(&p), n[i].want);
    }
    amconf_defaults(&p);
    p.val[AM_TILELAYOUT][0] = 0;
    CHECK_STR(amconf_tile_layout(&p), "dwindle");
    /* a key at its default is not written */
    amconf_defaults(&p);
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);
    CHECK(strstr(txt, "tile-") == 0);
    /* non-default values round-trip through serialize + parse */
    strcpy(p.val[AM_TILELAYOUT], "grid");
    strcpy(p.val[AM_TILERATIO], "65");
    strcpy(p.val[AM_TILENMASTER], "2");
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);
    CHECK(strstr(txt, "tile-layout = grid") != 0);
    amconf_parse(txt, (long)strlen(txt), &q);
    CHECK_STR(amconf_tile_layout(&q), "grid");
    CHECK_INT(amconf_tile_ratio(&q), 65);
    CHECK_INT(amconf_tile_nmaster(&q), 2);
}

static void test_amconf_roundtrip(void)
{
    am_prefs p, q;
    char buf[1024];
    long n;
    amconf_defaults(&p);
    n = amconf_serialize(&p, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(strstr(buf, "amarchy-") == 0 && strstr(buf, "menu-style") == 0); /* all defaults: no key written */
    amconf_parse(buf, n, &q);
    CHECK(same_prefs(&p, &q));
    strcpy(p.val[AM_STYLE], "minimal");
    strcpy(p.val[AM_FONT], "topaz.font:8");
    strcpy(p.val[AM_SKIP], "Work:Tmp");
    n = amconf_serialize(&p, buf, sizeof buf);
    CHECK(strstr(buf, "menu-style = minimal") != 0);
    CHECK(strstr(buf, "amarchy-app-icons") == 0); /* still at its default: absent */
    amconf_parse(buf, n, &q);
    CHECK(same_prefs(&p, &q));
    CHECK_INT(amconf_serialize(&p, buf, 10), -1);
}

static void test_amconf_legacy(void)
{
    static const char old[] =
        "; UP-Term\n[profile default]\nfont = topaz.font:8\nmenu-style = text\namarchy-index-volumes = Net:\n"
        "[profile ansi]\nmenu-style = minimal\namarchy-app-icons = none\n";
    am_prefs p;
    amconf_legacy(old, (long)strlen(old), &p);
    CHECK_STR(p.val[AM_STYLE], "text");
    CHECK_STR(p.val[AM_VOLUMES], "Net:");
    CHECK_STR(p.val[AM_ICONS], "generic"); /* only the default section counts */
}

static void test_amconf_style(void)
{
    am_prefs p;
    const char *txt = "[profile default]\nmenu-style = minimal\nfont = x\n";
    amconf_parse(txt, (long)strlen(txt), &p);
    CHECK_INT(amconf_style(&p), UC_MENU_MINIMAL);
    amconf_defaults(&p);
    CHECK_INT(amconf_style(&p), UC_MENU_GRAPHICAL);
    strcpy(p.val[AM_STYLE], "text");
    CHECK_INT(amconf_style(&p), UC_MENU_TEXT);
    strcpy(p.val[AM_ICONS], "none");
    CHECK_INT(amconf_icons(&p), UC_ICONS_NONE);
}

/* T12: the Prefs "Menu theme" row reaches the saved key through the fields: to_fields shows the saved name, from_fields
 * stores the typed one, an emptied row falls back to the default theme */
/* The Amarchy font row's chooser result (ASL name + size, a TrueType face included) reaches the spec
 * amarchy-gui's mt_open parses: compose -> field -> amconf file -> parse gives the same name and size. */
static void test_amconf_ttf_font_roundtrip(void)
{
    am_prefs p, q;
    prefs_fields f;
    char buf[1024], name[64];
    long size = 0;
    long n;
    memset(&f, 0, sizeof f);
    amconf_defaults(&p);
    /* the menu picker composes with blanks allowed: ttfinstall names faces
     * "Vera Mono", and the A1200's ASL list offers exactly that */
    CHECK_INT(prefs_font_compose("Vera Mono.font", 14, f.am_font, sizeof f.am_font), 1);
    CHECK_STR(f.am_font, "Vera Mono:14");
    amconf_from_fields(&p, &f);
    n = amconf_serialize(&p, buf, sizeof buf);
    CHECK(strstr(buf, "amarchy-menu-font = Vera Mono:14") != 0);
    amconf_parse(buf, n, &q);
    CHECK_INT(prefs_font_parse(q.val[AM_FONT], name, sizeof name, &size), 1);
    CHECK_STR(name, "Vera Mono.font"); /* what vo_ttf_spec cuts to FONTS:Vera Mono.otag */
    CHECK_INT((int)size, 14);
}

static void test_amconf_gtheme_fields(void)
{
    am_prefs p;
    prefs_fields f;
    memset(&f, 0, sizeof f);
    amconf_defaults(&p);
    amconf_to_fields(&p, &f);
    CHECK_STR(f.am_gtheme, "tokyo-night");
    strcpy(f.am_gtheme, "nord");
    amconf_from_fields(&p, &f);
    CHECK_STR(p.val[AM_GTHEME], "nord");
    f.am_gtheme[0] = 0;
    amconf_from_fields(&p, &f);
    CHECK_STR(p.val[AM_GTHEME], "tokyo-night");
}

static void test_amconf_index_keys(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    am_prefs p;
    mf_put(&m, AP, "amarchy-index-skip = Work:Tmp,Ram:\namarchy-index-volumes = Net:,Srv:\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_ENVARC);
    CHECK_STR(p.val[AM_SKIP], "Work:Tmp,Ram:");
    CHECK_STR(p.val[AM_VOLUMES], "Net:,Srv:");
}

static void test_prefs_strip_legacy_amarchy(void)
{
    static upconf c;
    const char *txt = "[profile default]\nfont = topaz.font:8\namarchy-music-drawers = Mod\n"
                      "amarchy-index-skip = Ram:\nmenu-style = text\n";
    CHECK_INT(upconf_parse(&c, txt, (long)strlen(txt)), 1);
    CHECK_INT(amconf_strip_legacy(&c), 3);
    CHECK(upconf_str(&c, "default", "amarchy-music-drawers", 0) == 0);
    CHECK(upconf_str(&c, "default", "menu-style", 0) == 0);
    CHECK_STR(upconf_str(&c, "default", "font", ""), "topaz.font:8");
    CHECK_INT(amconf_strip_legacy(&c), 0);
}

/* The reported symptom: ENVARC:Amarchy/prefs exists, so an edit of the Amarchy page that went into the
 * UP-Term profile was never read. The page now loads from and saves to Amarchy's file; the profile holds no
 * Amarchy key afterwards. Drives the editor's path: load, edit a field, amconf_save_fields_with, prefs_stage,
 * prefs_save_at on the UP-Term paths. */
static void prefs_amarchy_page_edit_reaches_amarchy_prefs(void)
{
    static upconf conf, work;
    static char buf[UC_MAX_FILE + 1];
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    prefs_fields f;
    am_prefs am, got;
    int src, failed;
    long len;
    mf_put(&m, AP, "amarchy-music-drawers = Mod\n");
    mf_put(&m, UA, "[profile default]\nfont = topaz.font:8\nmenu-in-terminal = graphical\nmenu-style = text\n");
    src = amconf_load_with(&io, &am);
    CHECK_INT(src, AMCONF_ENVARC);
    CHECK_INT(amconf_term_menu(&am), UC_TERMMENU_TEXT);
    CHECK_INT(prefs_load(&conf, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)), UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &conf, "default");
    amconf_to_fields(&am, &f);
    CHECK_STR(f.am_music, "Mod");
    f.am_termmenu = UC_TERMMENU_GRAPHICAL;
    CHECK_INT(amconf_save_fields_with(&io, &am, &src, &f, 1, &failed), PREFS_INSTALL_OK);
    len = prefs_stage(&work, &conf, "default", &f, buf, sizeof(buf));
    CHECK(len > 0);
    CHECK_INT(prefs_save_at(io.fs, io.dir, &prefs_paths_upterm, buf, len, 1, &failed), PREFS_INSTALL_OK);
    CHECK_INT(amconf_load_with(&io, &got), AMCONF_ENV);
    CHECK_STR(got.val[AM_TERMMENU], "graphical");
    CHECK_STR(got.val[AM_MUSIC], "Mod");
    CHECK(strstr(mf_get(&m, AP), "menu-in-terminal = graphical") != 0);
    CHECK(strstr(mf_get(&m, UA), "menu-in-terminal") == 0);
    CHECK(strstr(mf_get(&m, UA), "menu-style") == 0);
    CHECK(strstr(mf_get(&m, UA), "topaz.font:8") != 0);
    /* nothing changed and nothing legacy: no write at all */
    mf_put(&m, AP, "marker\n");
    CHECK_INT(amconf_save_fields_with(&io, &am, &src, &f, 1, &failed), PREFS_INSTALL_OK);
    CHECK_STR(mf_get(&m, AP), "marker\n");
    /* values only read from the old UP-Term file are moved into Amarchy's file before the profile drops them */
    mf_put(&m, AP, "");
    am.val[AM_MUSIC][0] = 0;
    src = AMCONF_LEGACY_ENVARC;
    CHECK_INT(amconf_save_fields_with(&io, &am, &src, &f, 1, &failed), PREFS_INSTALL_OK);
    CHECK_INT(src, AMCONF_ENVARC);
}

static void test_amconf_migrate_rule(void)
{
    CHECK_INT(amconf_need_migrate(0), 1);
    CHECK_INT(amconf_need_migrate(1), 0);
}

static void test_amconf_load_chain(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    am_prefs p;
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_DEFAULTS);
    mf_put(&m, UA, "menu-style = minimal\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_LEGACY_ENVARC);
    CHECK_INT(amconf_style(&p), UC_MENU_MINIMAL);
    mf_put(&m, UP, "menu-style = text\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_LEGACY_ENV);
    CHECK_INT(amconf_style(&p), UC_MENU_TEXT);
    mf_put(&m, AP, "menu-style = graphical\namarchy-app-icons = none\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_ENVARC);
    CHECK_INT(amconf_icons(&p), UC_ICONS_NONE);
    mf_put(&m, EP, "amarchy-app-icons = amiga\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_ENV);
    CHECK_INT(amconf_icons(&p), UC_ICONS_AMIGA);
    CHECK_INT(amconf_style(&p), UC_MENU_GRAPHICAL); /* the new file wins whole: no key mixing with legacy */
}

static void test_amconf_migrate_idempotent(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    am_prefs p;
    char before[512];
    m.envarc_fallthrough = 1;
    mf_put(&m, UP, "[profile default]\nmenu-style = text\n");
    mf_put(&m, UA, "[profile default]\nmenu-style = text\n");
    CHECK_INT(amconf_migrate_with(&io), AMCONF_MIG_DONE);
    CHECK(mf_find(&m, AP) >= 0 && mf_find(&m, EP) >= 0);
    amconf_load_with(&io, &p);
    CHECK_INT(amconf_style(&p), UC_MENU_TEXT);
    /* second start: nothing written, even when the user changed the new file */
    mf_put(&m, AP, "menu-style = minimal\n");
    strcpy(before, mf_get(&m, AP));
    CHECK_INT(amconf_migrate_with(&io), AMCONF_MIG_NONE);
    CHECK_STR(mf_get(&m, AP), before);
    /* no legacy key at all: the file is still written (the marker), header only, all defaults */
    mf_init(&m, &fs);
    CHECK_INT(amconf_migrate_with(&io), AMCONF_MIG_DONE);
    CHECK(mf_find(&m, AP) >= 0);
    amconf_load_with(&io, &p);
    CHECK_INT(amconf_style(&p), UC_MENU_GRAPHICAL);
    CHECK_INT(amconf_migrate_with(&io), AMCONF_MIG_NONE);
    /* a failed write: reported, retried next start (no marker) */
    mf_init(&m, &fs);
    m.fail_write = AP ".new";
    CHECK_INT(amconf_migrate_with(&io), AMCONF_MIG_FAILED);
    CHECK_INT(mf_find(&m, AP), -1);
}

/* the commodity watches the ENV: copies only: an ENVARC: watch makes the reload race the ENV: write (error 202) */
static void test_amarchy_notify_env_only(void)
{
    int i, keymap = 0, user = 0, own = 0;
    for (i = 0; i < PREFS_NNOTIFY_AMARCHY; i++) {
        const char *n = prefs_notify_amarchy[i];
        CHECK(strncmp(n, "ENVARC:", 7) != 0);
        keymap += strcmp(n, "ENV:up-term/keymap") == 0;
        user += strcmp(n, "ENV:up-term/keymap.user") == 0;
        own += strcmp(n, prefs_paths_amarchy.path[PREFS_T_ENV]) == 0;
    }
    CHECK_INT(keymap, 1);
    CHECK_INT(user, 1);
    CHECK_INT(own, 1);
}

/* the install of C5: both path sets keep the OS 3.2 rule (ENV: in place, ENVARC: by the rename dance) */
static void test_amconf_install_both_sets(void)
{
    const prefs_paths *set[2];
    int s;
    set[0] = &prefs_paths_upterm;
    set[1] = &prefs_paths_amarchy;
    for (s = 0; s < 2; s++) {
        memfs m;
        prefs_fs fs;
        char orig[64];
        const prefs_paths *p = set[s];
        mf_init(&m, &fs);
        m.envarc_fallthrough = 1;
        mf_put(&m, p->path[PREFS_T_ENV], "old");
        mf_put(&m, p->path[PREFS_T_ENVARC], "old");
        CHECK_INT(prefs_install_target_at(&fs, p, PREFS_T_ENVARC, "new", 3), PREFS_INSTALL_OK);
        CHECK_STR(mf_get(&m, p->path[PREFS_T_ENVARC]), "new");
        CHECK(mf_find(&m, p->orig) >= 0);
        snprintf(orig, sizeof orig, "%s", p->orig);
        mf_put(&m, p->path[PREFS_T_ENV], "old");
        CHECK_INT(prefs_install_target_at(&fs, p, PREFS_T_ENV, "new", 3), PREFS_INSTALL_OK);
        CHECK_STR(mf_get(&m, p->path[PREFS_T_ENV]), "new");
        m.fail_write = p->path[PREFS_T_ENV];
        CHECK_INT(prefs_install_target_at(&fs, p, PREFS_T_ENV, "x", 1), PREFS_INSTALL_WRITE);
    }
    {   /* the wrapper is the UP-Term set */
        memfs m;
        prefs_fs fs;
        mf_init(&m, &fs);
        CHECK_INT(prefs_install_target(&fs, PREFS_T_ENV, "new", 3), PREFS_INSTALL_OK);
        CHECK_STR(mf_get(&m, "ENV:up-term/up-term"), "new");
    }
}

/* C6: the drawer step names the set's own drawers; the two sets never share a file or a drawer */
static int g_dirs_seen;
static const char *g_dir_last[2];
static int note_dir(const prefs_paths *p, int t)
{
    g_dir_last[t] = p->dir[t];
    g_dirs_seen++;
    return 1;
}

static void test_prefs_dos_dir_amarchy(void)
{
    memfs m;
    prefs_fs fs;
    int failed, t;
    mf_init(&m, &fs);
    g_dirs_seen = 0;
    CHECK_INT(prefs_save_at(&fs, note_dir, &prefs_paths_amarchy, "x", 1, 1, &failed), PREFS_INSTALL_OK);
    CHECK_INT(g_dirs_seen, 2);
    CHECK_STR(g_dir_last[PREFS_T_ENV], "ENV:Amarchy");
    CHECK_STR(g_dir_last[PREFS_T_ENVARC], "ENVARC:Amarchy");
    g_dirs_seen = 0;
    prefs_save_at(&fs, note_dir, &prefs_paths_amarchy, "x", 1, 0, &failed); /* Use: ENV: only */
    CHECK_INT(g_dirs_seen, 1);
    for (t = 0; t < 2; t++) {
        CHECK(strcmp(prefs_paths_amarchy.dir[t], prefs_paths_upterm.dir[t]) != 0);
        CHECK(strcmp(prefs_paths_amarchy.path[t], prefs_paths_upterm.path[t]) != 0);
    }
    CHECK(strcmp(prefs_paths_amarchy.tmp, prefs_paths_upterm.tmp) != 0);
}

static int no_dir(const prefs_paths *p, int t)
{
    (void)p;
    (void)t;
    return 0;
}

static void test_amconf_save_nodir(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    am_prefs p;
    int failed = -1;
    amconf_defaults(&p);
    io.dir = no_dir;
    CHECK_INT(amconf_save_with(&io, &p, 1, &failed), PREFS_SAVE_NODIR);
    CHECK_INT(failed, PREFS_T_ENVARC);
}

/* LF_SETTING (plan 2026-10-26-amarchy-prefs-inline.md, M3): Enter on a check row writes UP-Term's file and the
 * reloaded fields show the new value; the profile's other keys and the highlight-cat variable go with it. */
static void test_lf_setting_check_toggle(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *s = &sd_settings[SDI_BLINK];
    upconf c;
    prefs_fields f;
    char buf[UC_MAX_FILE + 2];
    long got;
    int failed = -1;
    mf_put(&m, UP, "[default]\nfont = topaz\n");
    CHECK_INT(lf_setting_step(&io, s, &failed), LF_SET_OK);
    got = io.read(io.fs->ctx, UP, buf, UC_MAX_FILE + 1);
    buf[got < 0 ? 0 : got] = 0;
    CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
    prefs_from_conf(&f, &c, LF_PROFILE);
    CHECK_INT(sd_index(s, &f), 1);
    CHECK(strstr(buf, "font") != 0);                         /* the other keys stay */
    CHECK(mf_get(&m, UA) != 0);                              /* ENVARC: written too (keep) */
    CHECK_STR(mf_get(&m, "ENV:up-term/highlight-cat"), "on\n");
    CHECK_INT(lf_setting_step(&io, s, &failed), LF_SET_OK);  /* toggles back */
    got = io.read(io.fs->ctx, UP, buf, UC_MAX_FILE + 1);
    buf[got] = 0;
    prefs_load(&c, buf, got, UC_MAX_FILE);
    prefs_from_conf(&f, &c, LF_PROFILE);
    CHECK_INT(sd_index(s, &f), 0);
}

static void test_lf_setting_cycle_next_and_choices(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *s = &sd_settings[SDI_AM_ICONS];
    mn_item it[8];
    am_prefs a;
    int v, failed = -1;
    CHECK_INT(s->nvalues >= 2, 1);
    CHECK_INT(lf_setting_next(s, 0), 1);
    CHECK_INT(lf_setting_next(s, s->nvalues - 1), 0);        /* wraps */
    CHECK_INT(lf_setting_next(&sd_settings[SDI_SCROLLBACK], 0), -1);
    CHECK_INT(lf_setting_step(&io, s, &failed), LF_SET_OK);  /* Amarchy's own file */
    CHECK(mf_get(&m, AP) != 0);
    amconf_load_with(&io, &a);
    CHECK_INT(sd_index(s, &a), (sd_def_index(s) + 1) % s->nvalues);
    memset(it, 0, sizeof it);
    for (v = 0; v < s->nvalues && v < 8; v++) {
        it[v].row = MN_ROW_CHOICE;
        it[v].ix = s->id;
        it[v].seg[0] = (char)('0' + v);
        it[v].seg[1] = 0;
        strcpy(it[v].label, s->values[v].text);
    }
    lf_setting_labels(&io, it, v);
    for (v = 0; v < s->nvalues && v < 8; v++)
        CHECK_INT(it[v].label[0] == '*', v == sd_index(s, &a));
    it[0].row = MN_ROW_SETTING;                              /* a setting row shows "Label: Value" */
    lf_setting_labels(&io, it, 1);
    CHECK(strchr(it[0].label, ':') != 0);
}

static void test_lf_setting_text(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *sb = &sd_settings[SDI_SCROLLBACK], *mu = &sd_settings[SDI_AM_MUSIC];
    char out[UC_MAX_VALUE + 2], before[UC_MAX_FILE + 2];
    const char *f;
    am_prefs a;
    int failed = -1;
    mf_put(&m, UP, "[default]\nfont = topaz\nscrollback = 300\n");
    mf_put(&m, UA, "[default]\nscrollback = 300\n");
    CHECK_INT(lf_setting_text(&io, sb, out, sizeof out), 1);
    CHECK_STR(out, "300");                                   /* the prompt's prefill */
    f = mf_get(&m, UP);
    snprintf(before, sizeof before, "%s", f);
    CHECK_INT(lf_setting_set_text(&io, sb, "100000", &failed), LF_SET_INVALID);   /* over 99999 */
    CHECK_INT(lf_setting_set_text(&io, sb, "12x", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_text(&io, sb, "-5", &failed), LF_SET_INVALID);
    CHECK_STR(mf_get(&m, UP), before);                       /* file bytes unchanged */
    CHECK_STR(mf_get(&m, UA), "[default]\nscrollback = 300\n");   /* and ENVARC: untouched */
    CHECK_INT(lf_setting_set_text(&io, sb, "1200", &failed), LF_SET_OK);
    lf_setting_text(&io, sb, out, sizeof out);
    CHECK_STR(out, "1200");                                  /* written and reloaded */
    CHECK(strstr(mf_get(&m, UP), "font = topaz") != 0);
    CHECK_INT(lf_setting_set_text(&io, mu, "Mods:Music", &failed), LF_SET_OK);   /* Amarchy's file */
    amconf_load_with(&io, &a);
    CHECK_STR(a.val[AM_MUSIC], "Mods:Music");
    CHECK_INT(lf_setting_set_text(&io, mu, "a\tb", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_text(&io, &sd_settings[SDI_BLINK], "1", &failed), LF_SET_KIND);
    CHECK_INT(sb->max, PREFS_SB_MAX);                        /* the row's bound is Prefs' cut */
}

/* LF_SETTING colour pickers (plan 2026-10-26-amarchy-prefs-inline.md, M5): picking palette entry 5 writes that
 * slot to ENV: and ENVARC:; a swatch takes a palette entry or a typed hex; bad hex leaves the files alone. */
static void test_lf_setting_colour_picker(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *pal = &sd_settings[SDI_PALETTE], *fg = &sd_settings[SDI_FG];
    char before[UC_MAX_FILE + 2], hex[16];
    mn_item it[20];
    upconf c;
    prefs_fields f;
    char buf[UC_MAX_FILE + 2];
    long got;
    int failed = -1, t;
    mf_put(&m, UP, "[default]\nfont = topaz\n");
    mf_put(&m, UA, "[default]\nfont = topaz\n");
    snprintf(before, sizeof before, "%s", mf_get(&m, UP));
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "zz0000", &failed), LF_SET_INVALID);   /* bad hex */
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "12345", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_colour(&io, pal, 16, "112233", &failed), LF_SET_KIND);
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, 0, &failed), LF_SET_KIND);
    CHECK_STR(mf_get(&m, UP), before);                       /* ENV: unchanged */
    CHECK_STR(mf_get(&m, UA), before);                       /* ENVARC: unchanged */
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "#aa11BB", &failed), LF_SET_OK);
    for (t = 0; t < 2; t++) {
        got = io.read(io.fs->ctx, t ? UA : UP, buf, UC_MAX_FILE + 1);
        buf[got < 0 ? 0 : got] = 0;
        CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &c, LF_PROFILE);
        CHECK_STR(f.pal[5], "AA11BB");                       /* the slot, in both files */
        CHECK_STR(f.pal[4], "");                             /* and only that slot */
        CHECK(strstr(buf, "font") != 0);
    }
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "AA11BB");
    lf_palette_hex(&io, 1, hex);
    CHECK_STR(hex, "CD0000");                                /* unset entry: xterm's built-in */
    /* a swatch picks palette entry 5, then a typed hex */
    CHECK_INT(lf_setting_set_colour(&io, fg, 5, 0, &failed), LF_SET_OK);
    got = io.read(io.fs->ctx, UP, buf, UC_MAX_FILE + 1);
    buf[got] = 0;
    prefs_load(&c, buf, got, UC_MAX_FILE);
    prefs_from_conf(&f, &c, LF_PROFILE);
    CHECK_STR(f.fg, "AA11BB");
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "nothex", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_colour(&io, fg, SD_PICK_CUSTOM, "010203", &failed), LF_SET_OK);
    lf_colour_current(&io, fg, 0, hex);
    CHECK_STR(hex, "010203");
    /* the picker level's rows: 16 entries (+ Custom for a swatch), labelled with the entry name and hex */
    memset(it, 0, sizeof it);
    it[0].row = MN_ROW_CHOICE; it[0].ix = SDI_PALETTE; strcpy(it[0].seg, "5");
    it[1].row = MN_ROW_CHOICE; it[1].ix = SDI_FG; strcpy(it[1].seg, "16");
    lf_setting_labels(&io, it, 2);
    CHECK(strstr(it[0].label, "AA11BB") != 0);
    CHECK_STR(it[1].label, "Custom...");
    /* M8: the drawn menu's chip colour per row (lf_row_chip), the text menu's hex in the label */
    {
        long chip[6];
        memset(it, 0, sizeof it);
        it[0].row = MN_ROW_SETTING; it[0].ix = SDI_FG;
        it[1].row = MN_ROW_CHOICE; it[1].ix = SDI_PALETTE; strcpy(it[1].seg, "5");
        it[2].row = MN_ROW_CHOICE; it[2].ix = SDI_FG; strcpy(it[2].seg, "1");
        it[3].row = MN_ROW_CHOICE; it[3].ix = SDI_FG; strcpy(it[3].seg, "16");
        it[4].row = MN_ROW_SETTING; it[4].ix = SDI_BLINK;
        it[5].row = MN_ROW_SETTING; it[5].ix = SDI_PALETTE;
        lf_setting_chips(&io, it, 6, chip);
        CHECK_INT(chip[0], 0x010203L);                       /* a swatch row with its colour set */
        CHECK_INT(chip[1], 0xAA11BBL);                       /* palette entry 5 as written */
        CHECK_INT(chip[2], 0xCD0000L);                       /* an unset entry: xterm's built-in */
        CHECK_INT(chip[3], -1L);                             /* Custom... has no chip */
        CHECK_INT(chip[4], -1L);                             /* a cycle row has none */
        CHECK_INT(chip[5], -1L);                             /* nor the palette row itself ("...") */
        lf_setting_labels(&io, it, 1);
        CHECK(strstr(it[0].label, "#010203") != 0);          /* text menu: the value as hex */
    }
}


/* fake requesters: a pick, or a cancel; calls counted so the test proves the fake ran */
typedef struct fake_asl { int ok, font_calls, mode_calls, fixed; char in_name[64]; long in_size; unsigned long in_id; int in_depth; int otag_calls; char otag_path[64]; } fake_asl;
static int fake_font(void *ctx, const char *name, long size, int fixed, char *out, int cap, long *osize)
{
    fake_asl *f = (fake_asl *)ctx;
    f->font_calls++;
    f->fixed = fixed;
    snprintf(f->in_name, sizeof f->in_name, "%s", name);
    f->in_size = size;
    if (!f->ok)
        return 0;
    snprintf(out, (size_t)cap, "%s", "JetBrains Mono");
    *osize = 13;
    return 1;
}
static int fake_mode(void *ctx, unsigned long id, int depth, unsigned long *oid, int *odepth)
{
    fake_asl *f = (fake_asl *)ctx;
    f->mode_calls++;
    f->in_id = id;
    f->in_depth = depth;
    if (!f->ok)
        return 0;
    *oid = 0x29000UL;
    *odepth = 6;
    return 1;
}

static int fake_otag(void *ctx, char *out, int cap)
{
    fake_asl *f = (fake_asl *)ctx;
    f->otag_calls++;
    if (!f->ok)
        return 0;
    snprintf(out, (size_t)cap, "%s", f->otag_path);
    return 1;
}

/* LF_SETTING font and screen mode pickers (plan 2026-10-26-amarchy-prefs-inline.md, M6): a picked font or mode
 * is written to ENV: and ENVARC:, a cancelled requester leaves both files byte-identical. */
static void test_lf_setting_asl_pickers(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    const sd_setting *font = &sd_settings[SDI_FONT], *mode = &sd_settings[SDI_SCREEN_MODE], *mf = &sd_settings[SDI_AM_FONT];
    fake_asl fk;
    lf_asl asl;
    char before_u[UC_MAX_FILE + 2], before_a[UC_MAX_FILE + 2], buf[UC_MAX_FILE + 2];
    upconf c;
    prefs_fields f;
    am_prefs a;
    long got;
    int failed = -1, t;
    memset(&fk, 0, sizeof fk);
    asl.font = fake_font;
    asl.mode = fake_mode;
    asl.otag = fake_otag;
    asl.ctx = &fk;
    mf_put(&m, UP, "[default]\nfont = topaz:8\nscreen-mode = 0x11000\nscreen-depth = 4\n");
    mf_put(&m, UA, "[default]\nfont = topaz:8\nscreen-mode = 0x11000\nscreen-depth = 4\n");
    /* cancelled: nothing written, in either store */
    snprintf(before_u, sizeof before_u, "%s", mf_get(&m, UP));
    snprintf(before_a, sizeof before_a, "%s", mf_get(&m, UA));
    CHECK_INT(lf_setting_pick(&io, font, &asl, &failed), LF_SET_CANCELLED);
    CHECK_STR(fk.in_name, "topaz.font");                          /* the requester starts on the current value */
    CHECK_INT(fk.in_size, 8);
    CHECK_INT(lf_setting_pick(&io, mode, &asl, &failed), LF_SET_CANCELLED);
    CHECK_INT(fk.in_id, 0x11000);
    CHECK_INT(fk.in_depth, 4);
    CHECK_INT(lf_setting_pick(&io, mf, &asl, &failed), LF_SET_CANCELLED);
    CHECK_INT(fk.font_calls, 2);                             /* the requesters ran */
    CHECK_INT(fk.mode_calls, 1);
    CHECK_STR(mf_get(&m, UP), before_u);
    CHECK_STR(mf_get(&m, UA), before_a);
    CHECK_STR(mf_get(&m, AP), "<absent>");                /* Amarchy's file never created */
    /* picked */
    fk.ok = 1;
    CHECK_INT(lf_setting_pick(&io, font, &asl, &failed), LF_SET_OK);
    CHECK_INT(fk.fixed, 1);                                  /* the terminal font asks for fixed width */
    CHECK_INT(lf_setting_pick(&io, mode, &asl, &failed), LF_SET_OK);
    for (t = 0; t < 2; t++) {
        got = io.read(io.fs->ctx, t ? UA : UP, buf, UC_MAX_FILE + 1);
        buf[got < 0 ? 0 : got] = 0;
        CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &c, LF_PROFILE);
        CHECK_STR(f.font, "JetBrains Mono:13");
        CHECK_STR(f.screenmode, "0x00029000");
        CHECK_STR(f.screendepth, "6");
    }
    /* the menu font lives in Amarchy's file, and any width may be picked */
    CHECK_INT(lf_setting_pick(&io, mf, &asl, &failed), LF_SET_OK);
    CHECK_INT(fk.fixed, 0);
    amconf_load_with(&io, &a);
    CHECK_STR(a.val[AM_FONT], "JetBrains Mono:13");
    /* the fallback font is an outline file (.otag), stored as its base name in both files */
    fk.ok = 0;
    CHECK_INT(lf_setting_pick(&io, &sd_settings[SDI_FALLBACK], &asl, &failed), LF_SET_CANCELLED);
    CHECK_INT(fk.otag_calls, 1);                             /* the file requester ran, not the font one */
    fk.ok = 1;
    strcpy(fk.otag_path, "FONTS:Symbols.txt");
    CHECK_INT(lf_setting_pick(&io, &sd_settings[SDI_FALLBACK], &asl, &failed), LF_SET_INVALID);
    strcpy(fk.otag_path, "FONTS:SymbolsNerd.otag");
    CHECK_INT(lf_setting_pick(&io, &sd_settings[SDI_FALLBACK], &asl, &failed), LF_SET_OK);
    for (t = 0; t < 2; t++) {
        got = io.read(io.fs->ctx, t ? UA : UP, buf, UC_MAX_FILE + 1);
        buf[got < 0 ? 0 : got] = 0;
        CHECK_INT(prefs_load(&c, buf, got, UC_MAX_FILE), PREFS_LOAD_OK);
        prefs_from_conf(&f, &c, LF_PROFILE);
        CHECK_STR(f.fallback, "SymbolsNerd");
    }
}

/* M7: the profile list and the theme list ------------------------------------------------------------ */
static memfs *list_fs;      /* the file table the fake drawer listing scans */

static int mem_list(const char *dir, void (*add)(void *ud, const char *file), void *ud)
{
    size_t l = strlen(dir);
    int i;
    for (i = 0; i < FS_FILES; i++)
        if (list_fs->used[i] && !strncmp(list_fs->name[i], dir, l) && list_fs->name[i][l] == '/')
            add(ud, list_fs->name[i] + l + 1);
    return 1;
}

static void test_lf_profile_and_theme_lists(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    static const char file[] =
        "# my terminals\n[profile default]\nfont = topaz.font:8\ncursor-blink = off\nscrollback = 777\n\n"
        "[profile work]\n# the office box\nfont = JetBrains Mono:13\nfg = ff8800\nbg = 102030\nscrollback = 4000\n";
    char names[256];
    upconf c;
    int failed = 0, k;
    static const char *const wkeys[] = { "font", "fg", "bg", "scrollback" };
    static const char *const wvals[] = { "JetBrains Mono:13", "ff8800", "102030", "4000" };
    io.list = mem_list;
    list_fs = &m;
    mf_put(&m, UP, file);
    mf_put(&m, UA, file);
    CHECK_INT(lf_profile_use(&io, "default"), LF_SET_OK);
    CHECK_INT(lf_profile_names(&io, names, sizeof names), 2);
    CHECK_INT(lf_profile_use(&io, "work"), LF_SET_OK);
    CHECK_STR(lf_profile(), "work");
    lf_reset();                 /* a new open of the warm server starts on "default" again */
    CHECK_STR(lf_profile(), "default");
    CHECK_STR(names, "default");
    CHECK_STR(names + 8, "work");

    /* rename keeps every key, the other profile and both files; the comment goes with it */
    CHECK_INT(lf_profile_rename(&io, "work", "  [office] ", &failed), LF_SET_OK);
    upconf_parse(&c, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)));
    CHECK(!prefs_profile_exists(&c, "work"));
    CHECK(prefs_profile_exists(&c, "office"));
    for (k = 0; k < 4; k++)
        CHECK_STR(upconf_str(&c, "office", wkeys[k], "<absent>"), wvals[k]);
    CHECK_STR(upconf_str(&c, "default", "scrollback", ""), "777");
    CHECK_STR(upconf_str(&c, "default", "cursor-blink", ""), "off");
    CHECK(strstr(mf_get(&m, UA), "# the office box") != 0);
    CHECK_STR(mf_get(&m, UP), mf_get(&m, UA));
    CHECK_INT(lf_profile_rename(&io, "nosuch", "x", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_rename(&io, "office", "DEFAULT", &failed), LF_SET_EXISTS);
    CHECK_INT(lf_profile_rename(&io, "office", " [] ", &failed), LF_SET_INVALID);

    /* renaming the active profile moves the rows with it */
    CHECK_INT(lf_profile_use(&io, "office"), LF_SET_OK);
    CHECK_STR(lf_profile(), "office");
    CHECK_INT(lf_profile_rename(&io, "office", "lab", &failed), LF_SET_OK);
    CHECK_STR(lf_profile(), "lab");

    /* new: a copy of the active profile, which becomes the active one; a taken name is refused */
    CHECK_INT(lf_profile_new(&io, "copy", &failed), LF_SET_OK);
    CHECK_STR(lf_profile(), "copy");
    upconf_parse(&c, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)));
    for (k = 0; k < 4; k++)
        CHECK_STR(upconf_str(&c, "copy", wkeys[k], "<absent>"), wvals[k]);
    CHECK_INT(lf_profile_new(&io, "LAB", &failed), LF_SET_EXISTS);
    CHECK_INT(lf_profile_new(&io, "  ", &failed), LF_SET_INVALID);
    CHECK_INT(lf_profile_names(&io, names, sizeof names), 3);

    /* delete: gone from both files; deleting the active one hands over to the first profile left */
    CHECK_INT(lf_profile_delete(&io, "copy", &failed), LF_SET_OK);
    CHECK_STR(lf_profile(), "default");
    CHECK(strstr(mf_get(&m, UP), "copy") == 0);
    CHECK_INT(lf_profile_delete(&io, "copy", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_delete(&io, "lab", &failed), LF_SET_OK);
    CHECK_INT(lf_profile_delete(&io, "default", &failed), LF_SET_LAST);
    CHECK_INT(lf_profile_names(&io, names, sizeof names), 1);

    /* themes: new holds the active profile's colours; apply, rename, delete */
    mf_put(&m, UP, file);
    mf_put(&m, UA, file);
    CHECK_INT(lf_profile_use(&io, "work"), LF_SET_OK);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 0);
    CHECK_INT(lf_theme_new(&io, "Sunset", &failed), LF_SET_OK);
    CHECK_INT(lf_theme_new(&io, "Sunset", &failed), LF_SET_EXISTS);
    CHECK_INT(lf_theme_new(&io, "a/b", &failed), LF_SET_INVALID);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 1);
    CHECK_STR(names, "Sunset");
    CHECK(strstr(mf_get(&m, "ENVARC:up-term/themes/Sunset.conf"), "ff8800") != 0);
    CHECK(strstr(mf_get(&m, "ENVARC:up-term/themes/Sunset.conf"), "scrollback") == 0);   /* colours only */
    CHECK_STR(mf_get(&m, "ENV:up-term/themes/Sunset.conf"), mf_get(&m, "ENVARC:up-term/themes/Sunset.conf"));
    CHECK_INT(lf_theme_rename(&io, "Sunset", "Dusk", &failed), LF_SET_OK);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 1);
    CHECK_STR(names, "Dusk");
    CHECK_STR(mf_get(&m, "ENV:up-term/themes/Dusk.conf") , mf_get(&m, "ENVARC:up-term/themes/Dusk.conf"));
    CHECK_INT(lf_theme_rename(&io, "Sunset", "Other", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_use(&io, "default"), LF_SET_OK);
    CHECK_INT(lf_theme_apply(&io, "Dusk", &failed), LF_SET_OK);
    upconf_parse(&c, mf_get(&m, UA), (long)strlen(mf_get(&m, UA)));
    CHECK_STR(upconf_str(&c, "default", "fg", ""), "ff8800");
    CHECK_STR(upconf_str(&c, "default", "scrollback", ""), "777");      /* the rest of the profile stays */
    CHECK_INT(lf_theme_apply(&io, "Nothing", &failed), LF_SET_MISSING);
    CHECK_INT(lf_theme_delete(&io, "Dusk", &failed), LF_SET_OK);
    CHECK_INT(lf_theme_names(&io, names, sizeof names), 0);
    CHECK_STR(mf_get(&m, "ENV:up-term/themes/Dusk.conf"), "<absent>");
    CHECK_INT(lf_theme_delete(&io, "Dusk", &failed), LF_SET_MISSING);
    CHECK_INT(lf_profile_use(&io, "default"), LF_SET_OK);
}

void suite_amconf(void)
{
    test_amconf_parse_defaults();
    test_amconf_roundtrip();
    test_amconf_tile_gap();
    test_amconf_tile_layout_keys();
    test_amconf_legacy();
    test_amconf_style();
    test_amconf_gtheme_fields();
    test_amconf_ttf_font_roundtrip();
    test_amconf_index_keys();
    test_prefs_strip_legacy_amarchy();
    prefs_amarchy_page_edit_reaches_amarchy_prefs();
    test_amconf_migrate_rule();
    test_amconf_load_chain();
    test_amconf_migrate_idempotent();
    test_amconf_install_both_sets();
    test_amarchy_notify_env_only();
    test_prefs_dos_dir_amarchy();
    test_amconf_save_nodir();
    test_lf_setting_check_toggle();
    test_lf_setting_cycle_next_and_choices();
    test_lf_setting_text();
    test_lf_setting_colour_picker();
    test_lf_setting_asl_pickers();
    test_lf_profile_and_theme_lists();
}
