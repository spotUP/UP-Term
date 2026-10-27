/* The Amarchy GUI themes (amarchy/gthemes_core.c, menu_theme_roles_gui, the AM_GTHEME key; plan
 * 2026-10-26-amarchy-gui-themes.md checks H1..H8). The shipped files are read from dist/amarchy-themes. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/amconf_core.h"
#include "../amarchy/gthemes_core.h"
#include "../amarchy/menu_theme.h"
#include "harness.h"

#define THEME_DIR "dist/amarchy-themes"

/* name, mode, background, foreground, dark_foreground, accent: the plan's table, from Omarchy's colors.toml */
static const struct {
    const char *name;
    int light;
    unsigned long bg, fg, dim, accent;
} shipped[] = {
    { "catppuccin-latte", 1, 0xeff1f5, 0x4c4f69, 0x9ca0b0, 0x1e66f5 },
    { "catppuccin", 0, 0x1e1e2e, 0xcdd6f4, 0x6c7086, 0x89b4fa },
    { "ethereal", 0, 0x060B1E, 0xffcead, 0x6d7db6, 0x7d82d9 },
    { "everforest", 0, 0x2d353b, 0xd3c6aa, 0x4f585e, 0x7fbbb3 },
    { "flexoki-light", 1, 0xFFFCF0, 0x100F0F, 0x878580, 0x205EA6 },
    { "gruvbox", 0, 0x282828, 0xd4be98, 0x7c6f64, 0x7daea3 },
    { "hackerman", 0, 0x0B0C16, 0xddf7ff, 0x6a6e95, 0x82FB9C },
    { "jade-horse", 0, 0x142620, 0xdfede5, 0x91ad9f, 0x8bcbb1 },
    { "kanagawa", 0, 0x1f1f28, 0xdcd7ba, 0x727169, 0xdcd7ba },
    { "last-horizon", 0, 0x0c0b0c, 0xFAFCFB, 0x584e51, 0xb59790 },
    { "lumon", 0, 0x16242d, 0xd6e2ee, 0x4d86b0, 0x8bc9eb },
    { "lupine", 1, 0xfafafa, 0x212121, 0x757575, 0x3264eb },
    { "matte-black", 0, 0x121212, 0xbebebe, 0x555555, 0xe68e0d },
    { "miasma", 0, 0x222222, 0xc2c2b0, 0x555555, 0x78824b },
    { "nord", 0, 0x2e3440, 0xd8dee9, 0x667080, 0x81a1c1 },
    { "retro-82", 0, 0x05182e, 0xf6dcac, 0x3f8f8a, 0xfaa968 },
    { "ristretto", 0, 0x2c2525, 0xe6d9db, 0x72696a, 0xf38d70 },
    { "rose-pine", 1, 0xfaf4ed, 0x575279, 0x9893a5, 0x56949f },
    { "solitude", 0, 0x101315, 0xcacccc, 0x4b4e55, 0x798186 },
    { "starship", 0, 0x1c1c1e, 0xf5f5f7, 0x98989d, 0xff9f0a },
    { "tokyo-night", 0, 0x1a1b26, 0xa9b1d6, 0x565f89, 0x7aa2f7 },
    { "vantablack", 0, 0x000000, 0xffffff, 0x505050, 0x8d8d8d },
    { "white", 1, 0xffffff, 0x000000, 0xc0c0c0, 0x6e6e6e },
};
#define NSHIPPED ((int)(sizeof shipped / sizeof shipped[0]))

static long read_file(const char *path, char *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

static void load_shipped(const char *name, gtheme *t)
{
    char path[200], buf[UC_MAX_FILE];
    long n;
    snprintf(path, sizeof path, THEME_DIR "/%s.theme", name);
    n = read_file(path, buf, sizeof buf);
    CHECK(n > 0);
    gt_builtin(t);
    if (n > 0)
        gt_parse(buf, n, t);
}

/* H1: the colors.toml syntax */
static void test_gthemes_parse(void)
{
    gtheme t;
    const char *a = "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#445566\"\nforeground = \"#778899\"\n"
                    "dark_foreground = \"#aabbcc\"\nselection = \"#000001\"\n";
    const char *crlf = "# a comment\r\naccent = \"#112233\"\r\nbackground = \"#445566\"\r\n";
    gt_builtin(&t);
    gt_parse(a, (long)strlen(a), &t);
    CHECK_INT(t.accent, 0x112233);
    CHECK_INT(t.bg, 0x445566);
    CHECK_INT(t.fg, 0x778899);
    CHECK_INT(t.dim, 0xaabbcc);
    CHECK_INT(t.light, 1);
    /* CRLF, a comment line, missing keys keep the built-in tokyo-night values, no mode = dark */
    gt_builtin(&t);
    gt_parse(crlf, (long)strlen(crlf), &t);
    CHECK_INT(t.accent, 0x112233);
    CHECK_INT(t.bg, 0x445566);
    CHECK_INT(t.fg, 0xa9b1d6);
    CHECK_INT(t.dim, 0x565f89);
    CHECK_INT(t.light, 0);
    /* unknown keys and an empty file are harmless */
    gt_builtin(&t);
    gt_parse("nonsense = \"#ffffff\"\n", 21, &t);
    CHECK_INT(t.bg, 0x1a1b26);
    gt_parse("", 0, &t);
    CHECK_INT(t.accent, 0x7aa2f7);
    /* an unquoted value and a 0x value read too */
    gt_parse("background = #010203\nforeground = 0x040506\n", 42, &t);
    CHECK_INT(t.bg, 0x010203);
    CHECK_INT(t.fg, 0x040506);
}

static void test_gthemes_roles(void)
{
    gtheme t;
    uc_u32 rgb[MT_ROLES];
    int i;
    /* H2: the role map, for every shipped file; the count comes from the directory */
    int files = 0;
    DIR *d = opendir(THEME_DIR);
    struct dirent *e;
    CHECK(d != NULL);
    if (d) {
        while ((e = readdir(d)) != NULL) {
            char nm[GT_NAME];
            if (gt_name_of_file(e->d_name, nm, sizeof nm))
                files++;
        }
        closedir(d);
    }
    CHECK_INT(files, 23);
    CHECK_INT(NSHIPPED, files);
    for (i = 0; i < NSHIPPED; i++) {
        load_shipped(shipped[i].name, &t);
        menu_theme_roles_gui(&t, rgb);
        CHECK_INT(rgb[MT_BG], shipped[i].bg);
        CHECK_INT(rgb[MT_TEXT], shipped[i].fg);
        CHECK_INT(rgb[MT_DIM], shipped[i].dim);
        CHECK_INT(rgb[MT_ACCENT], shipped[i].bg);
        CHECK_INT(rgb[MT_SELBOX], shipped[i].accent);
        /* H3: each file has exactly the table's values and mode */
        CHECK_INT(t.bg, shipped[i].bg);
        CHECK_INT(t.fg, shipped[i].fg);
        CHECK_INT(t.dim, shipped[i].dim);
        CHECK_INT(t.accent, shipped[i].accent);
        CHECK_INT(t.light, shipped[i].light);
        /* H8: contrast is printed, never failed (D5): text on ground and the selected row's text on its box */
        {
            double lt = 0, lb = 0, ls = 0;
            uc_u32 c[3];
            int k, j;
            c[0] = rgb[MT_TEXT];
            c[1] = rgb[MT_BG];
            c[2] = rgb[MT_SELBOX];
            for (k = 0; k < 3; k++) {
                double ch[3], l;
                for (j = 0; j < 3; j++) {
                    double v = (double)((c[k] >> (16 - 8 * j)) & 0xFF) / 255.0;
                    ch[j] = v <= 0.03928 ? v / 12.92 : __builtin_pow((v + 0.055) / 1.055, 2.4);
                }
                l = 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
                if (k == 0)
                    lt = l;
                else if (k == 1)
                    lb = l;
                else
                    ls = l;
            }
            {
                double r1 = (lt > lb ? lt + 0.05 : lb + 0.05) / (lt > lb ? lb + 0.05 : lt + 0.05);
                double r2 = (ls > lb ? ls + 0.05 : lb + 0.05) / (ls > lb ? lb + 0.05 : ls + 0.05);
                if (r1 < 4.5 || r2 < 3.0)
                    printf("  [WARN] theme %s: text/background %.2f, selection box/background %.2f\n", shipped[i].name,
                           r1, r2);
            }
        }
    }
    /* RJ.4: every shipped file resolves MT_OK and MT_FAIL from its own green and red keys, read here from the file
     * text (not through gt_parse), and the roles differ from the core five colours' fallbacks only by the file */
    for (i = 0; i < NSHIPPED; i++) {
        char path[200], buf[UC_MAX_FILE];
        long n, v;
        const char *k;
        snprintf(path, sizeof path, THEME_DIR "/%s.theme", shipped[i].name);
        n = read_file(path, buf, sizeof buf - 1);
        CHECK(n > 0);
        if (n <= 0)
            continue;
        buf[n] = 0;
        load_shipped(shipped[i].name, &t);
        menu_theme_roles_gui(&t, rgb);
        k = strstr(buf, "\ngreen = \"#");
        CHECK(k != NULL);
        v = k ? strtol(k + 11, NULL, 16) : -1;
        CHECK_INT(rgb[MT_OK], v);
        k = strstr(buf, "\nred = \"#");
        CHECK(k != NULL);
        v = k ? strtol(k + 9, NULL, 16) : -1;
        CHECK_INT(rgb[MT_FAIL], v);
    }
    /* a file without green and red falls back to accent and foreground (not the built-in's colours) */
    gt_builtin(&t);
    {
        const char *nokeys = "accent = \"#112233\"\nforeground = \"#778899\"\n";
        gt_parse(nokeys, (long)strlen(nokeys), &t);
    }
    menu_theme_roles_gui(&t, rgb);
    CHECK_INT(rgb[MT_OK], 0x112233);
    CHECK_INT(rgb[MT_FAIL], 0x778899);
    /* few-pen screens: the two new roles fall back to the row text; the five core roles have no fallback role */
    CHECK_INT(menu_theme_fallback_role(MT_OK), MT_TEXT);
    CHECK_INT(menu_theme_fallback_role(MT_FAIL), MT_TEXT);
    for (i = 0; i <= MT_SELBOX; i++)
        CHECK_INT(menu_theme_fallback_role(i), -1);
    /* the lister palette stays the five core colours: pen numbers 0..4 are unchanged */
    {
        char out[256];
        uc_u32 r7[MT_ROLES] = { 1, 2, 3, 4, 5, 6, 7 };
        CHECK(menu_theme_dopus_commands(r7, out, sizeof out) > 0);
        CHECK(strstr(out, "dopus set palette 0x000001 0x000002 0x000003 0x000004 0x000005\n") == out);
    }
    /* minimal menu: OK and FAIL are the foreground */
    gt_builtin(&t);
    menu_theme_roles_gui(&t, rgb);
    CHECK(menu_theme_minimal(rgb) == 1);
    CHECK_INT(rgb[MT_OK], rgb[MT_TEXT]);
    CHECK_INT(rgb[MT_FAIL], rgb[MT_TEXT]);
    /* T20 knob audit: each of the four keys moves a different role */
    gt_builtin(&t);
    menu_theme_roles_gui(&t, rgb);
    {
        uc_u32 base[MT_ROLES];
        gtheme u;
        memcpy(base, rgb, sizeof base);
        u = t;
        u.bg ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_BG] != base[MT_BG] && rgb[MT_ACCENT] != base[MT_ACCENT]);
        u = t;
        u.fg ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_TEXT] != base[MT_TEXT]);
        u = t;
        u.dim ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_DIM] != base[MT_DIM]);
        u = t;
        u.accent ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_SELBOX] != base[MT_SELBOX]);
        u = t;
        u.ok ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_OK] != base[MT_OK]);
        u = t;
        u.fail ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_FAIL] != base[MT_FAIL]);
    }
    /* H7: the minimal menu over the GUI roles: ground and text only */
    load_shipped("nord", &t);
    menu_theme_roles_gui(&t, rgb);
    CHECK(menu_theme_minimal(rgb) == 1);
    CHECK_INT(rgb[MT_BG], 0x2e3440);
    CHECK_INT(rgb[MT_ACCENT], 0x2e3440);
    CHECK_INT(rgb[MT_TEXT], 0xd8dee9);
    CHECK_INT(rgb[MT_DIM], 0xd8dee9);
    CHECK_INT(rgb[MT_SELBOX], 0xd8dee9);
    /* H6: the terminal's roles are untouched: a set bg and fg still win over the palette */
    menu_theme_roles_fb(NULL, 0x01445566UL, 0x01112233UL, rgb);
    CHECK_INT(rgb[MT_BG], 0x112233);
    CHECK_INT(rgb[MT_TEXT], 0x445566);
}

typedef struct rd_ctx {
    int calls;
} rd_ctx;

static long fake_read(void *ctx, const char *path, char *buf, long cap)
{
    rd_ctx *c = (rd_ctx *)ctx;
    const char *body = NULL;
    c->calls++;
    if (!strcmp(path, GT_USER_DIR "/nord.theme"))
        body = "background = \"#000001\"\n";
    else if (!strcmp(path, GT_SHIPPED_DIR "/nord.theme"))
        body = "background = \"#000002\"\n";
    else if (!strcmp(path, GT_SHIPPED_DIR "/solo.theme"))
        body = "background = \"#000003\"\n";
    if (!body || (long)strlen(body) > cap)
        return -1;
    memcpy(buf, body, strlen(body));
    return (long)strlen(body);
}

/* H4: the user's drawer wins, the list is sorted and one row per name, the label marks the current one */
static void test_gthemes_user(void)
{
    gtheme t;
    gt_list l;
    rd_ctx c = { 0 };
    char lab[80];
    gt_load(fake_read, &c, "nord", &t);
    CHECK_INT(t.bg, 0x000001);
    gt_load(fake_read, &c, "solo", &t);
    CHECK_INT(t.bg, 0x000003);
    CHECK_STR(t.name, "solo");
    /* an unknown name: the built-in default is drawn */
    gt_load(fake_read, &c, "nonesuch", &t);
    CHECK_INT(t.bg, 0x1a1b26);
    CHECK_INT(t.accent, 0x7aa2f7);
    memset(&l, 0, sizeof l);
    gt_list_add(&l, "nord.theme", 0);
    gt_list_add(&l, "Zed.theme", 0);
    gt_list_add(&l, "gruvbox.theme", 0);
    gt_list_add(&l, "readme.txt", 0);
    gt_list_add(&l, "NORD.THEME", 1);
    gt_list_add(&l, "mine.theme", 1);
    CHECK_INT(l.n, 4);
    CHECK_STR(l.name[0], "gruvbox");
    CHECK_STR(l.name[1], "mine");
    CHECK_STR(l.name[2], "NORD");
    CHECK_STR(l.name[3], "Zed");
    CHECK_INT(l.user[0], 0);
    CHECK_INT(l.user[1], 1);
    CHECK_INT(l.user[2], 1);
    gt_label("nord", "Nord", lab, sizeof lab);
    CHECK_STR(lab, "nord (current)");
    gt_label("gruvbox", "nord", lab, sizeof lab);
    CHECK_STR(lab, "gruvbox");
    CHECK(gt_path(GT_USER_DIR, "nord", lab, sizeof lab) == 1);
    CHECK_STR(lab, "ENVARC:Amarchy/themes/nord.theme");
    CHECK(gt_path("UP-Term:", "x", lab, sizeof lab) == 1);
    CHECK_STR(lab, "UP-Term:x.theme");
}

/* H5: AM_GTHEME round trips through the settings file; the default is tokyo-night */
static void test_gthemes_amconf(void)
{
    am_prefs p, q;
    char buf[2048];
    int n;
    amconf_parse("", 0, &p);
    CHECK_STR(p.val[AM_GTHEME], GT_DEFAULT);
    CHECK_STR(am_keys[AM_GTHEME].name, "amarchy-gui-theme");
    strcpy(p.val[AM_GTHEME], "nord");
    n = (int)amconf_serialize(&p, buf, sizeof buf);
    CHECK(n > 0);
    amconf_parse(buf, n, &q);
    CHECK_STR(q.val[AM_GTHEME], "nord");
}


/* fake drawers: the shipped one holds nord and solo, the user's holds nord (an override) and mine */
static void fake_dir(void *ctx, const char *dir, gt_emit_fn emit, void *ectx)
{
    int *empty = (int *)ctx;
    if (*empty)
        return;
    if (!strcmp(dir, GT_SHIPPED_DIR)) {
        emit(ectx, "solo.theme");
        emit(ectx, "nord.theme");
    } else if (!strcmp(dir, GT_USER_DIR)) {
        emit(ectx, "mine.theme");
        emit(ectx, "nord.theme");
    }
}

/* T5: gt_scan merges both drawers, the user's file wins and marks the row, an empty pair gives the default */
static void test_gthemes_scan(void)
{
    gt_list l;
    int empty = 0;
    char nm[GT_NAME];
    memset(&l, 0, sizeof l);
    gt_scan(fake_dir, &empty, &l);
    CHECK_INT(l.n, 3);
    CHECK_STR(l.name[0], "mine");
    CHECK_STR(l.name[1], "nord");
    CHECK_STR(l.name[2], "solo");
    CHECK_INT(l.user[0], 1);
    CHECK_INT(l.user[1], 1);
    CHECK_INT(l.user[2], 0);
    empty = 1;
    memset(&l, 0, sizeof l);
    gt_scan(fake_dir, &empty, &l);
    CHECK_INT(l.n, 1);
    CHECK_STR(l.name[0], GT_DEFAULT);
    CHECK(gt_path_name("gtheme:nord", nm, sizeof nm));
    CHECK_STR(nm, "nord");
    CHECK(!gt_path_name("clip:3", nm, sizeof nm));
    CHECK(!gt_path_name("gtheme:", nm, sizeof nm));
}
/* T8: the name the commodity saves is a plain theme name, never a path or an overlong string */
static void test_gthemes_select_name(void)
{
    char longname[GT_NAME + 4];
    memset(longname, 'a', sizeof longname - 1);
    longname[sizeof longname - 1] = 0;
    CHECK(gt_name_ok("nord"));
    CHECK(!gt_name_ok(""));
    CHECK(!gt_name_ok(0));
    CHECK(!gt_name_ok("../x"));
    CHECK(!gt_name_ok("ram:nord"));
    CHECK(!gt_name_ok(longname));
}

/* T13: an unreadable or unknown theme name draws the compiled-in default, never garbage or the terminal's colours */
static long no_files(void *ctx, const char *path, char *buf, long cap)
{
    (void)ctx; (void)path; (void)buf; (void)cap;
    return -1;
}

static void test_gthemes_fallback(void)
{
    gtheme t, d;
    gt_builtin(&d);
    gt_load(no_files, 0, "missing", &t);
    CHECK_INT((long)t.bg, (long)d.bg);
    CHECK_INT((long)t.accent, (long)d.accent);
    CHECK_STR(d.name, GT_DEFAULT);
    CHECK_INT((long)d.bg, 0x1a1b26L);
}

/* a failed theme save says which step and which file, with the DOS error */
static void test_gthemes_save_text(void)
{
    char t[200];
    gt_save_text(GT_SAVE_NAME, 0, 0, t, sizeof t);
    CHECK_STR(t, "the theme could not be saved: check the name of the theme");
    gt_save_text(PREFS_INSTALL_WRITE, PREFS_T_ENV, 202, t, sizeof t);
    CHECK_STR(t, "the theme could not be saved: write ENV:Amarchy/prefs, error 202");
    gt_save_text(PREFS_INSTALL_WRITE, PREFS_T_ENVARC, 103, t, sizeof t);
    CHECK(strstr(t, "write ENVARC:Amarchy/") && strstr(t, ", error 103"));
    gt_save_text(PREFS_SAVE_NODIR, PREFS_T_ENVARC, 0, t, sizeof t);
    CHECK_STR(t, "the theme could not be saved: make the drawer ENVARC:Amarchy");
    gt_save_text(PREFS_INSTALL_BACKUP, PREFS_T_ENVARC, 5, t, sizeof t);
    CHECK(strstr(t, "back up to ENVARC:Amarchy/") && strstr(t, ", error 5"));
    gt_save_text(PREFS_INSTALL_PLACE, PREFS_T_ENVARC, 5, t, sizeof t);
    CHECK(strstr(t, "rename into place ENVARC:Amarchy/prefs"));
    gt_save_text(PREFS_INSTALL_STRANDED, PREFS_T_ENVARC, 5, t, sizeof t);
    CHECK(strstr(t, "restore after a failed rename"));
}

void suite_gthemes(void)
{
    test_gthemes_save_text();
    test_gthemes_select_name();
    test_gthemes_fallback();
    test_gthemes_parse();
    test_gthemes_roles();
    test_gthemes_user();
    test_gthemes_amconf();
    test_gthemes_scan();
}
