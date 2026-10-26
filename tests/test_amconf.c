/* Amarchy's own settings file (amarchy/amconf_core.c): the key table, parse, serialize, the UP-Term file as the
 * legacy source, the load chain, the migration and the shared install path, against a file table in memory. */
#include <stdio.h>
#include <string.h>
#include "../amarchy/amconf_core.h"
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
    /* the table lists ten keys, each named once */
    CHECK_INT(AM_NKEYS, 10);
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
    CHECK_INT(prefs_font_compose("DejaVuSansMono.font", 14, f.am_font, sizeof f.am_font), 1);
    CHECK_STR(f.am_font, "DejaVuSansMono:14");
    amconf_from_fields(&p, &f);
    n = amconf_serialize(&p, buf, sizeof buf);
    CHECK(strstr(buf, "amarchy-menu-font = DejaVuSansMono:14") != 0);
    amconf_parse(buf, n, &q);
    CHECK_INT(prefs_font_parse(q.val[AM_FONT], name, sizeof name, &size), 1);
    CHECK_STR(name, "DejaVuSansMono.font"); /* what vo_ttf_spec cuts to FONTS:DejaVuSansMono.otag */
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

void suite_amconf(void)
{
    test_amconf_parse_defaults();
    test_amconf_roundtrip();
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
    test_prefs_dos_dir_amarchy();
    test_amconf_save_nodir();
}
