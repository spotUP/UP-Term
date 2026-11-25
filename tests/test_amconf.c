/* Patina's own settings file (config/amconf_core.c): the key table, parse, serialize, the UP-Term file as the
 * legacy source, the load chain, the migration and the shared install path, against a file table in memory. */
#include <stdio.h>
#include <string.h>
#include "../config/amconf_core.h"
#include "../prefs/settings_def.h"
#include "../prefs/prefs_ui.h"
#include "harness.h"
#include "memfs.h"

#define AP "ENVARC:Patina/prefs"
#define EP "ENV:Patina/prefs"
#define UP "ENV:up-term/up-term"
#define UA "ENVARC:up-term/up-term"

static amconf_io io_of(memfs *m, prefs_fs *fs)
{
    amconf_io io;
    mf_init(m, fs);
    io.fs = fs;
    io.read = mf_read;
    io.dir = mf_dir;
    io.alloc = mf_prefs_alloc;
    io.release = mf_prefs_release;
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
    /* the table lists twenty-five keys, each named once */
    CHECK_INT(AM_NKEYS, 31);
    CHECK_STR(am_keys[AM_SCREENTHEMES].name, "screen-themes");   /* hidden: no Prefs gadget, default on */
    CHECK_STR(am_keys[AM_SCREENTHEMES].def, "on");
    CHECK_STR(am_keys[AM_WELCOME].name, "welcome-shown");   /* hidden: no Prefs gadget, default off */
    CHECK_STR(am_keys[AM_WELCOME].def, "off");
    CHECK_STR(p.val[AM_ICONSTYLE], "filled");          /* s28: Filled is the default icon set */
    CHECK_INT(amconf_icon_outline(&p), 0);
    strcpy(p.val[AM_ICONSTYLE], "outline");
    CHECK_INT(amconf_icon_outline(&p), 1);
    strcpy(p.val[AM_ICONSTYLE], "bogus");
    CHECK_INT(amconf_icon_outline(&p), 0);
    strcpy(p.val[AM_ICONSTYLE], "filled");
    /* FILENAME-PEN: icon-text, plain is the default; outline 1, shadow 2, anything else plain */
    CHECK_STR(p.val[AM_ICONTEXT], "plain");
    CHECK_INT(amconf_icon_text(&p), 0);
    strcpy(p.val[AM_ICONTEXT], "outline");
    CHECK_INT(amconf_icon_text(&p), 1);
    strcpy(p.val[AM_ICONTEXT], "shadow");
    CHECK_INT(amconf_icon_text(&p), 2);
    strcpy(p.val[AM_ICONTEXT], "bogus");
    CHECK_INT(amconf_icon_text(&p), 0);
    strcpy(p.val[AM_ICONTEXT], "plain");
    for (k = 0; k < AM_NKEYS; k++) {
        int j;
        for (j = k + 1; j < AM_NKEYS; j++)
            CHECK(strcmp(am_keys[k].name, am_keys[j].name) != 0);
    }
    {   /* every key is read by its own name */
        static const char txt[] =
            "[profile default]\nmenu-style = Text\npatina-app-icons = none\npatina-menu-font = topaz.font:11\n"
            "patina-music-drawers = Work:Music\npatina-demo-drawers = Work:Demos\npatina-hide-drawers = Sys:Tools\n"
            "patina-index-skip = Work:Tmp\npatina-index-volumes = Net:\n";
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

/* shot-dir (CAP's drawer): default RAM:Shots, the file's value wins, empty -> default, survives the file round trip */
static void test_amconf_shot_dir(void)
{
    static const char txt[] = "[profile default]\nshot-dir = Work:Pics\n";
    am_prefs p;
    char out[256];
    amconf_parse("", 0, &p);
    CHECK_STR(am_keys[AM_SHOTDIR].name, "shot-dir");
    CHECK_STR(amconf_shot_dir(&p), "RAM:Shots");
    amconf_parse(txt, (long)strlen(txt), &p);
    CHECK_STR(amconf_shot_dir(&p), "Work:Pics");
    CHECK(amconf_serialize(&p, out, sizeof out) > 0);
    amconf_defaults(&p);
    amconf_parse(out, (long)strlen(out), &p);
    CHECK_STR(amconf_shot_dir(&p), "Work:Pics");
    p.val[AM_SHOTDIR][0] = 0;
    CHECK_STR(amconf_shot_dir(&p), "RAM:Shots");
}

/* the desktop theme switches: master on by default (S32-DESKTOP-THEME-DEFAULT: a fresh install writes pointer.prefs and icontrol.prefs), the parts on under it, the mask follows desk_core's target order */
static void test_amconf_desk(void)
{
    static const char on[] = "[profile default]\ndesktop-theme = on\ndesktop-pointer = off\n";
    am_prefs p;
    char out[512];
    amconf_parse("", 0, &p);
    CHECK_STR(am_keys[AM_DESK].name, "desktop-theme");
    CHECK_STR(am_keys[AM_DESKLBL].name, "desktop-labels");
    CHECK_STR(p.val[AM_DESK], "on");
    CHECK(amconf_desk_mask(&p) & 0x4u);   /* bit DT_POINTER: the default writes pointer.prefs */
    amconf_parse(on, (long)strlen(on), &p);
    CHECK_INT(amconf_desk_mask(&p), 0xbu); /* palette, wbpattern, font; pointer off */
    CHECK(amconf_serialize(&p, out, sizeof out) > 0);
    amconf_defaults(&p);
    amconf_parse(out, (long)strlen(out), &p);
    CHECK_INT(amconf_desk_mask(&p), 0xbu);
    strcpy(p.val[AM_DESK], "off");
    CHECK_INT(amconf_desk_mask(&p), 0u);
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

/* ai-agent: which agent Super + L opens; default claude, unknown values read as claude, a default is not written */
static void test_amconf_ai_agent(void)
{
    am_prefs p;
    char txt[512];
    amconf_parse("", 0, &p);
    CHECK_STR(am_keys[AM_AGENT].name, "ai-agent");
    CHECK_STR(amconf_agent(&p), "claude");
    strcpy(p.val[AM_AGENT], "pi");
    CHECK_STR(amconf_agent(&p), "pi");
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);
    CHECK(strstr(txt, "ai-agent = pi") != 0);
    amconf_parse(txt, (long)strlen(txt), &p);
    CHECK_STR(amconf_agent(&p), "pi");
    amconf_defaults(&p);
    memset(txt, 0, sizeof txt); /* serialize does not terminate */
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);
    CHECK(strstr(txt, "ai-agent") == 0);
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
    CHECK(strstr(buf, "patina-") == 0 && strstr(buf, "menu-style") == 0); /* all defaults: no key written */
    amconf_parse(buf, n, &q);
    CHECK(same_prefs(&p, &q));
    strcpy(p.val[AM_STYLE], "minimal");
    strcpy(p.val[AM_FONT], "topaz.font:8");
    strcpy(p.val[AM_SKIP], "Work:Tmp");
    n = amconf_serialize(&p, buf, sizeof buf);
    CHECK(strstr(buf, "menu-style = minimal") != 0);
    CHECK(strstr(buf, "patina-app-icons") == 0); /* still at its default: absent */
    amconf_parse(buf, n, &q);
    CHECK(same_prefs(&p, &q));
    CHECK_INT(amconf_serialize(&p, buf, 10), -1);
}

static void test_amconf_legacy(void)
{
    static const char old[] =
        "; UP-Term\n[profile default]\nfont = topaz.font:8\nmenu-style = text\npatina-index-volumes = Net:\n"
        "[profile ansi]\nmenu-style = minimal\npatina-app-icons = none\n";
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
/* The Patina font row's chooser result (ASL name + size, a TrueType face included) reaches the spec
 * patina-gui's mt_open parses: compose -> field -> amconf file -> parse gives the same name and size. */
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
    CHECK(strstr(buf, "patina-menu-font = Vera Mono:14") != 0);
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
    mf_put(&m, AP, "patina-index-skip = Work:Tmp,Ram:\npatina-index-volumes = Net:,Srv:\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_ENVARC);
    CHECK_STR(p.val[AM_SKIP], "Work:Tmp,Ram:");
    CHECK_STR(p.val[AM_VOLUMES], "Net:,Srv:");
}

static void test_prefs_strip_legacy_patina(void)
{
    static upconf c;
    const char *txt = "[profile default]\nfont = topaz.font:8\npatina-music-drawers = Mod\n"
                      "patina-index-skip = Ram:\nmenu-style = text\n";
    CHECK_INT(upconf_parse(&c, txt, (long)strlen(txt)), 1);
    CHECK_INT(amconf_strip_legacy(&c), 3);
    CHECK(upconf_str(&c, "default", "patina-music-drawers", 0) == 0);
    CHECK(upconf_str(&c, "default", "menu-style", 0) == 0);
    CHECK_STR(upconf_str(&c, "default", "font", ""), "topaz.font:8");
    CHECK_INT(amconf_strip_legacy(&c), 0);
}

/* The reported symptom: ENVARC:Patina/prefs exists, so an edit of the Patina page that went into the
 * UP-Term profile was never read. The page now loads from and saves to Patina's file; the profile holds no
 * Patina key afterwards. Drives the editor's path: load, edit a field, amconf_save_fields_with, prefs_stage,
 * prefs_save_at on the UP-Term paths. */
static void prefs_patina_page_edit_reaches_patina_prefs(void)
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
    mf_put(&m, AP, "patina-music-drawers = Mod\n");
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
    /* values only read from the old UP-Term file are moved into Patina's file before the profile drops them */
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
    mf_put(&m, AP, "menu-style = graphical\npatina-app-icons = none\n");
    CHECK_INT(amconf_load_with(&io, &p), AMCONF_ENVARC);
    CHECK_INT(amconf_icons(&p), UC_ICONS_NONE);
    mf_put(&m, EP, "patina-app-icons = amiga\n");
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
static void test_patina_notify_env_only(void)
{
    int i, keymap = 0, user = 0, own = 0;
    for (i = 0; i < PREFS_NNOTIFY_PATINA; i++) {
        const char *n = prefs_notify_patina[i];
        CHECK(strncmp(n, "ENVARC:", 7) != 0);
        keymap += strcmp(n, "ENV:up-term/keymap") == 0;
        user += strcmp(n, "ENV:up-term/keymap.user") == 0;
        own += strcmp(n, prefs_paths_patina.path[PREFS_T_ENV]) == 0;
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
    set[1] = &prefs_paths_patina;
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

static void test_prefs_dos_dir_patina(void)
{
    memfs m;
    prefs_fs fs;
    int failed, t;
    mf_init(&m, &fs);
    g_dirs_seen = 0;
    CHECK_INT(prefs_save_at(&fs, note_dir, &prefs_paths_patina, "x", 1, 1, &failed), PREFS_INSTALL_OK);
    CHECK_INT(g_dirs_seen, 2);
    CHECK_STR(g_dir_last[PREFS_T_ENV], "ENV:Patina");
    CHECK_STR(g_dir_last[PREFS_T_ENVARC], "ENVARC:Patina");
    g_dirs_seen = 0;
    prefs_save_at(&fs, note_dir, &prefs_paths_patina, "x", 1, 0, &failed); /* Use: ENV: only */
    CHECK_INT(g_dirs_seen, 1);
    for (t = 0; t < 2; t++) {
        CHECK(strcmp(prefs_paths_patina.dir[t], prefs_paths_upterm.dir[t]) != 0);
        CHECK(strcmp(prefs_paths_patina.path[t], prefs_paths_upterm.path[t]) != 0);
    }
    CHECK(strcmp(prefs_paths_patina.tmp, prefs_paths_upterm.tmp) != 0);
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

/* LF_SETTING (plan 2026-10-26-patina-prefs-inline.md, M3): Enter on a check row writes UP-Term's file and the
 * reloaded fields show the new value; the profile's other keys and the highlight-cat variable go with it. */
/* FLAT-C5: each row of the Flat Look page writes its key (flat-system-look, flat-window-gadgets,
 * flat-border-size) through the editor's fields, and reads it back. */
static void test_amconf_flat_rows_write_their_keys(void)
{
    static const struct { int key, gadget; const char *name, *on; } row[5] = {
        { AM_FLAT, PG_AM_FLAT, "flat-system-look", "off" },
        { AM_FLATGAD, PG_AM_FLATGAD, "flat-window-gadgets", "off" },
        { AM_FLATSIZE, PG_AM_FLATSIZE, "flat-border-size", "thin" },
        { AM_ICONSTYLE, PG_AM_ICONSTYLE, "icon-style", "outline" },
        { AM_ICONTEXT, PG_AM_ICONTEXT, "icon-text", "outline" } };
    int i;
    for (i = 0; i < 5; i++) {
        am_prefs p, q;
        prefs_fields f;
        static char txt[UC_MAX_FILE + 1];
        char want[64];
        const sd_setting *sd = sd_find_gadget(row[i].gadget);
        CHECK(sd != 0 && pg_find(row[i].gadget) != 0);
        if (sd) {
            CHECK_STR(sd->key, row[i].name);
            CHECK_INT(sd->page, PG_PAGE_FLAT);
        }
        CHECK_STR(am_keys[row[i].key].name, row[i].name);
        memset(&f, 0, sizeof f);
        amconf_defaults(&p);
        amconf_to_fields(&p, &f);
        amconf_from_fields(&p, &f);
        CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
        CHECK(strstr(txt, row[i].name) == 0);
        (&f.am_flat)[i] = i < 2 ? 0 : 1; /* the two flat switches default on: the row writes the non-default off */
        amconf_from_fields(&p, &f);
        CHECK_STR(p.val[row[i].key], row[i].on);
        CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
        strcpy(want, row[i].name);
        strcat(want, " = ");
        strcat(want, row[i].on);
        CHECK(strstr(txt, want) != 0);
        amconf_parse(txt, (long)strlen(txt), &q);
        memset(&f, 0, sizeof f);
        amconf_to_fields(&q, &f);
        CHECK_INT((&f.am_flat)[i], i < 2 ? 0 : 1);
    }
}

/* AM-BORDERBLANK: border-blank is on by default (absent from the file), its Flat Look check row writes "off". */
static void test_amconf_borderblank_row(void)
{
    am_prefs p, q;
    prefs_fields f;
    static char txt[UC_MAX_FILE + 1];
    const sd_setting *sd = sd_find_gadget(PG_AM_BORDERBLANK);
    CHECK(sd != 0 && pg_find(PG_AM_BORDERBLANK) != 0);
    if (sd) {
        CHECK_STR(sd->key, "border-blank");
        CHECK_INT(sd->page, PG_PAGE_FLAT);
    }
    CHECK_STR(am_keys[AM_BORDERBLANK].name, "border-blank");
    amconf_defaults(&p);
    CHECK_STR(p.val[AM_BORDERBLANK], "on");
    memset(&f, 0, sizeof f);
    amconf_to_fields(&p, &f);
    CHECK_INT(f.am_borderblank, 1);
    amconf_from_fields(&p, &f);
    CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
    CHECK(strstr(txt, "border-blank") == 0);
    f.am_borderblank = 0;
    amconf_from_fields(&p, &f);
    CHECK_STR(p.val[AM_BORDERBLANK], "off");
    CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
    CHECK(strstr(txt, "border-blank = off") != 0);
    amconf_parse(txt, (long)strlen(txt), &q);
    amconf_to_fields(&q, &f);
    CHECK_INT(f.am_borderblank, 0);
}

/* AA-1: ui-antialias is on by default (absent from the file); off parses and round-trips; its Flat Look check row exists. */
static void test_amconf_uiaa_key(void)
{
    am_prefs p, q;
    prefs_fields f;
    static char txt[UC_MAX_FILE + 1];
    static const char off[] = "[profile default]\nui-antialias = off\n";
    CHECK_STR(am_keys[AM_UIAA].name, "ui-antialias");
    CHECK(sd_find_gadget(PG_AM_UIAA) != 0 && pg_find(PG_AM_UIAA) != 0);
    amconf_defaults(&p);
    CHECK_STR(p.val[AM_UIAA], "on");
    CHECK_INT(amconf_flat_index(&p, AM_UIAA), 1);
    amconf_parse(off, (long)strlen(off), &q);
    CHECK_STR(q.val[AM_UIAA], "off");
    CHECK_INT(amconf_flat_index(&q, AM_UIAA), 0);
    memset(&f, 0, sizeof f);
    amconf_to_fields(&q, &f);
    CHECK_INT(f.am_uiaa, 0);
    amconf_from_fields(&p, &f);
    CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
    CHECK(strstr(txt, "ui-antialias = off") != 0);
}

/* PALETTE-BUTTONS-BEVELED: flat-private-screens is on by default; off parses and round-trips; its Flat Look check row exists. */
static void test_amconf_flatpriv_key(void)
{
    am_prefs p, q;
    prefs_fields f;
    static char txt[UC_MAX_FILE + 1];
    static const char off[] = "[profile default]\nflat-private-screens = off\n";
    CHECK_STR(am_keys[AM_FLATPRIV].name, "flat-private-screens");
    CHECK(sd_find_gadget(PG_AM_FLATPRIV) != 0 && pg_find(PG_AM_FLATPRIV) != 0);
    amconf_defaults(&p);
    CHECK_INT(amconf_flat_index(&p, AM_FLATPRIV), 1);
    amconf_parse(off, (long)strlen(off), &q);
    CHECK_INT(amconf_flat_index(&q, AM_FLATPRIV), 0);
    memset(&f, 0, sizeof f);
    amconf_to_fields(&q, &f);
    CHECK_INT(f.am_flatpriv, 0);
    amconf_from_fields(&p, &f);
    CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
    CHECK(strstr(txt, "flat-private-screens = off") != 0);
}

/* INSTALLER-FLAT-OFF: the installer writes ENVARC:Patina/prefs with the header only, so the key defaults are the
 * look a fresh install shows. A header-only file means the flat look and flat window gadgets on (the thin border
 * size stays stock: it changes the window geometry). */
static void test_amconf_header_only_prefs_is_flat_on(void)
{
    am_prefs p;
    static const char hdr[] = "; Patina preferences.\n";
    amconf_parse(hdr, (long)strlen(hdr), &p);
    CHECK_INT(amconf_flat_index(&p, AM_FLAT), 1);
    CHECK_INT(amconf_flat_index(&p, AM_FLATGAD), 1);
    CHECK_INT(amconf_flat_index(&p, AM_FLATSIZE), 0);
    CHECK_INT(amconf_desk_flat_pens(&p), 1);
}

/* FLAT-SOLID-FILL: a prefs file saved before the key went away still holds "flat-glyph-style = line"; it is ignored
 * (no error, no effect on the other keys) and the next save drops it. */
static void test_amconf_old_flat_glyph_style_ignored(void)
{
    am_prefs p;
    prefs_fields f;
    static char txt[UC_MAX_FILE + 1];
    static const char old[] = "[profile default]\nflat-window-gadgets = off\nflat-glyph-style = line\nflat-border-size = thin\n";
    amconf_parse(old, (long)strlen(old), &p);
    CHECK_STR(p.val[AM_FLATGAD], "off");
    CHECK_STR(p.val[AM_FLATSIZE], "thin");
    memset(&f, 0, sizeof f);
    amconf_to_fields(&p, &f);
    amconf_from_fields(&p, &f);
    CHECK(amconf_serialize(&p, txt, UC_MAX_FILE) > 0);
    CHECK(strstr(txt, "flat-glyph-style") == 0);
    CHECK(strstr(txt, "flat-window-gadgets = off") != 0);
}

/* S90: UP-Term Prefs' path (gt_term_fields / gt_term_store on the edited fields): the colour rows show the theme's colours, and only
 * the changed ones go to the theme's user copy and GT_TERM_FILE; no theme loads: GT_TERM_NONE, the caller writes the profile */
static void test_prefs_term_theme(void)
{
    memfs m;
    prefs_fs fs;
    amconf_io io = io_of(&m, &fs);
    static prefs_fields f;
    const char *shipped = "mode = \"dark\"\nbackground = \"#2e3440\"\nforeground = \"#d8dee9\"\nred = \"#bf616a\"\n";
    char before[UC_MAX_FILE + 2];
    mf_put(&m, UP, "[default]\nfont = topaz\nfg = 112233\n");
    snprintf(before, sizeof before, "%s", mf_get(&m, UP));
    prefs_defaults(&f);
    CHECK_INT(gt_term_fields(io.read, fs.ctx, "nord", &f), 0);          /* no such theme: untouched */
    CHECK_INT(gt_term_store(io.read, &fs, "nord", &f), GT_TERM_NONE);
    mf_put(&m, "UP-Term:Patina/Themes/nord.theme", shipped);
    CHECK_INT(gt_term_fields(io.read, fs.ctx, "nord", &f), 1);
    CHECK_STR(f.fg, "D8DEE9");
    CHECK_INT(gt_term_store(io.read, &fs, "nord", &f), GT_TERM_OK);     /* nothing edited: nothing written */
    CHECK(!fs.exists(fs.ctx, "ENVARC:Patina/themes/nord.theme"));
    strcpy(f.fg, "AABBCC");
    strcpy(f.pal[5], "AA11BB");
    CHECK_INT(gt_term_store(io.read, &fs, "nord", &f), GT_TERM_OK);
    CHECK(strstr(mf_get(&m, "ENVARC:Patina/themes/nord.theme"), "term_fg = AABBCC") != 0);
    CHECK(strstr(mf_get(&m, "ENV:up-term/theme-active"), "fg = AABBCC") != 0);
    CHECK(strstr(mf_get(&m, "ENV:up-term/theme-active"), "5,AA11BB") != 0);
    CHECK(strstr(mf_get(&m, "ENVARC:Patina/themes/nord.theme"), "term_bg") == 0);   /* unedited colours stay the theme's */
    CHECK_STR(mf_get(&m, "UP-Term:Patina/Themes/nord.theme"), shipped);
    CHECK_STR(mf_get(&m, UP), before);                                  /* profile untouched */
}

void suite_amconf(void)
{
    test_amconf_parse_defaults();
    test_amconf_roundtrip();
    test_amconf_tile_gap();
    test_amconf_shot_dir();
    test_amconf_desk();
    test_amconf_ai_agent();
    test_amconf_tile_layout_keys();
    test_amconf_legacy();
    test_amconf_style();
    test_amconf_gtheme_fields();
    test_amconf_ttf_font_roundtrip();
    test_amconf_index_keys();
    test_prefs_strip_legacy_patina();
    prefs_patina_page_edit_reaches_patina_prefs();
    test_amconf_migrate_rule();
    test_amconf_load_chain();
    test_amconf_migrate_idempotent();
    test_amconf_install_both_sets();
    test_patina_notify_env_only();
    test_prefs_dos_dir_patina();
    test_amconf_save_nodir();
    test_amconf_flat_rows_write_their_keys();
    test_amconf_header_only_prefs_is_flat_on();
    test_amconf_borderblank_row();
    test_amconf_uiaa_key();
    test_amconf_flatpriv_key();
    test_amconf_old_flat_glyph_style_ignored();
    test_prefs_term_theme();
}
