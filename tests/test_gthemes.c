/* The Amarchy GUI themes (amarchy/gthemes_core.c, menu_theme_roles_gui, the AM_GTHEME key; plan
 * 2026-10-26-amarchy-gui-themes.md checks H1..H8). The shipped files are read from dist/amarchy-themes. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../amarchy/amconf_core.h"
#include "../amarchy/desk_core.h"
#include "../amarchy/flat_core.h"
#include "../amarchy/gthemes_core.h"
#include "../amarchy/iconpen_core.h"
#include "../amarchy/wbreset_core.h"
#include "../amarchy/leaf.h"
#include "../prefs/settings_def.h"
#include "../amarchy/menu_core.h"
#include "../amarchy/menu_theme.h"
#include "../amarchy/pen_cache.h"
#include "harness.h"
#include "memfs.h"

/* Reference oracle for the differential test: the recursive matcher as it was before TE10-MATCH-BACKTRACK. */
static int ref_lc(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static int ref_screen_match(const char *p, const char *s)
{
    while (*p) {
        if (*p == '#' && p[1]) {
            int any = (p[1] == '?');
            int ch = ref_lc((unsigned char)p[1]);
            p += 2;
            for (;;) {
                if (ref_screen_match(p, s))
                    return 1;
                if (!*s || !(any || ref_lc((unsigned char)*s) == ch))
                    return 0;
                s++;
            }
        }
        if (!*s)
            return 0;
        if (*p != '?' && ref_lc((unsigned char)*p) != ref_lc((unsigned char)*s))
            return 0;
        p++;
        s++;
    }
    return !*s;
}


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
    { "vantablack", 0, 0x000000, 0xBEBEBE, 0x505050, 0x8d8d8d },
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

/* F3: an Amarchy theme switch recolours every console tab, so the theme becomes the text theme_apply reads: one profile with
 * fg, bg and the 16-colour palette, all from the theme's colors.toml */
static void test_gthemes_term_text(void)
{
    static char buf[4096], out[1024];
    long n = read_file(THEME_DIR "/gruvbox.theme", buf, (long)sizeof buf);
    static upconf u;
    const char *names[UC_MAX_PROFILES + 1];
    int len;
    CHECK(n > 0);
    len = gt_term_text(buf, n, "gruvbox", out, (int)sizeof out);
    CHECK(len > 0);
    upconf_clear(&u);
    upconf_parse(&u, out, len);
    CHECK_INT(upconf_profiles(&u, names), 1);
    CHECK(upconf_has(&u, "gruvbox", "fg"));
    CHECK(!strcmp(upconf_get(&u, "gruvbox", "bg"), "282828"));
    CHECK(!strcmp(upconf_get(&u, "gruvbox", "fg"), "D4BE98"));
    CHECK(strstr(upconf_get(&u, "gruvbox", "palette"), "1,EA6962,2,A9B665,3,D8A657,4,7DAEA3") != 0);
    CHECK(strstr(upconf_get(&u, "gruvbox", "palette"), "15,D4BE98") != 0);
    /* two themes differ in bg (a switch changes the tab), a too-small buffer is refused */
    n = read_file(THEME_DIR "/nord.theme", buf, (long)sizeof buf);
    CHECK(gt_term_text(buf, n, "nord", out, (int)sizeof out) > 0);
    CHECK(strstr(out, "282828") == 0);
    CHECK_INT(gt_term_text(buf, n, "nord", out, 20), 0);
}

/* TE-7b.2: the Shell Font reaches the terminal text as one "font = NAME:SIZE" line in the same profile section */
static long term_font_text(char *buf, long cap, const char *shell_font)
{
    long n = snprintf(buf, (size_t)cap, "amarchy-theme = 2\nname = \"tf\"\nmode = \"dark\"\n"
                      "[profile swatches]\nbackground = #101010\nforeground = #eeeeee\naccent = #336699\n[profile look]\ncorner-radius = 4\n");
    if (shell_font)
        n += snprintf(buf + n, (size_t)(cap - n), "shell-font = \"%s\"\n", shell_font);
    return n;
}

static void test_gthemes_term_font(void)
{
    static char buf[4096], out[1024], plain[1024];
    static upconf u;
    char name[GT_NAME];
    int len, plen, i, cnt;
    const char *p;
    long n = term_font_text(buf, (long)sizeof buf, 0);
    plen = gt_term_text(buf, n, "tf", plain, (int)sizeof plain);
    CHECK(plen > 0);
    CHECK(strstr(plain, "font") == 0); /* (b) no Shell Font: no font line */
    n = term_font_text(buf, (long)sizeof buf, "XEN.font/9");
    len = gt_term_text(buf, n, "tf", out, (int)sizeof out);
    CHECK(len > 0);
    for (cnt = 0, p = out; (p = strstr(p, "font = ")) != 0; p++)
        cnt++;
    CHECK_INT(cnt, 1); /* (a) exactly one line */
    p = strstr(out, "font = ");
    CHECK(p != 0);
    if (p) {
        CHECK(strstr(out, "palette = ") < p); /* inside the profile section, after the palette line */
        CHECK_STR(p, "font = XEN.font:9\n");
    }
    CHECK(strncmp(out, plain, (size_t)plen) == 0); /* (c) the old lines byte for byte */
    CHECK_INT(len, plen + 18);
    upconf_clear(&u);
    upconf_parse(&u, out, len);
    CHECK_STR(upconf_get(&u, "tf", "font"), "XEN.font:9");
    /* a size of 0 or none: no line */
    n = term_font_text(buf, (long)sizeof buf, "XEN.font/0");
    CHECK_INT(gt_term_text(buf, n, "tf", out, (int)sizeof out), plen);
    n = term_font_text(buf, (long)sizeof buf, "XEN.font");
    CHECK_INT(gt_term_text(buf, n, "tf", out, (int)sizeof out), plen);
    /* (d) the longest font spec the model stores (look.font is 40 bytes: a 35-char name, "/999") fits the 1024 bytes of every caller */
    for (i = 0; i < 30; i++)
        name[i] = (char)('a' + i % 26);
    memcpy(name + i, ".font", 6);
    snprintf(out, sizeof out, "%s/999", name);
    n = term_font_text(buf, (long)sizeof buf, out);
    len = gt_term_text(buf, n, "a-long-theme-name-of-thirty-nine-chr", out, (int)sizeof out);
    CHECK(len > 0 && len < 1024);
    CHECK(strstr(out, ":999\n") != 0);
}

static char boot_name[64];
static int boot_calls;
static void boot_send(const char *name)
{
    strcpy(boot_name, name);
    boot_calls++;
}

/* F3-THEME-BOOT: the startup write goes out once, with the configured theme, or GT_DEFAULT when unset or unusable */
static void test_gthemes_boot_term(void)
{
    boot_calls = 0;
    CHECK_INT(gt_boot_term("gruvbox", boot_send), 1);
    CHECK_INT(boot_calls, 1);
    CHECK_STR(boot_name, "gruvbox");
    CHECK_INT(gt_boot_term("", boot_send), 1);
    CHECK_INT(boot_calls, 2);
    CHECK_STR(boot_name, GT_DEFAULT);
    gt_boot_term("a/b", boot_send);
    CHECK_STR(boot_name, GT_DEFAULT);
    gt_boot_term(0, boot_send);
    CHECK_INT(boot_calls, 4);
    CHECK_STR(boot_name, GT_DEFAULT);
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

/* THEME-TEXT-GREY (owner rule 2026-10-31): a dark theme with a #ffffff foreground gets #BEBEBE, derived colours follow */
static void test_gthemes_text_grey(void)
{
    gtheme t, g;
    const char *dark = "mode = \"dark\"\nbackground = \"#000000\"\nforeground = \"#ffffff\"\n";
    const char *nomode = "background = \"#000000\"\nforeground = \"#ffffff\"\n";
    const char *light = "mode = \"light\"\nbackground = \"#ffffff\"\nforeground = \"#000000\"\n";
    const char *lightw = "mode = \"light\"\nbackground = \"#808080\"\nforeground = \"#ffffff\"\n";
    const char *off = "mode = \"dark\"\nbackground = \"#101010\"\nforeground = \"#fffffe\"\n";
    gt_parse(dark, (long)strlen(dark), &t);
    CHECK_INT(t.fg, 0xBEBEBEUL);
    /* derived: lighter_bg is a tenth of the way from the background to the ADJUSTED foreground, fail falls back to it */
    CHECK_INT(t.lighter_bg, 0x131313UL);
    CHECK_INT(t.fail, 0xBEBEBEUL);
    gt_parse(nomode, (long)strlen(nomode), &g); /* no mode key is dark */
    CHECK_INT(g.fg, 0xBEBEBEUL);
    gt_parse(light, (long)strlen(light), &t);
    CHECK_INT(t.fg, 0x000000UL);
    CHECK_INT(t.bg, 0xffffffUL);
    gt_parse(lightw, (long)strlen(lightw), &t); /* a light theme keeps even a white foreground */
    CHECK_INT(t.fg, 0xffffffUL);
    gt_parse(off, (long)strlen(off), &t);
    CHECK_INT(t.fg, 0xfffffeUL);
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
        /* RL8 step 3: the dim role is GR_MUTED, the theme's muted pushed off the surface until it reads at GT_FLOOR_MUTED (menu dim item,
         * disabled label); a muted that already reads is kept as is */
        CHECK(gt_contrast(rgb[MT_DIM], rgb[MT_BG]) >= GT_FLOOR_MUTED);
        if (gt_contrast(t.muted, t.bg) >= GT_FLOOR_MUTED)
            CHECK_INT(rgb[MT_DIM], t.muted);
        CHECK_INT(rgb[MT_SELTEXT], shipped[i].bg);
        CHECK_INT(rgb[MT_THEME_ACCENT], shipped[i].accent);
        CHECK_INT(rgb[MT_HEAD_ROLE], shipped[i].accent); /* the header follows the theme's accent */
        /* D14: the lister's directory pen (colour 3 = accent) never equals its background pen (colour 0 = bg) */
        {
            char out[512], want[64];
            uc_u32 wb[WB_PENS];
            CHECK(t.accent != t.bg);
            CHECK(dopus_theme_commands(&t, out, sizeof out, 1) > 0);
            snprintf(want, sizeof want, "dopus set palette 0x%06lx 0x%06lx 0x%06lx 0x%06lx\n", (unsigned long)t.bg,
                     (unsigned long)t.fg, (unsigned long)t.muted, (unsigned long)t.accent);
            CHECK(strstr(out, want) == out);
            CHECK(strstr(out, "dopus set pens dirs 3 0\n") != NULL);
            /* Workbench pens 0-3: the guard's verdict for every shipped theme is the measured ratio against the
             * threshold; the pens are the theme's own */
            {
                int ok = wb_pens_plan(&t, wb);
                unsigned cr = (unsigned)gt_contrast(t.bg, t.fg);
                CHECK_INT(ok, cr >= GT_FLOOR_TEXT);
                CHECK_INT((long)wb[0], (long)t.bg);
                CHECK_INT((long)wb[3], (long)t.accent);
                if (!ok)
                    printf("  [WARN] theme %s: Workbench pens refused, contrast %u.%02u\n", shipped[i].name, cr / 100,
                           cr % 100);
            }
        }
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
            c[2] = rgb[MT_THEME_ACCENT];
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
    for (i = 0; i <= MT_THEME_ACCENT; i++)
        CHECK_INT(menu_theme_fallback_role(i), -1);
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
        CHECK(rgb[MT_BG] != base[MT_BG] && rgb[MT_SELTEXT] != base[MT_SELTEXT]);
        u = t;
        u.fg ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_TEXT] != base[MT_TEXT]);
        u = t;
        u.muted ^= 0x010101; /* RL8: MT_DIM follows muted, not dim */
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_DIM] != base[MT_DIM]);
        u = t;
        u.accent ^= 0x010101;
        menu_theme_roles_gui(&u, rgb);
        CHECK(rgb[MT_THEME_ACCENT] != base[MT_THEME_ACCENT]);
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
    CHECK_INT(rgb[MT_SELTEXT], 0x2e3440);
    CHECK_INT(rgb[MT_TEXT], 0xd8dee9);
    CHECK_INT(rgb[MT_DIM], 0xd8dee9);
    CHECK_INT(rgb[MT_THEME_ACCENT], 0xd8dee9);
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
/* T9: walking Style > Theme recolours the menu in the row's theme; leaving the list restores the saved one */
static char tp_sent[8][GT_NAME];
static int tp_n;
static void tp_send(const char *name)
{
    if (tp_n < 8)
        strcpy(tp_sent[tp_n], name);
    tp_n++;
}

/* THEME-PREVIEW-SHELL: every highlight move sends the row's theme to the console bridge; leaving the list restores the saved one */
static void test_gthemes_preview_term(void)
{
    char nm[GT_NAME], drawn[GT_NAME] = "tokyo-night";
    tp_n = 0;
    CHECK(gt_preview_term("gtheme:nord", "tokyo-night", drawn, nm, sizeof nm, tp_send));
    strcpy(drawn, nm);
    CHECK(gt_preview_term("gtheme:ethereal", "tokyo-night", drawn, nm, sizeof nm, tp_send));
    strcpy(drawn, nm);
    CHECK(gt_preview_term("clip:3", "tokyo-night", drawn, nm, sizeof nm, tp_send));    /* cancel: back to the saved theme */
    strcpy(drawn, nm);
    CHECK(!gt_preview_term("clip:3", "tokyo-night", drawn, nm, sizeof nm, tp_send));   /* nothing changed: no write */
    CHECK(tp_n == 3);
    CHECK(!strcmp(tp_sent[0], "nord") && !strcmp(tp_sent[1], "ethereal") && !strcmp(tp_sent[2], "tokyo-night"));
}

/* THEME-PREVIEW-LIVE-A1200: the text menu's picker recolours the consoles on each highlight move and Esc restores the
 * theme active before it opened; opening the menu on a non-theme row writes nothing (drawn starts as current) */
static void test_gthemes_text_menu_preview_recolours_consoles(void)
{
    char drawn[GT_NAME] = "tokyo-night";
    tp_n = 0;
    CHECK(!gt_preview_step("menu:style", "tokyo-night", drawn, sizeof drawn, tp_send));   /* menu opened: no write */
    CHECK(!gt_preview_step("gtheme:tokyo-night", "tokyo-night", drawn, sizeof drawn, tp_send)); /* list opens on (current) */
    CHECK(gt_preview_step("gtheme:nord", "tokyo-night", drawn, sizeof drawn, tp_send));   /* Down */
    CHECK_STR(drawn, "nord");
    CHECK(gt_preview_step("gtheme:ethereal", "tokyo-night", drawn, sizeof drawn, tp_send)); /* Down */
    CHECK(gt_preview_step(0, "tokyo-night", drawn, sizeof drawn, tp_send));               /* Esc: restored */
    CHECK_STR(drawn, "tokyo-night");
    CHECK(!gt_preview_step(0, "tokyo-night", drawn, sizeof drawn, tp_send));              /* menu closed: no second write */
    CHECK(tp_n == 3);
    CHECK(!strcmp(tp_sent[0], "nord") && !strcmp(tp_sent[1], "ethereal") && !strcmp(tp_sent[2], "tokyo-night"));
}

/* THEME-ENTER-BACK-ONE: Enter on a theme row saves the theme and backs out ONE level (Style), the menu stays open. The
 * closing restore then compares against the NEW saved theme, so it writes nothing and the consoles keep the new colours. */
static void test_gthemes_enter_backs_one_level(void)
{
    char up[64], drawn[GT_NAME] = "tokyo-night";
    tp_n = 0;
    CHECK(mn_pick_back("gtheme:nord", "style.theme", up, sizeof up));
    CHECK_STR(up, "style");                                   /* one level, not the root and not a closed menu */
    CHECK(!mn_pick_back("clip:3", "style.theme", up, sizeof up));   /* any other row keeps its own rules */
    CHECK(!mn_pick_back("gtheme:nord", "", up, sizeof up));          /* no parent: nothing to back to */
    CHECK(gt_preview_step("gtheme:nord", "tokyo-night", drawn, sizeof drawn, tp_send));   /* highlight: nord previewed */
    /* Enter: gt_dos_select saves nord, so the saved theme is now nord; the level change and the later close restore: */
    CHECK(!gt_preview_step("menu:style", "nord", drawn, sizeof drawn, tp_send));
    CHECK(!gt_preview_step(0, "nord", drawn, sizeof drawn, tp_send));
    CHECK(tp_n == 1 && !strcmp(tp_sent[0], "nord"));          /* never "tokyo-night" again */
}

static void test_gthemes_preview(void)
{
    char nm[GT_NAME];
    CHECK(gt_preview_pick("gtheme:nord", "tokyo-night", "tokyo-night", nm, sizeof nm));
    CHECK_STR(nm, "nord");
    CHECK(!gt_preview_pick("gtheme:nord", "tokyo-night", "nord", nm, sizeof nm));      /* already drawn */
    CHECK(!gt_preview_pick("gtheme:NORD", "tokyo-night", "nord", nm, sizeof nm));      /* without case */
    CHECK(gt_preview_pick("clip:3", "tokyo-night", "nord", nm, sizeof nm));            /* off the list: restore */
    CHECK_STR(nm, "tokyo-night");
    CHECK(gt_preview_pick(0, "tokyo-night", "nord", nm, sizeof nm));
    CHECK_STR(nm, "tokyo-night");
    CHECK(!gt_preview_pick(0, "tokyo-night", "tokyo-night", nm, sizeof nm));
    CHECK(!gt_preview_pick("gtheme:", "", "nord", nm, sizeof nm));                    /* nothing to draw */
    CHECK(!gt_preview_pick("gtheme:nord", "x", "y", nm, 4));                          /* does not fit out */
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

/* TE-10.2: the screen rules (ENVARC:Amarchy/screen-themes): parse, write, resolve one public screen name */
static int scr_theme(const gt_screen_rules *r, const char *name, const char *main_theme, char *th, int *take)
{
    return gt_screen_theme_of(r, name, main_theme, th, GT_NAME, take);
}

static void test_gthemes_screen_rules(void)
{
    gt_screen_rules r, r2;
    char th[GT_NAME], buf[8192], longname[160];
    int tk, i;
    long n;

    /* no file: default workbench, no rules; Workbench gets the main theme, nothing else */
    gt_screen_rules_parse(0, 0, &r);
    CHECK_INT(r.def, GT_SCR_WORKBENCH);
    CHECK_INT(r.n, 0);
    CHECK_INT(scr_theme(&r, "Workbench", "gruvbox", th, &tk), 1);
    CHECK_STR(th, "gruvbox");
    CHECK_INT(scr_theme(&r, "DOPUS.1", "gruvbox", th, &tk), 0);

    /* the three Default values on an unmatched name */
    gt_screen_rules_parse("default all\n", 12, &r);
    CHECK_INT(r.def, GT_SCR_ALL);
    CHECK_INT(scr_theme(&r, "MUIPrefs", "gruvbox", th, &tk), 1);
    CHECK_STR(th, "gruvbox");
    gt_screen_rules_parse("default none\n", 13, &r);
    CHECK_INT(r.def, GT_SCR_NONE);
    CHECK_INT(scr_theme(&r, "MUIPrefs", "gruvbox", th, &tk), 0);
    CHECK_INT(scr_theme(&r, "Workbench", "gruvbox", th, &tk), 0);
    gt_screen_rules_parse("default workbench\n", 18, &r);
    CHECK_INT(scr_theme(&r, "workbench", "gruvbox", th, &tk), 1);
    CHECK_INT(scr_theme(&r, "MUIPrefs", "gruvbox", th, &tk), 0);

    /* pattern: DOPUS#? matches DOPUS.1 not MUIPrefs; case-insensitive; first match wins; takeover flag */
    {
        const char *f = "default none\nDOPUS#? = nord takeover\nDOPUS.1 = gruvbox\n#? = catppuccin\n";
        gt_screen_rules_parse(f, (long)strlen(f), &r);
        CHECK_INT(r.n, 3);
        CHECK_INT(scr_theme(&r, "DOPUS.1", "x", th, &tk), 1);
        CHECK_STR(th, "gruvbox"); /* exact-name rules beat pattern rules */
        CHECK_INT(tk, 0);
        CHECK_INT(scr_theme(&r, "dopus.2", "x", th, &tk), 1);
        CHECK_STR(th, "nord");
        CHECK_INT(tk, 1);
        CHECK_INT(scr_theme(&r, "dopus", "x", th, &tk), 1);
        CHECK_INT(scr_theme(&r, "MUIPrefs", "x", th, &tk), 1);
        CHECK_STR(th, "catppuccin");
        CHECK_INT(tk, 0);
        CHECK_INT(gt_screen_match("DOPUS#?", "DOPUS.1"), 1);
        CHECK_INT(gt_screen_match("DOPUS#?", "MUIPrefs"), 0);
        CHECK_INT(gt_screen_match("TE1?", "TE10"), 1);
        CHECK_INT(gt_screen_match("TE1?", "TE1"), 0);
        CHECK_INT(gt_screen_match("#?", ""), 1);
        CHECK_INT(gt_screen_match("Work bench", "work BENCH"), 1);
        {
            /* TE10-MATCH-BACKTRACK: differential test against the old recursive matcher, every pattern 0..5 over a b ? #,
             * every name 0..4 over a b A */
            const char pa[] = "ab?#", na[] = "abA";
            char pat[8], nm[8];
            long pn, nn, pi, ni, diff = 0, pairs = 0;
            int pl, nl, k;
            for (pl = 0; pl <= 5; pl++) {
                pn = 1;
                for (k = 0; k < pl; k++) pn *= 4;
                for (pi = 0; pi < pn; pi++) {
                    long v = pi;
                    for (k = 0; k < pl; k++) { pat[k] = pa[v % 4]; v /= 4; }
                    pat[pl] = 0;
                    for (nl = 0; nl <= 4; nl++) {
                        nn = 1;
                        for (k = 0; k < nl; k++) nn *= 3;
                        for (ni = 0; ni < nn; ni++) {
                            long w = ni;
                            for (k = 0; k < nl; k++) { nm[k] = na[w % 3]; w /= 3; }
                            nm[nl] = 0;
                            pairs++;
                            if (gt_screen_match(pat, nm) != ref_screen_match(pat, nm)) {
                                diff++;
                                if (diff < 5) printf("match differs: pat '%s' name '%s'\n", pat, nm);
                            }
                        }
                    }
                }
            }
            CHECK_INT(diff, 0);
            CHECK_INT(pairs, 1365L * 121L);
        }
        {
            /* bound: work <= 4 * (pattern length + 1) * (name length + 1) */
            const char *crafted = "#?a#?a#?a#?a#?a#?ab";
            char name[160], big[160];
            int i;
            unsigned long w;
            for (i = 0; i < 24; i++) name[i] = 'a';
            name[24] = 0;
            gt_screen_match_steps = 0;
            CHECK_INT(gt_screen_match(crafted, name), 0);
            w = gt_screen_match_steps;
            CHECK_INT(w <= 4UL * (strlen(crafted) + 1) * (strlen(name) + 1), 1);
            /* the 139-character worst case runs only once the crafted case proved linear (the old matcher would not finish) */
            if (w <= 4UL * (strlen(crafted) + 1) * (strlen(name) + 1)) {
                for (i = 0; i < 46; i++) { big[3 * i] = '#'; big[3 * i + 1] = '?'; big[3 * i + 2] = 'a'; }
                big[138] = 0;
                for (i = 0; i < 138; i++) name[i] = 'a';
                name[138] = 0;
                gt_screen_match_steps = 0;
                CHECK_INT(gt_screen_match(big, name), 1);
                w = gt_screen_match_steps;
                CHECK_INT(w <= 4UL * (strlen(big) + 1) * (strlen(name) + 1), 1);
                big[138] = 'b';
                big[139] = 0;
                name[138] = 'a';
                name[139] = 0;
                gt_screen_match_steps = 0;
                CHECK_INT(gt_screen_match(big, name), 0);
                w = gt_screen_match_steps;
                CHECK_INT(w <= 4UL * (strlen(big) + 1) * (strlen(name) + 1), 1);
            }
        }
    }

    /* Exclude beats Default All */
    {
        const char *f = "default all\nTE1#? = exclude\n";
        gt_screen_rules_parse(f, (long)strlen(f), &r);
        CHECK_INT(scr_theme(&r, "TE10", "gruvbox", th, &tk), 0);
        CHECK_INT(scr_theme(&r, "Other", "gruvbox", th, &tk), 1);
    }

    /* key off = today's behaviour, whatever the rules say */
    {
        const char *f = "default all\nTE1#? = nord\n";
        gt_screen_rules_parse(f, (long)strlen(f), &r);
        r.enabled = 0;
        CHECK_INT(scr_theme(&r, "Workbench", "gruvbox", th, &tk), 1);
        CHECK_STR(th, "gruvbox");
        CHECK_INT(scr_theme(&r, "TE10", "gruvbox", th, &tk), 0);
        CHECK_INT(scr_theme(&r, "Other", "gruvbox", th, &tk), 0);
    }

    /* an unknown or unusable theme name falls back to GT_DEFAULT */
    {
        const char *f = "default none\nA = bad/name\nB = \n";
        gt_screen_rules_parse(f, (long)strlen(f), &r);
        CHECK_INT(scr_theme(&r, "A", "gruvbox", th, &tk), 1);
        CHECK_STR(th, GT_DEFAULT);
        gt_screen_rules_parse("default all\n", 12, &r);
        CHECK_INT(scr_theme(&r, "X", "", th, &tk), 1);
        CHECK_STR(th, GT_DEFAULT);
    }

    /* a 139-character screen name round-trips */
    for (i = 0; i < 139; i++)
        longname[i] = (char)('a' + i % 26);
    longname[139] = 0;
    memset(&r, 0, sizeof r);
    gt_screen_rules_parse(0, 0, &r);
    CHECK_INT(gt_screen_rules_add(&r, longname, "nord", 1), 1);
    n = gt_screen_rules_write(&r, buf, sizeof buf);
    CHECK(n > 0);
    gt_screen_rules_parse(buf, n, &r2);
    CHECK_INT(r2.n, 1);
    CHECK_STR(r2.rule[0].pat, longname);
    CHECK_STR(r2.rule[0].theme, "nord");
    CHECK_INT(r2.rule[0].takeover, 1);
    CHECK_INT(scr_theme(&r2, longname, "x", th, &tk), 1);

    /* rule order survives write then parse; exact-name rules sort above pattern rules; takeover per rule */
    gt_screen_rules_parse("default all\n", 12, &r);
    gt_screen_rules_add(&r, "A#?", "aa", 0);
    gt_screen_rules_add(&r, "Exact One", "bb", 1);
    gt_screen_rules_add(&r, "B#?", "exclude", 0);
    gt_screen_rules_add(&r, "Exact Two", "exclude", 0);
    gt_screen_rules_add(&r, "C?", "cc", 1);
    n = gt_screen_rules_write(&r, buf, sizeof buf);
    CHECK(strncmp(buf, "default all\n", 12) == 0);
    gt_screen_rules_parse(buf, n, &r2);
    CHECK_INT(r2.n, 5);
    CHECK_STR(r2.rule[0].pat, "Exact One");
    CHECK_STR(r2.rule[1].pat, "Exact Two");
    CHECK_STR(r2.rule[2].pat, "A#?");
    CHECK_STR(r2.rule[3].pat, "B#?");
    CHECK_STR(r2.rule[4].pat, "C?");
    CHECK_INT(r2.rule[0].takeover, 1);
    CHECK_INT(r2.rule[1].exclude, 1);
    CHECK_INT(r2.rule[2].takeover, 0);
    CHECK_INT(r2.rule[4].takeover, 1);
    CHECK_STR(r2.rule[4].theme, "cc");

    /* T1: exact-name rule after a pattern rule in memory still wins */
    gt_screen_rules_parse("default none\n", 13, &r);
    gt_screen_rules_add(&r, "DOPUS#?", "A", 0);
    gt_screen_rules_add(&r, "DOPUS.1", "B", 0);
    CHECK_INT(scr_theme(&r, "DOPUS.1", "x", th, &tk), 1);
    CHECK_STR(th, "B");
    CHECK_INT(scr_theme(&r, "DOPUS.2", "x", th, &tk), 1);
    CHECK_STR(th, "A");
    /* T2: resolve(r) == resolve(parse(write(r))) */
    {
        static const char *names[3] = {"DOPUS.1", "DOPUS.2", "Other"};
        gt_screen_rules lists[2];
        char th2[GT_NAME];
        int tk2, rv, rv2, k, j;
        lists[0] = r;
        gt_screen_rules_parse("default all\n", 12, &lists[1]);
        gt_screen_rules_add(&lists[1], "X#?", "exclude", 0);
        gt_screen_rules_add(&lists[1], "X.1", "nord", 1);
        gt_screen_rules_add(&lists[1], "#?", "cc", 0);
        names[2] = "Other";
        for (k = 0; k < 2; k++) {
            n = gt_screen_rules_write(&lists[k], buf, sizeof buf);
            gt_screen_rules_parse(buf, n, &r2);
            for (j = 0; j < 3; j++) {
                const char *nm = k == 0 ? names[j] : (j == 0 ? "X.1" : j == 1 ? "X.2" : "Other");
                rv = scr_theme(&lists[k], nm, "m", th, &tk);
                rv2 = scr_theme(&r2, nm, "m", th2, &tk2);
                CHECK_INT(rv, rv2);
                CHECK_INT(tk, tk2);
                if (rv && rv2)
                    CHECK_STR(th, th2);
            }
        }
    }
    /* T3-T5: the Workbench screen always uses the main theme (D10-10) */
    gt_screen_rules_parse("default none\n", 13, &r);
    gt_screen_rules_add(&r, "#?", "nord", 0);
    CHECK_INT(scr_theme(&r, "Workbench", "tokyo-night", th, &tk), 1);
    CHECK_STR(th, "tokyo-night");
    CHECK_INT(scr_theme(&r, "Other", "tokyo-night", th, &tk), 1);
    CHECK_STR(th, "nord");
    gt_screen_rules_parse("default all\n", 12, &r);
    gt_screen_rules_add(&r, "#?", "exclude", 0);
    CHECK_INT(scr_theme(&r, "Workbench", "tokyo-night", th, &tk), 0);
    gt_screen_rules_parse("default none\n", 13, &r);
    gt_screen_rules_add(&r, "#?", "nord", 1);
    CHECK_INT(scr_theme(&r, "Workbench", "tokyo-night", th, &tk), 1);
    CHECK_INT(tk, 1);
    CHECK_STR(th, "tokyo-night");

    /* Workbench off: an Exclude on the exact name Workbench is stored and leaves the colours alone; the main theme is kept */
    gt_screen_rules_parse("default workbench\n", 18, &r);
    gt_screen_rules_add(&r, "Workbench", "exclude", 0);
    n = gt_screen_rules_write(&r, buf, sizeof buf);
    CHECK(strstr(buf, "Workbench = exclude") != 0);
    gt_screen_rules_parse(buf, n, &r2);
    CHECK_INT(r2.n, 1);
    CHECK_INT(scr_theme(&r2, "Workbench", "gruvbox", th, &tk), 0);
    CHECK_INT(gt_screen_rules_remove(&r2, "workbench"), 1);
    CHECK_INT(r2.n, 0);
    CHECK_INT(scr_theme(&r2, "Workbench", "gruvbox", th, &tk), 1);
    CHECK_STR(th, "gruvbox");
    /* a theme rule on the exact name Workbench is not stored: the main theme key holds it */
    gt_screen_rules_parse("default none\n", 13, &r);
    gt_screen_rules_add(&r, "Workbench", "nord", 0);
    n = gt_screen_rules_write(&r, buf, sizeof buf);
    CHECK(strstr(buf, "Workbench") == 0);
    gt_screen_rules_parse("default none\nWorkbench = nord\n", 30, &r2);
    CHECK_INT(r2.n, 0);
}

/* TE-10.8: the rows of the Screens page edit the same rule list. Helpers: parse a text, read a rule list back as text, one row. */
static void te_parse(gt_screen_rules *r, const char *f)
{
    gt_screen_rules_parse(f, (long)strlen(f), r);
}

static const char *te_text(const gt_screen_rules *r)
{
    static char buf[4096];
    if (gt_screen_rules_write(r, buf, (long)sizeof buf) < 0)
        strcpy(buf, "?overflow");
    return buf;
}

static gt_screen_row te_row(const gt_screen_rules *r, const char *n)
{
    gt_screen_row rows[GT_SCR_ROWS], z;
    const char *o[1];
    int i, k;
    o[0] = n;
    k = gt_screen_rows(r, o, 1, rows, GT_SCR_ROWS);
    for (i = 0; i < k; i++)
        if (!strcasecmp(rows[i].name, n))
            return rows[i];
    memset(&z, 0, sizeof z);
    strcpy(z.name, "?missing");
    return z;
}

static const char *te_theme(const gt_screen_rules *r, const char *n)
{
    static char t[GT_NAME];
    gt_screen_row w = te_row(r, n);
    strcpy(t, w.theme);
    return t;
}

static int te_on(const gt_screen_rules *r, const char *n)
{
    char th[GT_NAME];
    int tk;
    return gt_screen_theme_of(r, n, "main", th, GT_NAME, &tk);
}

static void test_gthemes_screen_rows_fresh_rules_list_workbench_then_open_screens(void)
{
    gt_screen_rules r;
    gt_screen_row rows[GT_SCR_ROWS];
    const char *o[1];
    int n;
    o[0] = "DOPUS.1";
    gt_screen_rules_parse(0, 0, &r);
    n = gt_screen_rows(&r, o, 1, rows, GT_SCR_ROWS);
    CHECK_INT(n, 2);
    CHECK_STR(rows[0].name, "Workbench");
    CHECK_INT(rows[0].on, 1);
    CHECK_INT(rows[0].open, 1);
    CHECK_STR(rows[0].theme, "");
    CHECK_STR(rows[1].name, "DOPUS.1");
    CHECK_INT(rows[1].on, 0);
    CHECK_INT(rows[1].open, 1);
    CHECK_STR(te_text(&r), "default workbench\n");
}

static void test_gthemes_screen_main_token_carries_the_main_theme_for_a_named_screen(void)
{
    gt_screen_rules r;
    char th[GT_NAME], out[GT_NAME];
    int tk;
    const char *f = "default none\nX = :main\nY = :main takeover\n";
    te_parse(&r, f);
    CHECK_INT(r.n, 2);
    CHECK_STR(te_text(&r), f);
    CHECK_INT(gt_screen_theme_of(&r, "X", "gruvbox", th, GT_NAME, &tk), 1);
    CHECK_STR(th, "gruvbox");
    CHECK_INT(tk, 0);
    CHECK_INT(gt_screen_theme_of(&r, "x", "nord", th, GT_NAME, &tk), 1);
    CHECK_STR(th, "nord");
    CHECK_INT(gt_screen_theme_of(&r, "Y", "nord", th, GT_NAME, &tk), 1);
    CHECK_STR(th, "nord");
    CHECK_INT(tk, 1);
    gt_screen_draw_theme(&r, "X", "gruvbox", out, GT_NAME);
    CHECK_STR(out, "gruvbox");
    gt_screen_draw_theme(&r, "X", "nord", out, GT_NAME);
    CHECK_STR(out, "nord");
}

static void test_gthemes_screen_row_tick_on_off_under_each_default_and_rule(void)
{
    gt_screen_rules r;
    const char *start;
    char keep[512];

    /* default Workbench Only */
    te_parse(&r, "default workbench\n");
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS", 1), 1);
    CHECK_STR(te_text(&r), "default workbench\nDOPUS = :main\n");
    CHECK_INT(te_on(&r, "DOPUS"), 1);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS", 0), 1);
    CHECK_STR(te_text(&r), "default workbench\n");
    CHECK_INT(te_on(&r, "DOPUS"), 0);

    /* default All */
    te_parse(&r, "default all\n");
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS", 0), 1);
    CHECK_STR(te_text(&r), "default all\nDOPUS = exclude\n");
    CHECK_INT(te_on(&r, "DOPUS"), 0);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS", 1), 1);
    CHECK_STR(te_text(&r), "default all\n");
    CHECK_INT(te_on(&r, "DOPUS"), 1);

    /* a pattern Exclude rule: the exact rule wins, off again restores the start */
    start = "default all\nDOPUS#? = exclude\n";
    te_parse(&r, start);
    CHECK_INT(te_on(&r, "DOPUS.1"), 0);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS.1", 1), 1);
    CHECK_STR(te_text(&r), "default all\nDOPUS.1 = :main\nDOPUS#? = exclude\n");
    CHECK_INT(te_on(&r, "DOPUS.1"), 1);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS.1", 0), 1);
    CHECK_STR(te_text(&r), start);

    /* a pattern theme rule: off writes an Exclude, on removes it, the pattern's theme and takeover come back */
    start = "default none\nDOPUS#? = nord takeover\n";
    te_parse(&r, start);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS.1", 0), 1);
    CHECK_STR(te_text(&r), "default none\nDOPUS.1 = exclude\nDOPUS#? = nord takeover\n");
    CHECK_INT(te_on(&r, "DOPUS.1"), 0);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS.1", 1), 1);
    CHECK_STR(te_text(&r), start);
    CHECK_STR(te_theme(&r, "DOPUS.1"), "nord");
    CHECK_INT(te_row(&r, "DOPUS.1").takeover, 1);

    /* Workbench: off is an exact Exclude rule, on removes it; under Default None it cannot be ticked */
    te_parse(&r, "default workbench\n");
    CHECK_INT(gt_screen_row_tick(&r, "Workbench", 0), 1);
    CHECK_STR(te_text(&r), "default workbench\nWorkbench = exclude\n");
    CHECK_INT(te_on(&r, "Workbench"), 0);
    CHECK_INT(gt_screen_row_tick(&r, "Workbench", 1), 1);
    CHECK_STR(te_text(&r), "default workbench\n");
    CHECK_INT(te_on(&r, "Workbench"), 1);
    te_parse(&r, "default none\n");
    strcpy(keep, te_text(&r));
    CHECK_INT(gt_screen_row_tick(&r, "Workbench", 1), 0);
    CHECK_STR(te_text(&r), keep);

    /* names that are no exact screen name */
    CHECK_INT(gt_screen_row_tick(&r, "", 1), 0);
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS#?", 1), 0);
    CHECK_INT(gt_screen_row_tick(&r, "A?B", 1), 0);
    CHECK_INT(gt_screen_row_tick(&r, "0123456789012345678901234567890123456789012345678901234567890123456789"
                                      "0123456789012345678901234567890123456789012345678901234567890123456789", 1), 0);
    CHECK_STR(te_text(&r), keep);
}

static void test_gthemes_screen_row_theme_and_takeover_edit_the_exact_rule(void)
{
    gt_screen_rules r;
    gt_screen_row w;

    te_parse(&r, "default workbench\n");
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", "nord"), 1);
    CHECK_STR(te_text(&r), "default workbench\nDOPUS = nord\n");
    w = te_row(&r, "DOPUS");
    CHECK_INT(w.on, 1);
    CHECK_STR(w.theme, "nord");
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", ""), 1);
    CHECK_STR(te_text(&r), "default workbench\nDOPUS = :main\n"); /* still needed: the default does not theme it */
    CHECK_STR(te_theme(&r, "DOPUS"), "");
    CHECK_INT(te_row(&r, "DOPUS").on, 1);
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", GT_SCR_MAIN), 1);
    CHECK_STR(te_text(&r), "default workbench\nDOPUS = :main\n");

    /* Default All: going back to main leaves no rule that says nothing */
    te_parse(&r, "default all\n");
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", "nord"), 1);
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", ""), 1);
    CHECK_STR(te_text(&r), "default all\n");

    /* an excluded row becomes ticked; the takeover flag of the replaced rule stays */
    te_parse(&r, "default all\nDOPUS = exclude\nGONE = gruvbox takeover\n");
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", "nord"), 1);
    CHECK_INT(te_row(&r, "DOPUS").on, 1);
    CHECK_STR(te_theme(&r, "DOPUS"), "nord");
    CHECK_INT(gt_screen_row_theme(&r, "GONE", "dracula"), 1);
    CHECK_STR(te_theme(&r, "GONE"), "dracula");
    CHECK_INT(te_row(&r, "GONE").takeover, 1);

    /* refused: Workbench, a bad theme name */
    CHECK_INT(gt_screen_row_theme(&r, "Workbench", "nord"), 0);
    CHECK_INT(gt_screen_row_theme(&r, "workbench", ""), 0);
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", "a:b"), 0);
    CHECK_INT(gt_screen_row_theme(&r, "DOPUS", "a/b"), 0);
    CHECK_STR(te_theme(&r, "DOPUS"), "nord");

    /* takeover on a default-themed screen adds ":main takeover"; off again removes the rule */
    te_parse(&r, "default all\n");
    CHECK_INT(gt_screen_row_takeover(&r, "DOPUS", 1), 1);
    CHECK_STR(te_text(&r), "default all\nDOPUS = :main takeover\n");
    CHECK_INT(te_row(&r, "DOPUS").takeover, 1);
    CHECK_INT(gt_screen_row_takeover(&r, "DOPUS", 0), 1);
    CHECK_STR(te_text(&r), "default all\n");
    CHECK_INT(te_row(&r, "DOPUS").takeover, 0);

    /* takeover on a screen a pattern themes keeps the pattern's theme */
    te_parse(&r, "default none\nDOPUS#? = nord\n");
    CHECK_INT(gt_screen_row_takeover(&r, "DOPUS.1", 1), 1);
    CHECK_STR(te_theme(&r, "DOPUS.1"), "nord");
    CHECK_INT(te_row(&r, "DOPUS.1").takeover, 1);
    CHECK_INT(gt_screen_row_takeover(&r, "DOPUS.1", 0), 1);
    CHECK_STR(te_theme(&r, "DOPUS.1"), "nord");
    CHECK_INT(te_row(&r, "DOPUS.1").takeover, 0);

    /* refused: not themed, Workbench */
    te_parse(&r, "default workbench\nOFF = exclude\n");
    CHECK_INT(gt_screen_row_takeover(&r, "DOPUS", 1), 0);
    CHECK_INT(gt_screen_row_takeover(&r, "OFF", 1), 0);
    CHECK_INT(gt_screen_row_takeover(&r, "Workbench", 1), 0);
    CHECK_STR(te_text(&r), "default workbench\nOFF = exclude\n");
}

static void test_gthemes_screen_row_exact_rule_beats_a_pattern_rule_that_stands_first(void)
{
    gt_screen_rules r;
    te_parse(&r, "default none\n#? = exclude\n");
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS", 1), 1);
    CHECK_STR(r.rule[0].pat, "#?");
    CHECK_STR(r.rule[1].pat, "DOPUS");
    CHECK_INT(te_on(&r, "DOPUS"), 1);
    CHECK_INT(te_on(&r, "OTHER"), 0);
    te_parse(&r, "default none\n#? = nord\n");
    CHECK_INT(gt_screen_row_tick(&r, "DOPUS", 0), 1);
    CHECK_INT(te_on(&r, "DOPUS"), 0);
    CHECK_INT(te_on(&r, "OTHER"), 1);
}

static void test_gthemes_screen_rows_all_none_invert_flip_the_listed_screens_only(void)
{
    gt_screen_rules r;
    const char *start = "default workbench\nA = :main\nB = exclude\nOLD = nord\n";
    const char *o[3];
    const char *dup[2];
    o[0] = "A";
    o[1] = "B";
    o[2] = "C";
    dup[0] = "A";
    dup[1] = "a";

    te_parse(&r, start);
    CHECK_INT(gt_screen_rows_all(&r, o, 3), 0);
    CHECK_INT(te_on(&r, "Workbench") && te_on(&r, "A") && te_on(&r, "B") && te_on(&r, "C"), 1);
    CHECK_INT(r.def, GT_SCR_WORKBENCH);
    CHECK_STR(te_theme(&r, "OLD"), "nord");

    te_parse(&r, start);
    CHECK_INT(gt_screen_rows_none(&r, o, 3), 0);
    CHECK_INT(te_on(&r, "Workbench") || te_on(&r, "A") || te_on(&r, "B") || te_on(&r, "C"), 0);
    CHECK_INT(r.def, GT_SCR_WORKBENCH);
    CHECK_INT(te_on(&r, "OLD"), 1);
    CHECK_STR(te_theme(&r, "OLD"), "nord");

    te_parse(&r, start);
    CHECK_INT(gt_screen_rows_invert(&r, o, 3), 0);
    CHECK_INT(te_on(&r, "Workbench"), 0);
    CHECK_INT(te_on(&r, "A"), 0);
    CHECK_INT(te_on(&r, "B"), 1);
    CHECK_INT(te_on(&r, "C"), 1);
    CHECK_INT(r.def, GT_SCR_WORKBENCH);
    CHECK_STR(te_theme(&r, "OLD"), "nord");
    CHECK_INT(gt_screen_rows_invert(&r, o, 3), 0);
    CHECK_INT(te_on(&r, "Workbench"), 1);
    CHECK_INT(te_on(&r, "A"), 1);
    CHECK_INT(te_on(&r, "B"), 0);
    CHECK_INT(te_on(&r, "C"), 0);

    /* a screen listed twice flips once */
    te_parse(&r, start);
    CHECK_INT(gt_screen_rows_invert(&r, dup, 2), 0);
    CHECK_INT(te_on(&r, "A"), 0);
}

static void test_gthemes_screen_rows_list_rules_of_closed_screens_after_the_open_ones(void)
{
    gt_screen_rules r;
    gt_screen_row rows[GT_SCR_ROWS];
    const char *o[3];
    int n;
    o[0] = "A";
    o[1] = "B";
    o[2] = "workbench";
    te_parse(&r, "default workbench\nA = :main\nOLD = nord\nGONE = exclude\n#? = gruvbox\nB = exclude\n");
    n = gt_screen_rows(&r, o, 3, rows, GT_SCR_ROWS);
    CHECK_INT(n, 5);
    CHECK_STR(rows[0].name, "Workbench");
    CHECK_STR(rows[1].name, "A");
    CHECK_INT(rows[1].on, 1);
    CHECK_STR(rows[2].name, "B");
    CHECK_INT(rows[2].on, 0);
    CHECK_STR(rows[3].name, "OLD");
    CHECK_INT(rows[3].open, 0);
    CHECK_INT(rows[3].on, 1);
    CHECK_STR(rows[3].theme, "nord");
    CHECK_STR(rows[4].name, "GONE");
    CHECK_INT(rows[4].open, 0);
    CHECK_INT(rows[4].on, 0);
    CHECK_INT(gt_screen_rows(&r, o, 3, rows, 4), 4);
    CHECK_INT(gt_screen_rows(&r, o, 3, rows, 1), 1);
}

static void test_gthemes_screen_rows_a_full_rule_table_refuses_and_changes_nothing(void)
{
    gt_screen_rules r;
    char f[2048], keep[2048];
    const char *o[2];
    int i;
    o[0] = "NEW1";
    o[1] = "NEW2";
    strcpy(f, "default none\n");
    for (i = 0; i < GT_SCR_MAX; i++)
        snprintf(f + strlen(f), 32, "P%d = nord\n", i);
    te_parse(&r, f);
    CHECK_INT(r.n, GT_SCR_MAX);
    strcpy(keep, te_text(&r));
    CHECK_INT(gt_screen_row_tick(&r, "NEW1", 1), 0);
    CHECK_STR(te_text(&r), keep);
    CHECK_INT(gt_screen_row_theme(&r, "NEW1", "nord"), 0);
    CHECK_INT(gt_screen_row_takeover(&r, "NEW1", 1), 0);
    CHECK_INT(gt_screen_rows_all(&r, o, 2), 3);
    CHECK_STR(te_text(&r), keep);

    strcpy(f, "default all\n");
    for (i = 0; i < GT_SCR_MAX; i++)
        snprintf(f + strlen(f), 32, "P%d = nord\n", i);
    te_parse(&r, f);
    strcpy(keep, te_text(&r));
    CHECK_INT(gt_screen_row_tick(&r, "NEW1", 0), 0);
    CHECK_STR(te_text(&r), keep);
    CHECK_INT(gt_screen_row_tick(&r, "P3", 0), 1); /* removes its rule, adds the exclude: fits */
    CHECK_INT(te_on(&r, "P3"), 0);
}

static void test_gthemes_screen_rows_survive_write_and_parse(void)
{
    gt_screen_rules r, r2;
    gt_screen_row a[GT_SCR_ROWS], b[GT_SCR_ROWS];
    const char *o[4];
    int i, na, nb;
    o[0] = "A";
    o[1] = "B";
    o[2] = "C";
    o[3] = "D";
    gt_screen_rules_parse(0, 0, &r);
    CHECK_INT(gt_screen_row_tick(&r, "A", 1), 1);
    CHECK_INT(gt_screen_row_theme(&r, "B", "nord"), 1);
    CHECK_INT(gt_screen_row_takeover(&r, "A", 1), 1);
    CHECK_INT(gt_screen_row_tick(&r, "Workbench", 0), 1);
    CHECK_INT(gt_screen_row_tick(&r, "C", 1), 1);
    CHECK_INT(gt_screen_row_tick(&r, "C", 0), 1);
    CHECK_INT(gt_screen_row_theme(&r, "D", "gruvbox"), 1);
    te_parse(&r2, te_text(&r));
    na = gt_screen_rows(&r, o, 4, a, GT_SCR_ROWS);
    nb = gt_screen_rows(&r2, o, 4, b, GT_SCR_ROWS);
    CHECK_INT(na, 5);
    CHECK_INT(nb, na);
    for (i = 0; i < na && i < nb; i++) {
        CHECK_STR(b[i].name, a[i].name);
        CHECK_INT(b[i].on, a[i].on);
        CHECK_INT(b[i].open, a[i].open);
        CHECK_STR(b[i].theme, a[i].theme);
        CHECK_INT(b[i].takeover, a[i].takeover);
    }
    CHECK_INT(a[0].on, 0);
    CHECK_INT(a[1].on && a[1].takeover, 1);
    CHECK_STR(a[2].theme, "nord");
    CHECK_INT(a[3].on, 0);
    CHECK_STR(a[4].theme, "gruvbox");
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

/* The drawn menu on a true-colour screen with no free pens (Directory Opus 800x600 24-bit, rig 7 2026-10-28) got the
 * nearest palette greys for every role: fills and text must take the theme's own colours there. */
static void test_gthemes_ink_direct(void)
{
    uc_u32 rgb[MT_ROLES], grey[MT_ROLES], out[MT_ROLES];
    gtheme t;
    int r;
    load_shipped("everforest", &t);
    menu_theme_roles_gui(&t, rgb);
    for (r = 0; r < MT_ROLES; r++)
        grey[r] = 0x999999;
    menu_theme_ink(out, rgb, grey, 1, 1);
    for (r = 0; r < MT_ROLES; r++)
        CHECK_INT(out[r], rgb[r]);
    CHECK_INT(out[MT_HEAD_ROLE], t.accent);
    menu_theme_ink(out, rgb, grey, 0, 1);
    for (r = 0; r < MT_ROLES; r++)
        CHECK_INT(out[r], grey[r]);
    menu_theme_ink(out, rgb, grey, 1, 0);
    for (r = 0; r < MT_ROLES; r++)
        CHECK_INT(out[r], grey[r]);
}

/* R1.5: the desktop keys parse, and a missing key is derived (OD5) */
static void test_gthemes_desk_keys(void)
{
    static const char full[] = "background = \"#101010\"\nforeground = \"#e0e0e0\"\ndark_background = \"#0c0c0c\"\n"
                               "darker_background = \"#080808\"\nlighter_background = \"#202020\"\n"
                               "selection = \"#303030\"\nmuted = \"#404040\"\nbackdrop = \"Forest\"\n"
                               "font = \"helvetica.font\"\nwbtitle = \"%n free\"\n";
    static const char bare[] = "background = \"#100000\"\nforeground = \"#f0f0f0\"\ndark_foreground = \"#808080\"\n";
    gtheme t;
    int i;
    gt_builtin(&t);
    gt_parse(full, (long)sizeof full - 1, &t);
    CHECK_INT(t.dark_bg, 0x0c0c0c);
    CHECK_INT(t.darker_bg, 0x080808);
    CHECK_INT(t.lighter_bg, 0x202020);
    CHECK_INT(t.selection, 0x303030);
    CHECK_INT(t.muted, 0x404040);
    CHECK_STR(t.backdrop, "Forest");
    CHECK_STR(t.font, "helvetica.font");
    CHECK_STR(t.wbtitle, "%n free");
    /* derived: darker = bg at 80 %, lighter = a tenth toward fg, selection = lighter, muted = dim */
    gt_parse(bare, (long)sizeof bare - 1, &t);
    CHECK_INT(t.darker_bg, 0x0c0000);
    CHECK_INT(t.dark_bg, 0x100000);
    CHECK_INT(t.lighter_bg, 0x261818);
    CHECK_INT(t.selection, t.lighter_bg);
    CHECK_INT(t.muted, 0x808080);
    CHECK_STR(t.backdrop, "");
    CHECK_STR(t.font, "");
    load_shipped("nord", &t);
    CHECK_INT(t.selection, 0x434c5e);
    CHECK_INT(t.muted, 0x4c566a);
    CHECK_INT(t.darker_bg, 0x191c23);
    /* every shipped theme carries the keys; none is left at zero by the parser */
    for (i = 0; i < (int)(sizeof shipped / sizeof shipped[0]); i++) {
        load_shipped(shipped[i].name, &t);
        CHECK(t.darker_bg != t.lighter_bg);
        CHECK(t.selection != 0 || t.bg == 0);
        CHECK(t.muted != 0 || t.dim == 0);
    }
}

/* ---- the Style > Theme preview on a palette screen: every shipped theme, one after the other ------------------
 * A model of a 16-entry colour map with 4 entries taken (the Workbench pens) and menu_text's pen cache over it. The
 * list previews a theme per cursor move: each time the cache is told the theme changed (mt_theme_changed ->
 * pc_theme_changed) and the rows draw: normal = theme fg on bg, selected = bg on the selection bar (accent). */
#define PM_N 16
static unsigned long pm_col[PM_N];
static int pm_used[PM_N];

static long pm_dist(unsigned long a, unsigned long b)
{
    long dr = (long)((a >> 16) & 255) - (long)((b >> 16) & 255), dg = (long)((a >> 8) & 255) - (long)((b >> 8) & 255),
         db = (long)(a & 255) - (long)(b & 255);
    return dr * dr + dg * dg + db * db;
}

static long pm_best(void *u, unsigned long rgb, int exact)
{
    int i, k = -1;
    (void)u;
    for (i = 0; i < PM_N; i++)
        if (pm_used[i] && pm_col[i] == rgb)
            return i;
    if (exact) {
        for (i = 4; i < PM_N; i++)
            if (!pm_used[i]) {
                pm_used[i] = 1;
                pm_col[i] = rgb;
                return i;
            }
        return -1;
    }
    for (i = 0; i < PM_N; i++)
        if (pm_used[i] && (k < 0 || pm_dist(pm_col[i], rgb) < pm_dist(pm_col[k], rgb)))
            k = i;
    return k;
}

static void pm_release(void *u, long pen)
{
    (void)u;
    pm_used[pen] = 0;
}

static void test_gthemes_picker_pens(void)
{
    static pen_cache pc;
    int i, shown = 0;
    memset(pm_used, 0, sizeof pm_used);
    memset(&pc, 0, sizeof pc);
    pm_used[0] = pm_used[1] = pm_used[2] = pm_used[3] = 1;   /* black, white, blue, grey of the Workbench */
    pm_col[0] = 0x555555UL; pm_col[1] = 0x000000UL; pm_col[2] = 0xFFFFFFUL; pm_col[3] = 0x6688BBUL;
    for (i = 0; i < NSHIPPED; i++) {
        gtheme t;
        uc_u32 rgb[MT_ROLES];
        long fg, bg, sel;
        int pass;
        load_shipped(shipped[i].name, &t);
        menu_theme_roles_gui(&t, rgb);
        pc_theme_changed(&pc, pm_release, 0);
        /* the draw asks for each colour once per row, in the order of the rows, twice over: a repaint */
        for (pass = 0; pass < 2; pass++) {
            fg = pc_of(&pc, pm_best, 0, rgb[MT_TEXT]);
            bg = pc_of(&pc, pm_best, 0, rgb[MT_BG]);
            sel = pc_of(&pc, pm_best, 0, rgb[MT_THEME_ACCENT]);
            CHECK(fg >= 0 && bg >= 0 && sel >= 0);   /* a colour with no pen drew as pen 1 on pen 0 */
            if (fg < 0 || bg < 0 || sel < 0)
                break;
            /* the pens are the theme's own colours, so the rows read as the theme says */
            CHECK_INT(pm_col[fg], rgb[MT_TEXT]);
            CHECK_INT(pm_col[bg], rgb[MT_BG]);
            CHECK_INT(pm_col[sel], rgb[MT_THEME_ACCENT]);
            /* normal row: text over the window ground; selected row: the label (MT_SELTEXT = bg) over the bar */
            CHECK(gt_contrast(pm_col[fg], pm_col[bg]) >= GT_FLOOR_SEL);
            CHECK(gt_contrast(pm_col[bg], pm_col[sel]) >= GT_FLOOR_SEL);
        }
        shown++;
    }
    CHECK_INT(shown, NSHIPPED);
}

/* RL8 FLAT-COLOUR-OVERHAUL step 1: THE role table. Six UI roles, one derivation for every theme (gt_roles_of), pinned on four
 * shipped themes: dark (tokyo-night), black ground (vantablack: backdrop lifts to GT_BLACK_LIFT, white fg capped to bebebe), light
 * (catppuccin-latte, flexoki-light: the selection is pushed darker off the surface). The desktop pens (DK_*), the drawn menu's roles
 * (MT_*) and the flat glyph pens are views of the table, never their own picks. */
static void test_gthemes_inactive_key(void)
{
    gtheme t;
    gt_roles r;
    load_shipped("tokyo-night", &t);
    t.inactive = 0x123456UL;
    gt_roles_of(&t, &r);
    CHECK_INT(r.c[GR_INACTIVE], 0x123456UL);                                   /* an explicit `inactive` key wins over the mix */
    {   /* through the parser: the key, and its absence */
        const char *with = "background = \"#000000\"\nforeground = \"#c8c8c8\"\ninactive = \"#445566\"\n";
        const char *none = "background = \"#000000\"\nforeground = \"#c8c8c8\"\n";
        gt_builtin(&t);
        gt_parse(with, (long)strlen(with), &t);
        gt_roles_of(&t, &r);
        CHECK_INT(r.c[GR_INACTIVE], 0x445566UL);
        gt_builtin(&t);
        gt_parse(none, (long)strlen(none), &t);
        gt_roles_of(&t, &r);
        CHECK_INT(r.c[GR_INACTIVE], gt_mix(t.bg, t.fg, GT_INACTIVE_PCT));
    }
    CHECK_INT(gt_mix(0x000000UL, 0xc8c8c8UL, 50), 0x646464UL);
    CHECK_INT(gt_mix(0x102030UL, 0x102030UL, 45), 0x102030UL);
}

static void test_gthemes_role_table(void)
{
    static const struct {
        const char *name;
        uc_u32 c[GR_INACTIVE]; /* BACKGROUND, SURFACE, TEXT, MUTED, ACCENT, SELECTION */
        uc_u32 ok, fail;
    } want[] = {
        { "tokyo-night", { 0x0e0e14UL, 0x1a1b26UL, 0xa9b1d6UL, 0x414868UL, 0x7aa2f7UL, 0x3e4254UL }, 0x9ece6aUL, 0xf7768eUL },
        { "vantablack", { 0x222222UL, 0x000000UL, 0xbebebeUL, 0x7a7a7aUL, 0x8d8d8dUL, 0x1a1a1aUL }, 0xb6b6b6UL, 0xa4a4a4UL },
        { "catppuccin-latte", { 0xd7d8dcUL, 0xeff1f5UL, 0x4c4f69UL, 0xacb0beUL, 0x1e66f5UL, 0x8f9299UL }, 0x40a02bUL, 0xd20f39UL },
        { "flexoki-light", { 0xe5e2d8UL, 0xfffcf0UL, 0x100f0fUL, 0xb7b5acUL, 0x205ea6UL, 0x919089UL }, 0x879a39UL, 0xd14d41UL },
    };
    unsigned i;
    int k;
    for (i = 0; i < sizeof want / sizeof want[0]; i++) {
        gtheme t;
        gt_roles r;
        uc_u32 rgb[MT_ROLES];
        load_shipped(want[i].name, &t);
        gt_roles_of(&t, &r);
        for (k = 0; k < GR_INACTIVE; k++)
            CHECK_INT(r.c[k], want[i].c[k]);
        CHECK_INT(r.c[GR_INACTIVE], gt_mix(t.bg, t.fg, GT_INACTIVE_PCT));      /* S32-INACTIVE-BAR: default = 45 percent from the ground to the text */
        CHECK(r.c[GR_INACTIVE] != r.c[GR_BACKGROUND] || r.c[GR_INACTIVE] != r.c[GR_TEXT]);
        CHECK_INT(r.ok, want[i].ok);
        CHECK_INT(r.fail, want[i].fail);
        /* the desktop pens are a view: every role sits on its pen, the pen holds the role's colour */
        for (k = 0; k < GR_INACTIVE; k++)
            CHECK_INT(desk_role(&t, desk_pen_of_role(k)), r.c[k]);
        CHECK_INT(desk_role(&t, DK_GREEN), r.ok);
        CHECK_INT(desk_role(&t, DK_BACKDROP), r.fail); /* palette.prefs byte-compatible: pen 7 still writes the fail tone */
        /* the drawn menu is a view */
        menu_theme_roles_gui(&t, rgb);
        CHECK_INT(rgb[MT_BG], r.c[GR_SURFACE]);
        CHECK_INT(rgb[MT_TEXT], r.c[GR_TEXT]);
        CHECK_INT(rgb[MT_DIM], r.dim);
        CHECK_INT(rgb[MT_SELTEXT], r.c[GR_SURFACE]);
        CHECK_INT(rgb[MT_THEME_ACCENT], r.c[GR_ACCENT]);
        CHECK_INT(rgb[MT_OK], r.ok);
        CHECK_INT(rgb[MT_FAIL], r.fail);
    }
    /* the role-to-pen map keeps the pen-index == enum contract (0..7) */
    CHECK_INT(desk_pen_of_role(GR_BACKGROUND), DK_DARKER);
    CHECK_INT(desk_pen_of_role(GR_SURFACE), DK_BG);
    CHECK_INT(desk_pen_of_role(GR_TEXT), DK_FG);
    CHECK_INT(desk_pen_of_role(GR_MUTED), DK_MUTED);
    CHECK_INT(desk_pen_of_role(GR_ACCENT), DK_ACCENT);
    CHECK_INT(desk_pen_of_role(GR_SELECTION), DK_SELECTION);
    CHECK_INT(DK_BACKDROP, 7);
    {
        unsigned char tx, ln, bk;
        desk_flat_glyph_pens(&tx, &ln, &bk);
        CHECK_INT(tx, desk_pen_of_role(GR_TEXT));
        CHECK_INT(ln, desk_pen_of_role(GR_ACCENT));
        CHECK_INT(bk, desk_pen_of_role(GR_SURFACE));
    }
}

/* ---- TE-1: theme file v2 and the model ---- */
static gt_model te_m, te_m2;
static char te_buf[UC_MAX_FILE], te_out[UC_MAX_FILE], te_out2[UC_MAX_FILE];

static int roles_equal(const gt_roles *a, const gt_roles *b)
{
    int i;
    for (i = 0; i < GR_ROLES; i++)
        if (a->c[i] != b->c[i])
            return 0;
    return a->dim == b->dim && a->ok == b->ok && a->fail == b->fail;
}

/* TE-1.5 golden: every shipped theme, read as v1, gives byte-equal roles through the model; saved as v2 and read again
 * (by the model reader and by the v1 gt_parse) the roles stay equal, and a second save is identical text */
static void test_gthemes_model_golden(void)
{
    int i;
    CHECK(GTE_COUNT <= UC_MAX_KEYS);
    for (i = 0; i < NSHIPPED; i++) {
        gtheme t, t2;
        gt_roles want, got;
        char path[200];
        long n, len, len2;
        snprintf(path, sizeof path, THEME_DIR "/%s.theme", shipped[i].name);
        n = read_file(path, te_buf, (long)sizeof te_buf);
        CHECK(n > 0);
        gt_builtin(&t);
        gt_parse(te_buf, n, &t);
        gt_roles_of(&t, &want);
        gt_model_parse(te_buf, n, &te_m);
        CHECK_INT(te_m.version, 1);
        gt_roles_from_model(&te_m, &got);
        CHECK(roles_equal(&want, &got));
        gt_model_from_v1(&t, &te_m2);
        gt_roles_from_model(&te_m2, &got);
        CHECK(roles_equal(&want, &got));
        len = gt_model_save_text(&te_m, te_out, (long)sizeof te_out);
        CHECK(len > 0);
        gt_model_parse(te_out, len, &te_m2);
        CHECK_INT(te_m2.version, 2);
        CHECK_INT(te_m2.warn, 0);
        gt_roles_from_model(&te_m2, &got);
        CHECK(roles_equal(&want, &got));
        len2 = gt_model_save_text(&te_m2, te_out2, (long)sizeof te_out2);
        CHECK_INT(len2, len);
        CHECK(len2 == len && !memcmp(te_out, te_out2, (size_t)len));
        gt_builtin(&t2);
        gt_parse(te_out, len, &t2);
        gt_roles_of(&t2, &got);
        CHECK(roles_equal(&want, &got));
    }
}


/* ---- TE-2: the role consumers read element refs where an element exists; the group value stays the fallback ----------------
 * te2_view is every colour the consumers hand out for one theme: the menu roles, the desk roles and pen colours, the icon pen
 * blends and the two flat_state colours. te2_digest folds it to one number so a whole theme compares in one check. */
typedef struct te2_view {
    uc_u32 menu[MT_ROLES];
    uc_u32 role[DK_ROLES];
    uc_u32 pen[2][DK_ROLES];
    uint8_t icon[2][IP_PENS][3];
    unsigned char inbar[3], sel[3];
} te2_view;

static void te2_capture(const gtheme *t, te2_view *v)
{
    flat_state fs;
    int i, flat;
    memset(v, 0, sizeof *v);
    menu_theme_roles_gui(t, v->menu);
    for (i = 0; i < DK_ROLES; i++) {
        v->role[i] = desk_role(t, i);
        v->pen[0][i] = desk_pen_colour(t, i, 0);
        v->pen[1][i] = desk_pen_colour(t, i, 1);
    }
    for (flat = 0; flat < 2; flat++)
        ip_colours(t, ip_ground(t, flat, 0), v->icon[flat]);
    memset(&fs, 0, sizeof fs);
    desk_flat_inbar(t, &fs);
    memcpy(v->inbar, fs.inbar_rgb, 3);
    memcpy(v->sel, fs.sel_rgb, 3);
}

static unsigned long te2_digest(const te2_view *v)
{
    const unsigned char *p = (const unsigned char *)v;
    unsigned long h = 2166136261UL;
    size_t i;
    for (i = 0; i < sizeof *v; i++)
        h = ((h ^ p[i]) * 16777619UL) & 0xFFFFFFFFUL;
    return h;
}

/* captured from main (3e4027bd) before TE-2, in shipped[] order */
static const unsigned long te2_base[] = {
    0xbd82649dUL /* catppuccin-latte */,
    0xd01ab843UL /* catppuccin */,
    0xe4b792c0UL /* ethereal */,
    0x361cbeddUL /* everforest */,
    0x40e65359UL /* flexoki-light */,
    0x21913d56UL /* gruvbox */,
    0x1bd4a003UL /* hackerman */,
    0x1d8ea28eUL /* jade-horse */,
    0x002d1687UL /* kanagawa */,
    0x1d476c3dUL /* last-horizon */,
    0x916023e2UL /* lumon */,
    0xd1c016fbUL /* lupine */,
    0x897675d6UL /* matte-black */,
    0x43d14065UL /* miasma */,
    0xd3f320eaUL /* nord */,
    0x75e0c7f0UL /* retro-82 */,
    0xaaa24214UL /* ristretto */,
    0xe577544eUL /* rose-pine */,
    0x34814e92UL /* solitude */,
    0x07209f4bUL /* starship */,
    0x3b6c1589UL /* tokyo-night */,
    0x8b1d1921UL /* vantablack */,
    0x33fb808eUL /* white */
};

static void test_gthemes_te2_baseline(void)
{
    int i;
    CHECK_INT((int)(sizeof te2_base / sizeof te2_base[0]), NSHIPPED);
    for (i = 0; i < NSHIPPED; i++) {
        gtheme t;
        te2_view v;
        load_shipped(shipped[i].name, &t);
        te2_capture(&t, &v);
        CHECK(i < (int)(sizeof te2_base / sizeof te2_base[0]) && te2_digest(&v) == te2_base[i]);
        /* the same theme with its v1 model attached (no element overrides) gives the same view */
        {
            char path[200];
            long n;
            snprintf(path, sizeof path, THEME_DIR "/%s.theme", shipped[i].name);
            n = read_file(path, te_buf, (long)sizeof te_buf);
            CHECK(gt_model_attach(&t, te_buf, n));
            te2_capture(&t, &v);
            CHECK(i < (int)(sizeof te2_base / sizeof te2_base[0]) && te2_digest(&v) == te2_base[i]);
            gt_free(&t);
        }
    }
}

#define TE2_OVER 0x123456UL

/* give element elem the colour TE2_OVER (normal state) on a nord theme with its model attached; base is the view before */
static void te2_override(gtheme *t, int elem, te2_view *base, te2_view *over)
{
    gt_ref r;
    long n = read_file(THEME_DIR "/nord.theme", te_buf, (long)sizeof te_buf);
    CHECK(gt_model_attach(t, te_buf, n));
    te2_capture(t, base);
    CHECK(gt_ref_parse(t->model, "#123456", &r));
    t->model->elem[elem][GT_ST_NORMAL] = r;
    te2_capture(t, over);
}

/* expect: base patched by the caller must equal over; when icons is set the icon blends must have changed instead */
static void te2_expect(const te2_view *want, const te2_view *over, int icons)
{
    te2_view w = *want;
    if (icons)
        CHECK(memcmp(w.icon, over->icon, sizeof w.icon) != 0);
    memcpy(w.icon, over->icon, sizeof w.icon);
    CHECK(memcmp(&w, over, sizeof w) == 0);
}

/* TE-2.2 / TE-2.3: per consumer, an element override changes exactly the slot it names and nothing else */
static void test_gthemes_te2_overrides(void)
{
    gtheme t;
    te2_view b, o, w;
    int i, f;
    static const struct { int elem, slot; } menu[] = {
        { GTE_MENU_BACKGROUND, MT_BG }, { GTE_MENU_TEXT, MT_TEXT }, { GTE_MENU_SELECTED_TEXT, MT_SELTEXT },
        { GTE_MENU_HEADER, MT_THEME_ACCENT }, { GTE_SUCCESS_TEXT, MT_OK }, { GTE_FAILURE_TEXT, MT_FAIL } };
    for (i = 0; i < (int)(sizeof menu / sizeof menu[0]); i++) {
        gt_builtin(&t);
        te2_override(&t, menu[i].elem, &b, &o);
        CHECK(b.menu[menu[i].slot] != TE2_OVER);
        w = b;
        w.menu[menu[i].slot] = TE2_OVER;
        te2_expect(&w, &o, 0);
        gt_free(&t);
    }
    /* desk pen colours: Desktop Backdrop moves the backdrop pen (and the icon ground that is that pen), Requester Background pen DK_BG,
     * Button Face pen DK_SELECTION and the flat button fill; desk_role, the written-prefs view, never moves */
    gt_builtin(&t);
    te2_override(&t, GTE_DESKTOP_BACKDROP, &b, &o);
    w = b;
    for (f = 0; f < 2; f++)
        w.pen[f][desk_backdrop_pen()] = TE2_OVER;
    te2_expect(&w, &o, 1);
    gt_free(&t);
    te2_override(&t, GTE_REQUESTER_BACKGROUND, &b, &o);
    w = b;
    for (f = 0; f < 2; f++)
        w.pen[f][DK_BG] = TE2_OVER;
    te2_expect(&w, &o, 0);
    gt_free(&t);
    te2_override(&t, GTE_BUTTON_FACE, &b, &o);
    w = b;
    for (f = 0; f < 2; f++)
        w.pen[f][DK_SELECTION] = TE2_OVER;
    w.sel[0] = 0x12; w.sel[1] = 0x34; w.sel[2] = 0x56;
    te2_expect(&w, &o, 0);
    gt_free(&t);
    /* the icon pens: each element moves the blends and nothing else */
    {
        static const int el[] = { GTE_ICON_FOREGROUND, GTE_ICON_ACCENT, GTE_ICON_MUTED, GTE_ICON_SELECTION_PLATE };
        for (i = 0; i < 4; i++) {
            te2_view o2;
            te2_override(&t, el[i], &b, &o2);
            te2_expect(&b, &o2, 1);
            gt_free(&t);
        }
    }
    /* TE-2.3: Window Title Bar Inactive moves inbar_rgb alone; Scroll Knob has its own reader and moves no view */
    te2_override(&t, GTE_TITLE_BAR_INACTIVE, &b, &o);
    CHECK_INT(desk_inbar_rgb(&t), TE2_OVER);
    w = b;
    w.inbar[0] = 0x12; w.inbar[1] = 0x34; w.inbar[2] = 0x56;
    te2_expect(&w, &o, 0);
    gt_free(&t);
    te2_override(&t, GTE_SCROLL_KNOB, &b, &o);
    CHECK_INT(desk_knob_rgb(&t), TE2_OVER);
    te2_expect(&b, &o, 0);
    gt_free(&t);
    gt_builtin(&t);
    CHECK_INT(desk_knob_rgb(&t), desk_role(&t, DK_MUTED));
}

/* TE-1.2 / TE-1.3: the ref grammar, the nesting limit and the group cycle refusal */
static void test_gthemes_model_refs(void)
{
    gt_ref r;
    gtheme t;
    char txt[160];
    gt_builtin(&t);
    t.bg = 0x102030;
    t.fg = 0xE0E0E0;
    t.accent = 0x336699;
    gt_model_from_v1(&t, &te_m);
    CHECK(gt_ref_parse(&te_m, "mix(accent;#ffffff;50)", &r));
    CHECK_INT(r.kind, GT_REF_MIX);
    CHECK_INT(gt_ref_text(&te_m, &r, txt, (int)sizeof txt) > 0, 1);
    CHECK_STR(txt, "mix(accent;#ffffff;50)");
    CHECK_INT(gt_ref_rgb(&te_m, &r, GT_ST_NORMAL), gt_mix(0x336699, 0xFFFFFF, 50));
    /* four nested functions parse, five are refused */
    CHECK(gt_ref_parse(&te_m, "darker(darker(darker(darker(accent))))", &r));
    CHECK_INT(te_m.warn & GT_WARN_BAD_REF, 0);
    CHECK(!gt_ref_parse(&te_m, "darker(darker(darker(darker(darker(accent)))))", &r));
    CHECK(te_m.warn & GT_WARN_BAD_REF);
    CHECK_INT(r.kind, GT_REF_NONE);
    CHECK(!gt_ref_parse(&te_m, "nosuchswatch", &r));
    CHECK(!gt_ref_parse(&te_m, "#12345", &r));
    CHECK(gt_ref_parse(&te_m, "group:accent", &r));
    CHECK_INT(gt_ref_rgb(&te_m, &r, GT_ST_NORMAL), 0x336699);
}

static void test_gthemes_model_cycle(void)
{
    static const char text[] = "amarchy-theme = 2\nname = \"cyc\"\n[profile swatches]\nbackground = #101010\nforeground = #eeeeee\n"
                               "accent = #336699\n[profile groups]\ntext = group:accent\naccent = group:text\n";
    gt_roles r;
    gt_model_parse(text, (long)sizeof text - 1, &te_m);
    CHECK(te_m.warn & GT_WARN_CYCLE);
    gt_roles_from_model(&te_m, &r);
    CHECK_INT(r.c[GR_TEXT], 0xEEEEEE);
    CHECK_INT(r.c[GR_ACCENT], 0x336699);
}

/* TE-1.1 / TE-1.4: all 36 elements set fits the key cap; unknown keys warn and survive a save; Omarchy extras survive */
static void test_gthemes_model_full_and_extras(void)
{
    static char text[UC_MAX_FILE];
    long n = 0, len;
    int i;
    n += snprintf(text + n, sizeof text - (size_t)n, "amarchy-theme = 2\nname = \"full\"\nmode = \"dark\"\nomarchy_extra = \"keepme\"\n");
    n += snprintf(text + n, sizeof text - (size_t)n, "[profile swatches]\nbackground = #101010\nforeground = #eeeeee\naccent = #336699\n");
    n += snprintf(text + n, sizeof text - (size_t)n, "[profile groups]\nbogus-group = #010203\n[profile elements]\n");
    for (i = 0; i < GTE_COUNT; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, "%s = accent,foreground,#111111,mix(accent;foreground;30)\n", gt_elem_defs[i].key);
    n += snprintf(text + n, sizeof text - (size_t)n, "no-such-element = accent\n[profile look]\ncorner-radius = 4\ntitle-font = \"topaz.font\"\n");
    gt_model_parse(text, n, &te_m);
    CHECK_INT(te_m.warn & GT_WARN_OVERFLOW, 0);
    CHECK(te_m.warn & GT_WARN_UNKNOWN_KEY);
    CHECK_INT(te_m.look.corner_radius, 4);
    CHECK_STR(te_m.look.font[GT_FONT_TITLE], "topaz.font");
    CHECK_INT(gt_resolve_rgb(&te_m, GTE_COUNT - 1, GT_ST_NORMAL), 0x336699);
    CHECK_INT(gt_resolve_rgb(&te_m, 0, GT_ST_PRESSED), 0x111111);
    len = gt_model_save_text(&te_m, te_out, (long)sizeof te_out);
    CHECK(len > 0);
    CHECK(strstr(te_out, "omarchy_extra = \"keepme\"") != 0);
    CHECK(strstr(te_out, "bogus-group = #010203") != 0);
    CHECK(strstr(te_out, "no-such-element = accent") != 0);
    CHECK(strstr(te_out, "corner-radius = 4") != 0);
    gt_model_parse(te_out, len, &te_m2);
    CHECK_INT(te_m2.warn & GT_WARN_OVERFLOW, 0);
    CHECK_INT(gt_resolve_rgb(&te_m2, 0, GT_ST_PRESSED), 0x111111);
}

/* TE-1.6: the model hangs off a gtheme only through attach, and gt_free releases it */
static void test_gthemes_model_attach(void)
{
    gtheme t;
    long n = read_file(THEME_DIR "/nord.theme", te_buf, (long)sizeof te_buf);
    gt_builtin(&t);
    CHECK(t.model == 0);
    CHECK(gt_model_attach(&t, te_buf, n));
    CHECK(t.model != 0);
    gt_free(&t);
    CHECK(t.model == 0);
}

/* TE-2: gt_parse itself leaves no model, whatever the gtheme held before (a stack gtheme starts with garbage); a caller that
 * reads gt_roles_of / gt_elem_rgb after gt_parse must never follow a stale pointer */
static void test_gthemes_parse_clears_model(void)
{
    gtheme t;
    gt_roles r;
    const char *s = "background = #101820\nforeground = #e0e8f0\naccent = #ff8800\n";
    memset(&t, 0xA5, sizeof t);
    gt_parse(s, (long)strlen(s), &t);
    CHECK(t.model == 0);
    CHECK(gt_elem_rgb(&t, 0, 0x123456UL) == 0x123456UL);
    gt_roles_of(&t, &r);
    CHECK(r.c[GR_ACCENT] == 0xff8800UL);
}

/* ---- TE-3: Save, Save As, Duplicate, Import, Export, Delete on a memory file system ---- */
#define TM_MAX 16
static char tm_theme[UC_MAX_FILE];
typedef struct { char path[160]; char *data; long len; } tm_file;
typedef struct { tm_file f[TM_MAX]; int n; } tm_fs;

static tm_file *tm_find(tm_fs *m, const char *path)
{
    int i;
    for (i = 0; i < m->n; i++)
        if (!strcmp(m->f[i].path, path))
            return &m->f[i];
    return 0;
}
static int tm_write(void *c, const char *path, const char *buf, long len)
{
    tm_fs *m = (tm_fs *)c;
    tm_file *f = tm_find(m, path);
    if (!f) {
        if (m->n >= TM_MAX)
            return 0;
        f = &m->f[m->n++];
        snprintf(f->path, sizeof f->path, "%s", path);
        f->data = 0;
    }
    free(f->data);
    f->data = (char *)malloc((size_t)len + 1);
    memcpy(f->data, buf, (size_t)len);
    f->data[len] = 0;
    f->len = len;
    return 1;
}
static int tm_exists(void *c, const char *path) { return tm_find((tm_fs *)c, path) != 0; }
static int tm_remove(void *c, const char *path)
{
    tm_fs *m = (tm_fs *)c;
    tm_file *f = tm_find(m, path);
    if (f) {
        free(f->data);
        *f = m->f[--m->n];
    }
    return 1;
}
static int tm_rename(void *c, const char *a, const char *b) { (void)c; (void)a; (void)b; return 0; }
static long tm_read(void *c, const char *path, char *buf, long cap)
{
    tm_file *f = tm_find((tm_fs *)c, path);
    if (!f || f->len > cap)
        return -1;
    memcpy(buf, f->data, (size_t)f->len);
    return f->len;
}
static void tm_init(tm_fs *m, prefs_fs *fs)
{
    memset(m, 0, sizeof *m);
    if (!tm_theme[0])
        read_file(THEME_DIR "/nord.theme", tm_theme, (long)sizeof tm_theme - 1);
    fs->ctx = m;
    fs->write_file = tm_write;
    fs->exists = tm_exists;
    fs->remove = tm_remove;
    fs->rename = tm_rename;
}
static void tm_free(tm_fs *m)
{
    while (m->n)
        tm_remove(m, m->f[0].path);
}
static int tm_applied;
static int tm_apply(const char *name) { (void)name; tm_applied++; return 1; }

static void test_gthemes_te3_save(void)
{
    tm_fs m;
    prefs_fs fs;
    gt_model mod;
    char why[96], out[4096];
    tm_file *u, *e;
    long n;
    tm_init(&m, &fs);
    gt_model_parse(tm_theme, (long)strlen(tm_theme), &mod);
    strcpy(mod.name, "Mine");                     /* gt_save writes the target name: the round trip is of the model named Mine */
    n = gt_model_save_text(&mod, out, (long)sizeof out);
    CHECK(gt_save(&fs, "Mine", &mod, why, sizeof why));
    u = tm_find(&m, "ENVARC:Amarchy/themes/Mine.theme");
    e = tm_find(&m, "ENV:Amarchy/themes/Mine.theme");
    CHECK(u && e);
    CHECK(u && u->len == n && !memcmp(u->data, out, (size_t)n));     /* save text equals the round trip */
    CHECK(e && e->len == n && !memcmp(e->data, out, (size_t)n));
    CHECK(!gt_save(&fs, "bad/name", &mod, why, sizeof why));        /* name refusal */
    CHECK(strstr(why, "name") != 0);
    CHECK(!gt_save(&fs, "", &mod, why, sizeof why));
    /* D2: a shipped theme's name is never saved over, not even as a user copy */
    tm_write(&m, "UP-Term:Amarchy/Themes/Nord.theme", tm_theme, (long)strlen(tm_theme));
    CHECK(!gt_save(&fs, "Nord", &mod, why, sizeof why));
    CHECK(tm_find(&m, "ENVARC:Amarchy/themes/Nord.theme") == 0);
    CHECK(!gt_save_as(&fs, "Mine", &mod, why, sizeof why));         /* taken */
    CHECK(!gt_save_as(&fs, "Nord", &mod, why, sizeof why));
    CHECK(gt_save_as(&fs, "Nord Custom", &mod, why, sizeof why));
    tm_free(&m);
}

static int tm_taken(void *c, const char *name)
{
    char p[160];
    snprintf(p, sizeof p, "ENVARC:Amarchy/themes/%s.theme", name);
    return tm_exists(c, p);
}
static void test_gthemes_te3_custom_name(void)
{
    tm_fs m;
    prefs_fs fs;
    char nm[GT_NAME];
    tm_init(&m, &fs);
    gt_custom_name("Nord", tm_taken, &m, nm, sizeof nm);
    CHECK_STR(nm, "Nord Custom");
    tm_write(&m, "ENVARC:Amarchy/themes/Nord Custom.theme", "x", 1);
    gt_custom_name("Nord", tm_taken, &m, nm, sizeof nm);
    CHECK_STR(nm, "Nord Custom 2");
    tm_free(&m);
}

static void test_gthemes_te3_edit_applies(void)
{
    tm_fs m;
    prefs_fs fs;
    gt_model mod;
    char why[96];
    unsigned long before;
    tm_init(&m, &fs);
    gt_model_parse(tm_theme, (long)strlen(tm_theme), &mod);
    tm_applied = 0;
    before = gt_dos_edit_applies;
    CHECK(gt_save_edit(&fs, "Mine", &mod, "Other", tm_apply, why, sizeof why));
    CHECK_INT((int)(gt_dos_edit_applies - before), 0);               /* not the current one: no re-apply */
    CHECK_INT(tm_applied, 0);
    CHECK(gt_save_edit(&fs, "Mine", &mod, "mine", tm_apply, why, sizeof why));
    CHECK_INT((int)(gt_dos_edit_applies - before), 1);               /* current, file changed: applied (sentinel) */
    CHECK_INT(tm_applied, 1);
    CHECK(!gt_save_edit(&fs, "bad/n", &mod, "bad/n", tm_apply, why, sizeof why));
    CHECK_INT(tm_applied, 1);                                        /* a refused save applies nothing */
    tm_free(&m);
}


/* TE-4: the editor rows. Find a row by its label, step and set it through lf_setting_*, and read the edit buffer. */
static const sd_setting *te4_row(const char *label)
{
    int i;
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->label && !strcmp(s->label, label))
            return s;
    }
    return 0;
}

static void te4_open_builtin(void)
{
    gtheme t;
    long n = read_file(THEME_DIR "/nord.theme", te_buf, (long)sizeof te_buf);
    memset(&t, 0, sizeof t);
    CHECK(gt_model_attach(&t, te_buf, n));
    gte_open(t.model, "nord");
    gt_free(&t);
}

static void test_gthemes_te4_section4(void)
{
    const sd_setting *radius, *swatch0 = 0, *remove, *ghost;
    amconf_io io;
    int failed = 0, before, n, i, used = -1;
    char why[160], buf[96];
    unsigned long e0;

    memset(&io, 0, sizeof io);
    te4_open_builtin();
    /* every theme page has rows */
    for (n = PG_PAGE_THEME; n < PG_NPAGES_ALL; n++) {
        int rows = 0;
        for (i = 0; i < gte_nrows(); i++)
            if (gte_row(SDI_COUNT + i)->page == n)
                rows++;
        CHECK(rows > 0);
    }
    CHECK_INT(PG_NPAGES, 7);
    radius = te4_row("Corner Radius");
    ghost = te4_row("Ghost Style");
    CHECK(radius && ghost);
    e0 = gte_edits;
    CHECK_INT(lf_setting_set(&io, ghost, 2, &failed), 0);         /* SD_CYCLE */
    CHECK(gte_edits > e0);
    CHECK_INT(lf_setting_set_text(&io, radius, "7", &failed), 0); /* SD_INT */
    CHECK_INT(lf_setting_set_text(&io, radius, "999", &failed), LF_SET_INVALID);
    CHECK(gte_row_label(radius, buf, sizeof buf) > 0 && strstr(buf, "7"));
    CHECK(gte_dirty());
    CHECK_INT(lf_setting_step(&io, ghost, &failed), 0);
    /* SD_SWATCH: the first swatch row takes a colour */
    for (i = 0; i < gte_nrows(); i++)
        if (gte_row(SDI_COUNT + i)->kind == SD_SWATCH) {
            swatch0 = gte_row(SDI_COUNT + i);
            break;
        }
    CHECK(swatch0 != 0);
    CHECK_INT(lf_setting_set_text(&io, swatch0, "#102030", &failed), 0);
    CHECK_INT(lf_setting_set_text(&io, swatch0, "nonsense", &failed), LF_SET_INVALID);
    /* Remove Swatch is refused while a group or element uses it, and the answer names the users */
    before = gte_swatch_count();
    for (i = 0; i < before; i++)
        if (gte_swatch_users(i, why, sizeof why) > 0) {
            used = i;
            break;
        }
    CHECK(used >= 0);
    CHECK_INT(gte_swatch_remove(used, why, sizeof why), GTR_IN_USE);
    CHECK(why[0] != 0);
    CHECK_INT(gte_swatch_count(), before);
    /* a new swatch nobody uses can go */
    CHECK_INT(gte_swatch_add("Spare", 0x123456UL), GTR_OK);
    CHECK_INT(gte_swatch_remove(gte_swatch_count() - 1, why, sizeof why), GTR_OK);
    CHECK_INT(gte_swatch_count(), before);
    remove = te4_row("Remove Swatch...");
    CHECK(remove && remove->kind == SD_GACTION);
    /* revert reloads the file's model: the radius edit is gone */
    gte_revert();
    CHECK(!gte_dirty());
    CHECK(gte_row_label(radius, buf, sizeof buf) > 0 && !strstr(buf, ": 7"));
}

/* TE-4.2: swatch, swatch-reference and element rows open the colour picker (palette entry or Custom) and step/set through the
 * lf_setting_* handlers, writing the edit buffer. TE-4 item 2: Rename/Remove Swatch look the swatch up by name. */
/* TE-4.5: every row of plan section 4 exists on its page with its full English label, and the reused rows (Desktop switches, Icon
 * Label Style, Terminal Palette) write the store of the row they copy; Icon Label Colour is an element row visible in Simple mode. */
static void test_gthemes_te4_rows(void)
{
    static const struct { const char *label; int page; } want[] = {
        { "Mode", PG_PAGE_THEME }, { "Base Theme", PG_PAGE_THEME }, { "Save", PG_PAGE_THEME }, { "Save As...", PG_PAGE_THEME },
        { "Revert", PG_PAGE_THEME }, { "Apply to Desktop", PG_PAGE_THEME }, { "Contrast", PG_PAGE_THEME },
        { "Pen Budget", PG_PAGE_THEME }, { "Add Swatch...", PG_PAGE_THEME_SWATCHES },
        { "Rename Swatch...", PG_PAGE_THEME_SWATCHES }, { "Remove Swatch...", PG_PAGE_THEME_SWATCHES },
        { "Terminal Palette", PG_PAGE_THEME_SWATCHES }, { "Screen Font", PG_PAGE_THEME_TYPE },
        { "Menu Font", PG_PAGE_THEME_TYPE }, { "Icon Font", PG_PAGE_THEME_TYPE },
        { "Shell Font", PG_PAGE_THEME_TYPE }, { "Title Alignment", PG_PAGE_THEME_TYPE }, { "Title Style", PG_PAGE_THEME_TYPE },
        { "Corner Radius", PG_PAGE_THEME_SHAPE }, { "Border Width", PG_PAGE_THEME_SHAPE }, { "Title Bar Height", PG_PAGE_THEME_SHAPE },
        { "Gadget Size", PG_PAGE_THEME_SHAPE }, { "Gadget Padding", PG_PAGE_THEME_SHAPE }, { "Scroll Bar Width", PG_PAGE_THEME_SHAPE },
        { "Knob Minimum Length", PG_PAGE_THEME_SHAPE }, { "Knob Shape", PG_PAGE_THEME_SHAPE },
        { "Gadget Glyph Set", PG_PAGE_THEME_SHAPE }, { "Gadget Order", PG_PAGE_THEME_SHAPE },
        { "Ghost Style", PG_PAGE_THEME_STATES }, { "Hover Highlight", PG_PAGE_THEME_STATES },
        { "Inactive Window Dimming", PG_PAGE_THEME_STATES }, { "Theme the Desktop", PG_PAGE_THEME_DESKTOP },
        { "Desktop Palette", PG_PAGE_THEME_DESKTOP }, { "Desktop Backdrop", PG_PAGE_THEME_DESKTOP },
        { "Desktop Pointer", PG_PAGE_THEME_DESKTOP }, { "Desktop Icon Labels", PG_PAGE_THEME_DESKTOP },
        { "Pointer Colour 1", PG_PAGE_THEME_DESKTOP }, { "Pointer Colour 3", PG_PAGE_THEME_DESKTOP },
        { "Icon Label Style", PG_PAGE_THEME_DESKTOP }, { "Icon Label Colour", PG_PAGE_THEME_DESKTOP },
        { "Duplicate Theme...", PG_PAGE_THEME_FILES }, { "Import Theme...", PG_PAGE_THEME_FILES },
        { "Export Theme...", PG_PAGE_THEME_FILES }, { "Reset to Base", PG_PAGE_THEME_FILES },
        { "Delete User Theme", PG_PAGE_THEME_FILES }
    };
    memfs mfs;
    prefs_fs pfs;
    amconf_io io;
    am_prefs am;
    const sd_setting *r;
    int i, failed = 0;
    mf_init(&mfs, &pfs);
    io.fs = &pfs;
    io.read = mf_read;
    io.dir = mf_dir;
    te4_open_builtin();
    for (i = 0; i < (int)(sizeof want / sizeof want[0]); i++) {
        r = te4_row(want[i].label);
        CHECK(r != 0);
        if (r)
            CHECK_INT(r->page, want[i].page);
    }
    r = te4_row("Icon Label Colour");
    CHECK(r && r->kind == SD_ELEMENT && gte_row_visible(r));          /* a shortcut: shown in Simple mode too */
    r = te4_row("Terminal Palette");
    CHECK(r && r->kind == SD_PALETTE && r->store == SD_UPTERM);
    r = te4_row("Desktop Backdrop");
    CHECK(r && r->kind == SD_CYCLE && r->store == SD_AMARCHY && r->nvalues == 2);
    if (r) {
        CHECK_INT(lf_setting_set(&io, r, 0, &failed), LF_SET_OK);       /* writes the real desktop-backdrop key */
        CHECK(amconf_load_with(&io, &am) != AMCONF_DEFAULTS);
        CHECK_STR(am.val[AM_DESKBACK], "off");
    }
    r = te4_row("Icon Label Style");
    CHECK(r && r->store == SD_AMARCHY && r->nvalues == 3);
    if (r) {
        CHECK_INT(lf_setting_set(&io, r, 1, &failed), LF_SET_OK);
        CHECK(amconf_load_with(&io, &am) != AMCONF_DEFAULTS);
        CHECK_STR(am.val[AM_ICONTEXT], "outline");
    }
}

/* TE-7-TITLE-FONT: font.prefs has no title-font chunk, so the Theme Type page offers no Window Title Font row; every other font
 * row still edits its own slot, and a theme file carrying title-font loads and keeps the value on save (no format change). */
static void test_gthemes_te7_no_title_font_row(void)
{
    static const struct { const char *label; int slot; } rows[] = {
        { "Screen Font", GT_FONT_SCREEN }, { "Menu Font", GT_FONT_MENU }, { "Icon Font", GT_FONT_ICON }, { "Shell Font", GT_FONT_SHELL }
    };
    char text[256];
    long n, len;
    int i;
    te4_open_builtin();
    CHECK(te4_row("Window Title Font") == 0);
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        const sd_setting *r = te4_row(rows[i].label);
        CHECK(r && r->kind == SD_FONT);
        if (r)
            CHECK_INT(r->aux, rows[i].slot + 1);
    }
    n = snprintf(text, sizeof text, "amarchy-theme = 2\nname = \"tf\"\n[profile look]\ntitle-font = \"topaz.font\"\n");
    gt_model_parse(text, n, &te_m);
    CHECK_STR(te_m.look.font[GT_FONT_TITLE], "topaz.font");
    len = gt_model_save_text(&te_m, te_out, (long)sizeof te_out);
    CHECK(len > 0);
    gt_model_parse(te_out, len, &te_m2);
    CHECK_STR(te_m2.look.font[GT_FONT_TITLE], "topaz.font");
}

/* TE-4.3: the decision to ask when the menu leaves the editor with unsaved changes, and how the answer reads. */
static void test_gthemes_te4_leave(void)
{
    te4_open_builtin();
    CHECK(!gte_dirty());
    CHECK_INT(lf_leave_needed("settings.theme", ""), 0);          /* clean buffer: never asks */
    gte_edits = 0;
    gt_ed.m.base[0] = 'X';                                         /* any edit makes the buffer differ from the file */
    CHECK(gte_dirty());
    CHECK_INT(lf_leave_needed("settings.theme", "settings"), 1);   /* up out of the editor */
    CHECK_INT(lf_leave_needed("settings.theme", ""), 1);           /* the menu closes */
    CHECK_INT(lf_leave_needed("settings.theme.swatches", "settings.theme"), 0);   /* still inside */
    CHECK_INT(lf_leave_needed("settings.themex", "settings"), 0);  /* a sibling id is not the editor */
    CHECK_INT(lf_leave_needed("settings", ""), 0);
    CHECK_INT(lf_leave_decide("Save"), LF_LEAVE_SAVE);
    CHECK_INT(lf_leave_decide(" d"), LF_LEAVE_DISCARD);
    CHECK_INT(lf_leave_decide("Cancel"), LF_LEAVE_CANCEL);
    CHECK_INT(lf_leave_decide(""), LF_LEAVE_CANCEL);
    CHECK_INT(lf_leave_decide("x"), LF_LEAVE_CANCEL);
    CHECK_INT(lf_leave_decide(0), LF_LEAVE_CANCEL);
    /* TE-4.3: Launch and window close end the menu from the node it is on: dirty inside the editor asks, anywhere else never */
    CHECK_INT(lf_exit_needs_ask("settings.theme"), 1);
    CHECK_INT(lf_exit_needs_ask("settings.theme.swatches"), 1);
    CHECK_INT(lf_exit_needs_ask("settings"), 0);
    CHECK_INT(lf_exit_needs_ask(""), 0);
    gte_revert();                                                  /* Discard is this call: the prompt no longer fires */
    CHECK_INT(lf_leave_needed("settings.theme", ""), 0);
    CHECK_INT(lf_exit_needs_ask("settings.theme"), 0);             /* clean: close and launch go straight through */
}

static void test_gthemes_te4_dispatch(void)
{
    const sd_setting *sw = 0, *el = 0, *pal = 0;
    rd_ctx rc;
    int i;
    rc.calls = 0;
    /* theme_action: what each action does before it touches a file */
    CHECK_INT(gte_action_plan(GTA_OPEN, 0), GTP_LEAVE | GTP_NAME);
    CHECK_INT(gte_action_plan(GTA_RESET_BASE, 1), GTP_LEAVE);
    CHECK_INT(gte_action_plan(GTA_SAVE, 0), GTP_SKIP);
    CHECK_INT(gte_action_plan(GTA_SAVE, 1), 0);
    CHECK_INT(gte_action_plan(GTA_APPLY, 0), GTP_SKIP);
    CHECK_INT(gte_action_plan(GTA_SAVE_AS, 0), GTP_SKIP);
    CHECK_INT(gte_action_plan(GTA_DUPLICATE, 1), GTP_NAME);
    CHECK_INT(gte_action_plan(GTA_ADD_SWATCH, 0), GTP_NAME);
    CHECK_INT(gte_action_plan(GTA_IMPORT, 0), GTP_NAME);
    CHECK_INT(gte_action_plan(GTA_REVERT, 0), 0);
    CHECK_INT(gte_action_plan(GTA_CLEAR_ELEMENT, 1), 0);
    /* gt_dos_edit_open: a theme with a file opens in the buffer; an unknown one leaves the buffer alone */
    te4_open_builtin();
    strcpy(gt_ed.name, "before");
    CHECK_INT(gte_open_from(fake_read, &rc, "nowhere"), 0);
    CHECK(!strcmp(gt_ed.name, "before"));
    CHECK_INT(gte_open_from(fake_read, &rc, "solo"), 1);
    CHECK(!strcmp(gt_ed.name, "solo"));
    CHECK(gt_ed.open && !gte_dirty());
    CHECK(rc.calls > 0);
    /* lf_setting_enter, picker branch: which choices ask a value */
    te4_open_builtin();
    gt_ed.advanced = 1;
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *r = gte_row(SDI_COUNT + i);
        if (!sw && r->kind == SD_SWATCH)
            sw = r;
        if (!el && r->kind == SD_ELEMENT && gte_row_visible(r))
            el = r;
    }
    pal = te4_row("Terminal Palette");
    CHECK(sw && el && pal);
    CHECK(lf_picker_prompt(sw, 1) == 0);
    CHECK(!strcmp(lf_picker_prompt(sw, SD_PICK_CUSTOM), "RRGGBB"));
    CHECK(!strcmp(lf_picker_prompt(el, SD_PICK_CUSTOM), "colour or reference"));
    CHECK(!strcmp(lf_picker_prompt(pal, 3), "RRGGBB"));
}

static void test_gthemes_te4_pickers(void)
{
    const sd_setting *sw = 0, *ref = 0, *el = 0;
    memfs mfs;
    prefs_fs pfs;
    amconf_io io;
    mn_item it[3];
    long chip[3];
    int failed = 0, i, n, before;
    char buf[96], hex[16], why[160];
    mf_init(&mfs, &pfs);
    io.fs = &pfs;
    io.read = mf_read;
    io.dir = mf_dir;
    mf_put(&mfs, "ENVARC:up-term/up-term", "");
    CHECK(mf_get(&mfs, "ENVARC:up-term/up-term") != 0);
    te4_open_builtin();
    gte_edits = 0;
    CHECK_INT(lf_setting_set(&io, gte_row(SDI_COUNT), 1, &failed), 0);   /* the Mode row: Advanced shows Elements */
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (!sw && s->kind == SD_SWATCH)
            sw = s;
        if (!ref && s->kind == SD_SWATCHREF)
            ref = s;
        if (!el && s->kind == SD_ELEMENT && gte_row_visible(s))
            el = s;
    }
    CHECK(sw && ref && el);
    CHECK(SD_HAS_PICKER(sw) && SD_HAS_PICKER(ref) && SD_HAS_PICKER(el));
    /* swatch: palette entry 1 is a literal colour; Custom takes hex, refuses a bad one */
    lf_palette_hex(&io, 1, hex);
    CHECK_INT(lf_setting_set_colour(&io, sw, 1, 0, &failed), LF_SET_OK);
    gte_row_text(sw, buf, sizeof buf);
    CHECK(buf[0] == '#' && strlen(buf) == 7 && strncasecmp(buf + 1, hex, 6) == 0);
    CHECK_INT(lf_setting_set_colour(&io, sw, SD_PICK_CUSTOM, "0a0b0c", &failed), LF_SET_OK);
    gte_row_text(sw, buf, sizeof buf);
    CHECK_STR(buf, "#0a0b0c");
    lf_colour_current(&io, sw, 0, hex);
    CHECK_STR(hex, "0a0b0c");
    CHECK_INT(lf_setting_set_colour(&io, sw, SD_PICK_CUSTOM, "nothex", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_set_colour(&io, sw, 17, 0, &failed), LF_SET_KIND);
    /* reference rows: a palette entry becomes a literal, Custom takes any reference */
    CHECK_INT(lf_setting_set_colour(&io, ref, 2, 0, &failed), LF_SET_OK);
    gte_row_text(ref, buf, sizeof buf);
    CHECK(buf[0] == '#');
    CHECK_INT(lf_setting_set_colour(&io, ref, SD_PICK_CUSTOM, "no-such-swatch(", &failed), LF_SET_INVALID);
    CHECK_INT(lf_setting_step(&io, ref, &failed), LF_SET_OK);      /* next swatch name */
    gte_row_text(ref, buf, sizeof buf);
    CHECK(gte_swatch_find(buf) >= 0 || strchr(buf, '('));
    CHECK_INT(lf_setting_set_colour(&io, el, 3, 0, &failed), LF_SET_OK);
    gte_row_text(el, buf, sizeof buf);
    CHECK(buf[0] == '#');
    CHECK_INT(lf_setting_step(&io, el, &failed), LF_SET_OK);
    CHECK_INT(lf_setting_set_colour(&io, el, SD_PICK_CUSTOM, "", &failed), LF_SET_OK);   /* empty: follows its group */
    CHECK(!gte_elem_overridden(el->aux));
    CHECK(gte_edits > 0);
    /* the picker level's rows are labelled and chipped like the existing colour rows */
    memset(it, 0, sizeof it);
    it[0].row = MN_ROW_CHOICE; it[0].ix = sw->id; strcpy(it[0].seg, "5");
    it[1].row = MN_ROW_CHOICE; it[1].ix = ref->id; strcpy(it[1].seg, "16");
    it[2].row = MN_ROW_SETTING; it[2].ix = sw->id;
    lf_setting_labels(&io, it, 3);
    CHECK(strchr(it[0].label, '#') != 0);
    CHECK_STR(it[1].label, "Custom...");
    lf_setting_chips(&io, it, 3, chip);
    CHECK(chip[0] >= 0 && chip[1] == -1 && chip[2] == 0x0a0b0cL);
    /* rename by name; an unknown name is refused and changes nothing */
    gte_swatch_add("Spare", 0x123456UL);
    before = gte_swatch_count();
    CHECK(gte_swatch_find("spare") == before - 1);                  /* case blind */
    CHECK_INT(gte_swatch_rename_name("Spare", "Renamed"), GTR_OK);
    CHECK_INT(gte_swatch_find("Spare"), -1);
    CHECK_INT(gte_swatch_find("Renamed"), before - 1);
    CHECK_INT(gte_swatch_rename_name("Nobody", "Other"), GTR_BAD);
    CHECK_INT(gte_swatch_find("Other"), -1);
    CHECK_INT(gte_swatch_remove_name("Nobody", why, sizeof why), GTR_BAD);
    CHECK_INT(gte_swatch_count(), before);
    CHECK_INT(gte_swatch_remove_name("Renamed", why, sizeof why), GTR_OK);
    n = gte_swatch_count();
    CHECK_INT(n, before - 1);
}

static void test_gthemes_te4_modes(void)
{
    const sd_setting *mode, *el = 0;
    amconf_io io;
    int failed = 0, i, shown = 0, seen = 0, over = -1, g, marked = 0;
    char buf[96];

    memset(&io, 0, sizeof io);
    te4_open_builtin();
    mode = te4_row("Mode");
    CHECK(mode != 0);
    /* Simple: no Elements row is visible */
    CHECK_INT(lf_setting_set(&io, mode, 0, &failed), 0);
    CHECK(!gt_ed.advanced);
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->page == PG_PAGE_THEME_ELEMENTS && gte_row_visible(s))
            shown++;
    }
    CHECK_INT(shown, 0);
    /* Advanced: element rows show, an override takes, and the Groups page marks its group */
    CHECK_INT(lf_setting_set(&io, mode, 1, &failed), 0);
    CHECK(gt_ed.advanced);
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->page == PG_PAGE_THEME_ELEMENTS && s->kind == SD_ELEMENT && gte_row_visible(s)) {
            seen++;
            if (!el)
                el = s;
        }
    }
    CHECK(seen > 0 && el != 0);
    over = el->aux;
    CHECK_INT(lf_setting_set_text(&io, el, "#ff00ff", &failed), 0);
    CHECK(gte_elem_overridden(over));
    for (g = 0; g < 10; g++)
        if (gte_group_overridden(g))
            for (i = 0; i < gte_nrows(); i++) {
                const sd_setting *s = gte_row(SDI_COUNT + i);
                if (s->kind == SD_SWATCHREF && s->aux == g && s->aux2 == 0) {
                    gte_row_label(s, buf, sizeof buf);
                    marked += strstr(buf, "(elements override)") != 0;
                }
            }
    CHECK_INT(marked, 1);
    /* back to Simple: the override survives and the Elements page hides again */
    CHECK_INT(lf_setting_set(&io, mode, 0, &failed), 0);
    CHECK(gte_elem_overridden(over));
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->page == PG_PAGE_THEME_ELEMENTS && gte_row_visible(s))
            shown++;
    }
    CHECK_INT(shown, 0);
}

/* ---- TE-8: presets, contrast warning, pen budget. Reused: tm_fs/tm_init (memory theme files), te4_open_builtin/te4_row (editor rows),
 * read_file + THEME_DIR (shipped themes as fixtures); searched gthemes_core.h and this file for existing contrast/budget helpers: none. */
static void te8_load(gt_model *m, const char *name)
{
    char *buf = (char *)malloc(UC_MAX_FILE);
    long n;
    char path[96];
    snprintf(path, sizeof path, THEME_DIR "/%s.theme", name);
    n = read_file(path, buf, UC_MAX_FILE - 1);
    CHECK(n > 0);
    gt_model_parse(buf, n, m);
    free(buf);
}
/* every element in every state resolves to the same colour in both models */
static int te8_same(const gt_model *a, const gt_model *b)
{
    int e, s, same = 1;
    for (e = 0; e < GTE_COUNT; e++)
        for (s = 0; s <= GT_ST_DISABLED; s++)
            if (gt_resolve_rgb(a, e, s) != gt_resolve_rgb(b, e, s))
                same = 0;
    return same;
}
static void te8_put(tm_fs *fs, const char *dir, const char *name, const char *text)
{
    char p[160];
    snprintf(p, sizeof p, "%s/%s.theme", dir, name);
    CHECK(tm_write(fs, p, text, (long)strlen(text)));
}
static char te8_nord[UC_MAX_FILE], te8_gruv[UC_MAX_FILE];
static void te8_texts(void)
{
    if (!te8_nord[0]) {
        read_file(THEME_DIR "/nord.theme", te8_nord, (long)sizeof te8_nord - 1);
        read_file(THEME_DIR "/gruvbox.theme", te8_gruv, (long)sizeof te8_gruv - 1);
    }
}

static void test_gthemes_nearest_role_pen_ties_low_and_caps_at_depth(void)
{
    gtheme t;
    uint8_t rgb[IP_PENS][3];
    int pen, slot, depth;
    uc_u32 c;
    gt_builtin(&t);
    for (pen = 0; pen <= DK_DARKER; pen++)                     /* a colour equal to a role pen maps to a pen of that exact colour */
        CHECK(desk_role(&t, gt_nearest_role_pen(&t, desk_role(&t, pen), 3)) == desk_role(&t, pen));
    for (depth = 1; depth <= 3; depth++)                       /* depth 2 never returns a pen above DK_ACCENT */
        for (pen = 0; pen < DK_ROLES; pen++)
            CHECK(depth > 2 || gt_nearest_role_pen(&t, desk_role(&t, pen), depth) <= DK_ACCENT);
    for (c = 0; c < 0x1000000UL; c += 0x10F1UL) {              /* the first pen at the minimum distance wins: a tie goes to the lower pen */
        int want = 0, last = DK_DARKER;
        unsigned long bd = 0;
        for (pen = 0; pen <= last; pen++) {
            uc_u32 r = desk_role(&t, pen);
            long dr = (long)((c >> 16) & 255) - (long)((r >> 16) & 255), dg = (long)((c >> 8) & 255) - (long)((r >> 8) & 255),
                 db = (long)(c & 255) - (long)(r & 255);
            unsigned long d = (unsigned long)(dr * dr + dg * dg + db * db);
            if (pen == 0 || d < bd) {
                bd = d;
                want = pen;
            }
        }
        CHECK_INT(gt_nearest_role_pen(&t, c, 3), want);
    }
    gt_builtin(&t);                                            /* ip_fold is this same rule over the icon slot colours */
    ip_colours(&t, desk_role(&t, DK_BG), rgb);
    for (depth = 2; depth <= 3; depth++)
        for (slot = 0; slot < IP_PENS; slot++)
            CHECK_INT(ip_fold(&t, slot, depth), gt_nearest_role_pen(&t,
                ((uc_u32)rgb[slot][0] << 16) | ((uc_u32)rgb[slot][1] << 8) | rgb[slot][2], depth));
}

static int plan_excl_outside(const gt_plan_pen *p, int n, const uc_u32 *mask)
{
    int i, bad = 0;
    for (i = 0; i < n; i++)
        if (p[i].exclusive && !((mask[p[i].pen >> 5] >> (p[i].pen & 31)) & 1))
            bad++;
    return bad;
}

static int plan_excl_dups(const gt_plan_pen *p, int n)
{
    int i, j, d = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < i; j++)
            if (p[i].exclusive && p[j].exclusive && p[i].pen == p[j].pen)
                d++;
    return d;
}

static int plan_excl_count(const gt_plan_pen *p, int n)
{
    int i, c = 0;
    for (i = 0; i < n; i++)
        c += p[i].exclusive;
    return c;
}

static void test_gthemes_te105_plan(void)
{
    static const char *names[] = { "nord", "tokyo-night", "gruvbox" };
    static const int depths[] = { 3, 4, 8, 24 };
    gt_model m;
    gt_budget b;
    gt_plan_pen p[GTE_COUNT + 4], q[GTE_COUNT + 4], tiny[2];
    uc_u32 none[GT_MASK_WORDS], sparse[GT_MASK_WORDS], full[GT_MASK_WORDS];
    const uc_u32 *masks[3];
    int ni, di, mi, n, n2, i, guard_ok = 1, any_excl = 0;
    memset(none, 0, sizeof none);
    memset(full, 0xFF, sizeof full);
    memset(sparse, 0, sizeof sparse);
    sparse[0] = 0x00F0F000UL; sparse[1] = 0x0000FFFFUL; sparse[5] = 0xAAAAAAAAUL;
    masks[0] = none; masks[1] = sparse; masks[2] = full;
    for (ni = 0; ni < 3; ni++) {
        te8_load(&m, names[ni]);
        for (di = 0; di < 4; di++) {
            gt_pen_budget(&m, depths[di], &b);
            for (mi = 0; mi < 3; mi++) {
                n = gt_screen_apply_plan(&m, depths[di], masks[mi], 0, p, GTE_COUNT + 4);
                CHECK_INT(n, b.exact ? 0 : b.needing);               /* the count is the budget's */
                CHECK_INT(plan_excl_outside(p, n, masks[mi]), 0);    /* never a colour write on an owned pen */
                CHECK_INT(plan_excl_dups(p, n), 0);
                n2 = gt_screen_apply_plan(&m, depths[di], masks[mi], 0, q, GTE_COUNT + 4);
                CHECK_INT(n2, n);
                CHECK_INT(n == 0 || memcmp(p, q, (size_t)n * sizeof p[0]) == 0, 1); /* deterministic */
                n = gt_screen_apply_plan(&m, depths[di], masks[mi], 1, p, GTE_COUNT + 4);
                CHECK_INT(n, b.exact ? 0 : b.needing);
                CHECK_INT(plan_excl_dups(p, n), 0);
            }
        }
    }
    /* take over with an empty mask on a deep screen: the owner's pens are taken */
    te8_load(&m, "nord");
    gt_pen_budget(&m, 8, &b);
    n = gt_screen_apply_plan(&m, 8, none, 1, p, GTE_COUNT + 4);
    CHECK_INT(n > 0 && plan_excl_count(p, n) == n, 1);
    for (i = 0; i < n; i++)
        if (p[i].pen < b.held || p[i].pen >= b.pens)
            guard_ok = 0;
    CHECK_INT(guard_ok, 1);
    n = gt_screen_apply_plan(&m, 8, none, 0, p, GTE_COUNT + 4);
    CHECK_INT(plan_excl_count(p, n), 0);                                 /* all best */
    n = gt_screen_apply_plan(&m, 8, full, 0, p, GTE_COUNT + 4);
    any_excl = plan_excl_count(p, n);
    CHECK_INT(any_excl, n);                                              /* a free screen: every colour gets its own pen */
    /* capacity: cut off, never past the end */
    tiny[0].pen = tiny[1].pen = -7;
    memset(p, 0x5A, sizeof p);
    n = gt_screen_apply_plan(&m, 8, full, 0, p, 1);
    CHECK_INT(n, 1);
    CHECK_INT(p[1].pen, 0x5A5A);                                         /* untouched past cap */
    n = gt_screen_apply_plan(&m, 8, full, 0, tiny, 0);
    CHECK_INT(n, 0);
    CHECK_INT(tiny[0].pen, -7);
}

static void test_gthemes_te8_budget(void)
{
    gt_model m, g;
    gt_budget b;
    char t[160];
    int grp;
    te8_load(&m, "nord");
    gt_pen_budget(&m, 3, &b);
    CHECK_INT(b.pens, 8); CHECK_INT(b.held, 8); CHECK_INT(b.free, 0);
    CHECK_INT(b.exact, 0);
    CHECK_INT(b.colours, 10); CHECK_INT(b.shared, 26); CHECK_INT(b.needing, 4); CHECK_INT(b.fold, 4);
    gt_budget_text(&b, t, sizeof t);
    CHECK_STR(t, "Screen 8 colours: 4 of 10 colours fold to the nearest of 8 pens");
    for (grp = 0; grp < GTG_COUNT; grp++) {
        int has = gt_budget_share_text(&b, grp, t, sizeof t);
        CHECK_INT(has, grp >= 6 && grp <= 9);                       /* only the groups sharing a role pen get a line */
        if (!has)
            CHECK_STR(t, "");
    }
    gt_budget_share_text(&b, 6, t, sizeof t); CHECK_STR(t, "Shares a pen with Selection");
    gt_budget_share_text(&b, 7, t, sizeof t); CHECK_STR(t, "Shares a pen with Accent");
    gt_budget_share_text(&b, 8, t, sizeof t); CHECK_STR(t, "Shares a pen with Accent");
    gt_budget_share_text(&b, 9, t, sizeof t); CHECK_STR(t, "Shares a pen with Selection");
    gt_pen_budget(&m, 4, &b);
    CHECK_INT(b.pens, 16); CHECK_INT(b.held, 16); CHECK_INT(b.free, 0);
    CHECK_INT(b.colours, 10); CHECK_INT(b.shared, 26); CHECK_INT(b.needing, 4); CHECK_INT(b.fold, 4);
    gt_pen_budget(&m, 8, &b);
    CHECK_INT(b.pens, 256); CHECK_INT(b.held, 27); CHECK_INT(b.free, 229);
    CHECK_INT(b.fold, 0);
    CHECK_INT(gt_budget_share_text(&b, 7, t, sizeof t), 0);          /* a deep screen has no share lines */
    gt_pen_budget(&m, 24, &b);
    CHECK_INT(b.exact, 1); CHECK_INT(b.fold, 0);
    gt_budget_text(&b, t, sizeof t);
    CHECK(strstr(t, "Truecolour") != 0);
    /* second fixed model: jade-horse, whose colours repeat more than nord's (9 distinct, 3 needing a pen: the pushed inactive title text is one of them) */
    te8_load(&g, "jade-horse");
    gt_pen_budget(&g, 3, &b);
    gt_pen_budget(&g, 3, &b);
    CHECK_INT(b.pens, 8); CHECK_INT(b.free, 0);
    CHECK_INT(b.colours, 9); CHECK_INT(b.shared, 27); CHECK_INT(b.needing, 3); CHECK_INT(b.fold, 3);
    gt_budget_text(&b, t, sizeof t);
    CHECK_STR(t, "Screen 8 colours: 3 of 9 colours fold to the nearest of 8 pens");
    for (grp = 0; grp < GTG_COUNT; grp++)
        CHECK_INT(gt_budget_share_text(&b, grp, t, sizeof t), grp >= 6 && grp <= 9);
    gt_budget_share_text(&b, 6, t, sizeof t); CHECK_STR(t, "Shares a pen with Muted Text");
    gt_budget_share_text(&b, 7, t, sizeof t); CHECK_STR(t, "Shares a pen with Muted Text");
    gt_budget_share_text(&b, 8, t, sizeof t); CHECK_STR(t, "Shares a pen with Accent");
    gt_budget_share_text(&b, 9, t, sizeof t); CHECK_STR(t, "Shares a pen with Accent");
    gt_pen_budget(&g, 8, &b);
    CHECK_INT(b.held, 27); CHECK_INT(b.free, 229);
    CHECK_INT(b.colours, 9); CHECK_INT(b.shared, 27); CHECK_INT(b.needing, 3); CHECK_INT(b.fold, 0);
}

/* nord as shipped measures 1 warning (Inactive title text 142 under 220: the runtime pushes it off the surface, the raw colours are
 * what the editor measures). te8_clean sets that pair to black/white so a test starts from a model with none. */
static void te8_clean(gt_model *m)
{
    gt_ref w, k;
    CHECK(gt_ref_parse(m, "#ffffff", &w));
    CHECK(gt_ref_parse(m, "#000000", &k));
    m->elem[GTE_TITLE_TEXT_INACTIVE][GT_ST_NORMAL] = w;
    m->elem[GTE_TITLE_BAR_INACTIVE][GT_ST_NORMAL] = k;
}

/* TE-8-STOCK-CONTRAST P8: every stock theme gives the button the requester background, so that pair never warns */
static void test_gthemes_te8_button_face_equals_requester_gets_no_warning(void)
{
    gt_model m;
    gt_cpair p[GT_CPAIRS];
    gt_ref c;
    int np, i, first = 0;
    te8_load(&m, "nord");
    te8_clean(&m);
    CHECK(gt_ref_parse(&m, "#303030", &c));
    m.elem[GTE_BUTTON_FACE][GT_ST_NORMAL] = c;
    m.elem[GTE_REQUESTER_BACKGROUND][GT_ST_NORMAL] = c;
    CHECK(gt_ref_parse(&m, "#f0f0f0", &c));
    m.elem[GTE_BUTTON_TEXT][GT_ST_NORMAL] = c;
    np = gt_contrast_pairs(&m, p);
    CHECK(np > 0);
    for (i = 0; i < np; i++)
        CHECK(!(p[i].text == GTE_BUTTON_FACE && p[i].ground == GTE_REQUESTER_BACKGROUND));
    CHECK_INT(gt_contrast_warnings(&m, &first), 0);
    m.elem[GTE_BUTTON_TEXT][GT_ST_NORMAL] = m.elem[GTE_BUTTON_FACE][GT_ST_NORMAL];  /* the other pairs still warn */
    CHECK_INT(gt_contrast_warnings(&m, &first), 2);                 /* the text pair and the disabled pair that follows it */
    np = gt_contrast_pairs(&m, p);
    CHECK(first >= 0 && np > first);
    if (first >= 0 && np > first)
        CHECK_INT(p[first].text, GTE_BUTTON_TEXT);
}

/* TE-8-STOCK-CONTRAST P1: the inactive title text is dimmed on purpose, so its floor is GT_FLOOR_DIM (150), and the default value
 * is pushed off the inactive bar until it reads there. */
static void test_gthemes_te8_stock_inactive_title_text_reads_150_on_its_bar(void)
{
    gt_model m;
    gt_cpair p[GT_CPAIRS];
    gt_ref c, bar;
    int i, j, np;
    for (i = 0; i < NSHIPPED; i++) {
        te8_load(&m, shipped[i].name);
        CHECK(gt_contrast(gt_resolve_rgb(&m, GTE_TITLE_TEXT_INACTIVE, GT_ST_NORMAL), gt_resolve_rgb(&m, GTE_TITLE_BAR_INACTIVE, GT_ST_NORMAL)) >= GT_FLOOR_DIM);
        CHECK_INT(gt_contrast_elem_warns(&m, GTE_TITLE_TEXT_INACTIVE), 0);
        np = gt_contrast_pairs(&m, p);
        for (j = 0; j < np; j++)
            if (p[j].text == GTE_TITLE_TEXT_INACTIVE) {
                CHECK_INT(p[j].floor, GT_FLOOR_DIM);
                CHECK_INT(p[j].below, 0);
            }
    }
    /* a value the user sets is kept as set; only the floor (150, not 220) decides the warning */
    te8_load(&m, "nord");
    CHECK(gt_ref_parse(&m, "#404040", &bar));
    m.elem[GTE_TITLE_BAR_INACTIVE][GT_ST_NORMAL] = bar;
    CHECK(gt_ref_parse(&m, "#232323", &c));
    m.elem[GTE_TITLE_TEXT_INACTIVE][GT_ST_NORMAL] = c;
    CHECK_INT((int)gt_resolve_rgb(&m, GTE_TITLE_TEXT_INACTIVE, GT_ST_NORMAL), 0x232323);
    CHECK(gt_contrast(0x232323UL, 0x404040UL) >= GT_FLOOR_DIM && gt_contrast(0x232323UL, 0x404040UL) < GT_FLOOR_TEXT);
    CHECK_INT(gt_contrast_elem_warns(&m, GTE_TITLE_TEXT_INACTIVE), 0);   /* 160: above 150, below 220 */
    CHECK(gt_ref_parse(&m, "#303030", &c));
    m.elem[GTE_TITLE_TEXT_INACTIVE][GT_ST_NORMAL] = c;
    CHECK_INT((int)gt_resolve_rgb(&m, GTE_TITLE_TEXT_INACTIVE, GT_ST_NORMAL), 0x303030);
    CHECK_INT(gt_contrast_elem_warns(&m, GTE_TITLE_TEXT_INACTIVE), 1);
}

static void test_gthemes_te8_contrast(void)
{
    gt_model m;
    gt_cpair p[GT_CPAIRS];
    char buf[160];
    gt_ref lo, bg;
    int first = 0, w, np, i;
    const sd_setting *row;
    te8_load(&m, "nord");
    CHECK_INT(gt_contrast_warnings(&m, &first), 0);                 /* the shipped nord theme as measured: its inactive title text default reads 150 */
    te8_clean(&m);
    CHECK_INT(gt_contrast_warnings(&m, &first), 0);                 /* above every floor: no warning */
    CHECK_INT(first, -1);
    CHECK_INT(gt_contrast_elem_warns(&m, GTE_TITLE_TEXT), 0);
    np = gt_contrast_pairs(&m, p);
    CHECK(np > 0);
    for (i = 0; i < np; i++)
        CHECK_INT(p[i].below, 0);
    CHECK(gt_ref_parse(&m, "#303030", &bg));
    CHECK(gt_ref_parse(&m, "#383838", &lo));
    m.elem[GTE_TITLE_BAR][GT_ST_NORMAL] = bg;                       /* text barely off its ground: under GT_FLOOR_TEXT */
    m.elem[GTE_TITLE_TEXT][GT_ST_NORMAL] = lo;
    w = gt_contrast_warnings(&m, &first);
    CHECK_INT(w, 1);
    gt_contrast_pairs(&m, p);
    CHECK(first >= 0);
    if (first >= 0) {
        CHECK_INT(p[first].text, GTE_TITLE_TEXT);
        CHECK_INT(p[first].ground, GTE_TITLE_BAR);
        CHECK_INT(p[first].below, 1);
        CHECK(p[first].contrast < p[first].floor);
    }
    CHECK_INT(gt_contrast_elem_warns(&m, GTE_TITLE_TEXT), 1);
    CHECK_INT(gt_contrast_elem_warns(&m, GTE_TITLE_TEXT_INACTIVE), 0);
    CHECK_INT(gt_contrast_elem_warns(&m, GTE_MENU_TEXT), 0);
    /* the visible rows: the summary and the Elements row suffix */
    te4_open_builtin();
    gt_ed.m = m;
    gt_ed.advanced = 1;
    row = te4_row("Contrast");
    CHECK(row != 0);
    if (row) {
        gte_row_text(row, buf, sizeof buf);
        CHECK_STR(buf, "1 warning: Title Text on Title Bar too faint");
    }
    {
        int faint = 0, good = 0;
        for (i = 0; i < gte_nrows(); i++) {
            const sd_setting *s = gte_row(SDI_COUNT + i);
            if (s->kind == SD_ELEMENT && s->aux2 == GT_ST_NORMAL && gte_row_visible(s)) {
                gte_row_text(s, buf, sizeof buf);
                if (s->aux == GTE_TITLE_TEXT) { faint++; CHECK(strstr(buf, "(low contrast)") != 0); }
                if (s->aux == GTE_MENU_TEXT) { good++; CHECK(strstr(buf, "(low contrast)") == 0); }
            }
        }
        CHECK_INT(faint, 1);
        CHECK_INT(good, 1);
    }
    /* the same pair above its floor: no warning, no suffix */
    CHECK(gt_ref_parse(&m, "#f0f0f0", &lo));
    m.elem[GTE_TITLE_TEXT][GT_ST_NORMAL] = lo;
    CHECK_INT(gt_contrast_warnings(&m, &first), 0);
    gt_ed.m = m;
    row = te4_row("Contrast");
    if (row) {
        gte_row_text(row, buf, sizeof buf);
        CHECK_STR(buf, "0 warnings");
    }
}

static void test_gthemes_te8_presets(void)
{
    tm_fs fs;
    prefs_fs pfs;
    gt_model got, want;
    gtheme bt;
    char why[96];
    int ok;
    tm_init(&fs, &pfs);
    te8_texts();
    CHECK_INT(gt_preset_model(tm_read, &fs, "Probe", &got), 0);       /* no file, not the built-in name */
    ok = gt_preset_model(tm_read, &fs, GT_DEFAULT, &got);             /* neither file: the built-in */
    CHECK_INT(ok, 1);
    memset(&bt, 0, sizeof bt);
    gt_builtin(&bt);
    gt_model_from_v1(&bt, &want);
    CHECK(te8_same(&got, &want));
    CHECK_STR(got.name, GT_DEFAULT);
    te8_put(&fs, GT_SHIPPED_DIR, GT_DEFAULT, te8_nord);               /* a shipped file wins over the built-in */
    CHECK_INT(gt_preset_model(tm_read, &fs, GT_DEFAULT, &got), 1);
    gt_model_parse(te8_nord, (long)strlen(te8_nord), &want);
    CHECK(te8_same(&got, &want));
    te8_put(&fs, GT_USER_DIR, GT_DEFAULT, te8_gruv);                  /* the user file wins over the shipped one */
    CHECK_INT(gt_preset_model(tm_read, &fs, GT_DEFAULT, &got), 1);
    gt_model_parse(te8_gruv, (long)strlen(te8_gruv), &want);
    CHECK(te8_same(&got, &want));
    /* Duplicate Theme starts from the preset */
    te8_put(&fs, GT_SHIPPED_DIR, "Probe", te8_nord);
    CHECK(gt_duplicate(&pfs, tm_read, &fs, "Probe", "Probe Copy", why, sizeof why));
    CHECK_INT(gt_preset_model(tm_read, &fs, "Probe Copy", &got), 1);
    gt_model_parse(te8_nord, (long)strlen(te8_nord), &want);
    CHECK(te8_same(&got, &want));
    CHECK_STR(got.base, "Probe");
    CHECK(tm_exists(&fs, GT_USER_DIR "/Probe Copy.theme"));
    {   /* the saved text carries the copy's name, not the source's */
        static char txt[65536];
        long tn = tm_read(&fs, GT_USER_DIR "/Probe Copy.theme", txt, (long)sizeof txt);
        CHECK(tn > 0);
        CHECK(tn > 0 && strstr(txt, "name = \"Probe Copy\"") != NULL);
        CHECK(tn > 0 && strstr(txt, "name = \"Probe\"") == NULL);
        /* Save As from a model that carries another theme's name */
        CHECK(gt_save_as(&pfs, "Other Copy", &got, why, sizeof why));
        tn = tm_read(&fs, GT_USER_DIR "/Other Copy.theme", txt, (long)sizeof txt);
        CHECK(tn > 0 && strstr(txt, "name = \"Other Copy\"") != NULL);
        CHECK(tn > 0 && strstr(txt, "name = \"Probe Copy\"") == NULL);
    }
    tm_free(&fs);
}

static void test_gthemes_te8_reset_base(void)
{
    tm_fs fs;
    prefs_fs pfs;
    gt_model base, mine;
    unsigned long e0;
    tm_init(&fs, &pfs);
    te8_texts();
    te8_put(&fs, GT_SHIPPED_DIR, "nord", te8_nord);
    gt_model_parse(te8_nord, (long)strlen(te8_nord), &base);
    gt_model_parse(te8_gruv, (long)strlen(te8_gruv), &mine);        /* a different theme, based on nord */
    strcpy(mine.base, "nord");
    strcpy(mine.name, "Mine");
    gte_open(&mine, "Mine");
    CHECK(!te8_same(&gt_ed.m, &base));
    e0 = gte_edits;
    CHECK_INT(gte_reset_to_base(tm_read, &fs), 1);
    CHECK(te8_same(&gt_ed.m, &base));                               /* the base theme's values are back */
    CHECK_STR(gt_ed.m.name, "Mine");                                /* it keeps its own name and base */
    CHECK_STR(gt_ed.m.base, "nord");
    CHECK(gte_edits > e0);                                          /* and goes live like an edit */
    CHECK(gte_dirty());
    gte_open(&gt_ed.m, "Mine");                                     /* an unedited copy of its base: unchanged */
    {
        gt_model before = gt_ed.m;
        CHECK_INT(gte_reset_to_base(tm_read, &fs), 1);
        CHECK(te8_same(&gt_ed.m, &before));
    }
    strcpy(gt_ed.m.base, "gone");                                   /* a base that no longer exists: refused, buffer kept */
    CHECK_INT(gte_reset_to_base(tm_read, &fs), 0);
    CHECK(te8_same(&gt_ed.m, &base));
    gt_ed.m.base[0] = 0;
    CHECK_INT(gte_reset_to_base(tm_read, &fs), 0);                  /* no base named */
    tm_free(&fs);
}

/* the rows the budget adds: counted from the builder (gte_nrows) and by visibility on a shallow and a deep screen */
static void test_gthemes_te8_rows(void)
{
    int i, share_all = 0, share_vis, total;
    te4_open_builtin();
    total = gte_nrows();
    for (i = 0; i < total; i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->kind == SD_INFO && !strncmp(s->label, "Pen Share ", 10)) share_all++;
    }
    CHECK_INT(share_all, GTG_COUNT);                                   /* one Pen Share row per group, all built */
    CHECK_INT(total, 484);                                             /* the true row count counted from the builder: 309 earlier rows + 174 Screens page rows (TE-10.8b) + 1 Restore Original (TE-10.9) */
    gt_ed.depth = 3;
    share_vis = 0;
    for (i = 0; i < total; i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->kind == SD_INFO && !strncmp(s->label, "Pen Share ", 10) && gte_row_visible(s)) share_vis++;
    }
    CHECK_INT(share_vis, 4);                                           /* nord at depth 3: groups 6 to 9 */
    gt_ed.depth = 8;
    share_vis = 0;
    for (i = 0; i < total; i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->kind == SD_INFO && !strncmp(s->label, "Pen Share ", 10) && gte_row_visible(s)) share_vis++;
    }
    CHECK_INT(share_vis, 0);
    gt_ed.depth = 0;
}

static void test_gthemes_te3_import_export_delete(void)
{
    tm_fs m;
    prefs_fs fs;
    char why[96], buf[2048];
    tm_file *f;
    tm_init(&m, &fs);
    tm_write(&m, "RAM:ok.theme", tm_theme, (long)strlen(tm_theme));
    tm_write(&m, "RAM:bad.theme", "this is not a theme\n", 20);
    CHECK(!gt_import(&fs, tm_read, &m, "RAM:bad.theme", "Bad", why, sizeof why));
    CHECK(strstr(why, "no key = value") != 0);                       /* the parser's message */
    CHECK(tm_find(&m, "ENVARC:Amarchy/themes/Bad.theme") == 0);      /* nothing copied */
    CHECK(!gt_import(&fs, tm_read, &m, "RAM:missing.theme", "Bad", why, sizeof why));
    CHECK(gt_import(&fs, tm_read, &m, "RAM:ok.theme", "Imp", why, sizeof why));
    f = tm_find(&m, "ENVARC:Amarchy/themes/Imp.theme");
    CHECK(f && f->len == (long)strlen(tm_theme) && !memcmp(f->data, tm_theme, (size_t)f->len));   /* byte for byte */
    CHECK(tm_find(&m, "ENV:Amarchy/themes/Imp.theme") != 0);
    CHECK(!gt_import(&fs, tm_read, &m, "RAM:ok.theme", "Imp", why, sizeof why));   /* taken */
    CHECK(gt_export(&fs, tm_read, &m, "Imp", "RAM:out.theme", why, sizeof why));
    CHECK_INT((int)tm_read(&m, "RAM:out.theme", buf, sizeof buf), (int)strlen(tm_theme));
    CHECK(gt_duplicate(&fs, tm_read, &m, "Imp", "Imp Two", why, sizeof why));
    CHECK(tm_find(&m, "ENVARC:Amarchy/themes/Imp Two.theme") != 0);
    CHECK(!gt_duplicate(&fs, tm_read, &m, "Nothing", "X", why, sizeof why));
    /* Delete User Theme: a shipped theme of that name shows again, the shipped file is never touched */
    tm_write(&m, "UP-Term:Amarchy/Themes/Imp.theme", tm_theme, (long)strlen(tm_theme));
    CHECK(gt_delete_user(&fs, "Imp", why, sizeof why));
    CHECK(tm_find(&m, "ENVARC:Amarchy/themes/Imp.theme") == 0);
    CHECK(tm_find(&m, "ENV:Amarchy/themes/Imp.theme") == 0);
    CHECK(tm_find(&m, "UP-Term:Amarchy/Themes/Imp.theme") != 0);
    CHECK(!gt_delete_user(&fs, "Imp", why, sizeof why));             /* only the shipped one is left: nothing to delete */
    tm_free(&m);
}

/* ---- TE-6: live apply. The editor's real entry point (lf_setting_set_text on a Theme row) reaches the live hook with the in-memory
 * model (sentinel gte_live_sends), the preview writes pens and the version-5 flat fields, Revert puts the pens back. ---- */
static uc_u32 te6_pen[8];
static desk_pv te6_pv;
static flat_state te6_flat;
static int te6_hook_calls, te6_model_ok;
static void te6_get(void *c, int pen, uc_u32 *rgb) { (void)c; *rgb = te6_pen[pen]; }
static void te6_set(void *c, int pen, uc_u32 rgb) { (void)c; te6_pen[pen] = rgb; }
static void te6_live(const gt_model *m)       /* what gt_dos_preview_model does, minus the Amiga calls */
{
    gtheme th;
    te6_hook_calls++;
    te6_model_ok = m == &gt_ed.m;
    gt_model_to_v1(m, &th);
    th.model = (gt_model *)m;
    desk_preview(&te6_pv, &th);
    desk_flat_look(&th, &te6_flat);
}

static void test_gthemes_te6_live(void)
{
    amconf_io io;
    int failed = 0, i;
    uc_u32 before[8];
    unsigned long sends0;
    const sd_setting *radius;
    memset(&io, 0, sizeof io);
    te4_open_builtin();
    radius = te4_row("Corner Radius");
    CHECK(radius != 0);
    for (i = 0; i < 8; i++)
        te6_pen[i] = before[i] = 0x101010u * (unsigned)(i + 1);
    memset(&te6_pv, 0, sizeof te6_pv);
    te6_pv.get = te6_get;
    te6_pv.set = te6_set;
    te6_pv.npens = 8;
    memset(&te6_flat, 0, sizeof te6_flat);
    gte_live = te6_live;
    sends0 = gte_live_sends;
    CHECK_INT(lf_setting_set_text(&io, radius, "9", &failed), 0);
    CHECK_INT(gte_live_sends - sends0, 1);          /* the sentinel: the edit ran the live hook once */
    CHECK_INT(te6_hook_calls, 1);
    CHECK(te6_model_ok);                            /* it got the in-memory buffer, not a file */
    CHECK_INT(te6_flat.corner_radius, 9);           /* the number the edit set reached the v5 field */
    CHECK(te6_pv.have);
    CHECK(memcmp(te6_pen, before, sizeof before) != 0);   /* the preview moved the pens */
    /* Revert: the model reloads and the desk restore puts the pens back */
    gte_revert();
    CHECK_INT(te6_hook_calls, 2);                   /* revert shows the reloaded model too */
    CHECK(desk_preview(&te6_pv, 0));
    CHECK(memcmp(te6_pen, before, sizeof before) == 0);
    CHECK(te6_flat.corner_radius != 9);
    gte_live = 0;
}

/* ---- TE-7.1 (D1): the editor's Save asks for the Workbench reset only when a reset-bound field changed. Real entry points: the row
 * edit (lf_setting_set_text) and gt_save_editor, with the sentinel gt_dos_reset_prompts. ---- */
static unsigned te7_asked_mask;
static int te7_asks;
static void te7_ask(unsigned mask) { te7_asked_mask = mask; te7_asks++; }

static void test_gthemes_te7_reset_prompt(void)
{
    tm_fs m;
    prefs_fs fs;
    amconf_io io;
    char why[96];
    int failed = 0;
    unsigned long p0;
    const sd_setting *radius, *bar;
    memset(&io, 0, sizeof io);
    tm_init(&m, &fs);
    te4_open_builtin();
    radius = te4_row("Corner Radius");
    bar = te4_row("Title Bar Height");
    CHECK(radius != 0 && bar != 0);
    p0 = gt_dos_reset_prompts;
    /* a live field: saved and applied, no prompt */
    CHECK_INT(lf_setting_set_text(&io, radius, "7", &failed), 0);
    CHECK(gt_save_editor(&fs, "nord", &gt_ed.m, "nord", tm_apply, wbr_bound_mask, te7_ask, why, sizeof why));
    CHECK_INT(te7_asks, 0);
    CHECK_INT((int)(gt_dos_reset_prompts - p0), 0);
    gte_saved();
    /* a reset-bound field: the prompt, with exactly that field in the mask */
    CHECK_INT(lf_setting_set_text(&io, bar, "30", &failed), 0);
    CHECK(gt_save_editor(&fs, "nord", &gt_ed.m, "nord", tm_apply, wbr_bound_mask, te7_ask, why, sizeof why));
    CHECK_INT(te7_asks, 1);
    CHECK_INT((int)te7_asked_mask, WBR_B_BAR_HEIGHT);
    CHECK_INT((int)(gt_dos_reset_prompts - p0), 1);          /* the sentinel: the ask ran once through Save */
    gte_saved();
    /* saved again unchanged: nothing new to reset */
    CHECK(gt_save_editor(&fs, "nord", &gt_ed.m, "nord", tm_apply, wbr_bound_mask, te7_ask, why, sizeof why));
    CHECK_INT(te7_asks, 1);
    /* a reset-bound edit saved while another theme is the current one: nothing running changes */
    CHECK_INT(lf_setting_set_text(&io, bar, "31", &failed), 0);
    CHECK(gt_save_editor(&fs, "nord", &gt_ed.m, "other", tm_apply, wbr_bound_mask, te7_ask, why, sizeof why));
    CHECK_INT(te7_asks, 1);
    tm_free(&m);
}

/* TE-10.7: the drawn menu and the theme windows used the main theme on every screen */
static void test_drawn_windows_take_their_screens_theme(void)
{
    gt_screen_rules r;
    static char big[131072];
    char out[GT_NAME], small[5];
    const char *f = "default workbench\nDOPUS.1 = nord\nSkipme = exclude\n";
    long n;
    gt_screen_rules_parse(f, (long)strlen(f), &r);
    gt_screen_draw_theme(&r, "DOPUS.1", "gruvbox", out, (int)sizeof out);
    CHECK_STR(out, "nord");
    gt_screen_draw_theme(&r, "Workbench", "gruvbox", out, (int)sizeof out);
    CHECK_STR(out, "gruvbox");
    gt_screen_draw_theme(&r, "Skipme", "gruvbox", out, (int)sizeof out);
    CHECK_STR(out, "gruvbox");
    gt_screen_draw_theme(&r, "Unlisted", "gruvbox", out, (int)sizeof out);
    CHECK_STR(out, "gruvbox");
    gt_screen_draw_theme(&r, 0, "gruvbox", out, (int)sizeof out);
    CHECK_STR(out, "gruvbox");
    gt_screen_draw_theme(&r, "", "gruvbox", out, (int)sizeof out);
    CHECK_STR(out, "gruvbox");
    memset(small, 'x', sizeof small);
    gt_screen_draw_theme(&r, "DOPUS.1", "gruvbox", small, 5);
    CHECK((int)strlen(small) < 5);
    /* the callers: both windows ask the DOS side for their screen's theme; the drawn menu no longer reads the main theme */
    n = read_file("amarchy/gui.c", big, (long)sizeof big - 1);
    CHECK(n > 0);
    if (n > 0) {
        char *pg;
        big[n] = 0;
        CHECK(strstr(big, "ipd_screen_theme_name(") != 0);
        pg = strstr(big, "static void pens_get(gui *g, const char *name)\n{");
        CHECK(pg != 0);
        if (pg) {
            char *end = strstr(pg, "\n}\n");
            if (end)
                *end = 0;
            CHECK(strstr(pg, "gt_dos_load_current") == 0);
        }
    }
    n = read_file("amarchy/themewin.c", big, (long)sizeof big - 1);
    CHECK(n > 0);
    if (n > 0) {
        big[n] = 0;
        CHECK(strstr(big, "ipd_screen_theme_name(") != 0);
    }
    /* a public screen name is up to 139 characters (IP_PSN_NAME 140, as the rule pattern GT_SCR_PAT): a shorter name buffer
     * cuts the name and the screen's exact-name rule no longer matches */
    CHECK_INT(IP_PSN_NAME >= GT_SCR_PAT, 1);
    n = read_file("amarchy/iconpen_dos.c", big, (long)sizeof big - 1);
    CHECK(n > 0);
    if (n > 0) {
        char *fn;
        big[n] = 0;
        fn = strstr(big, "void ipd_screen_theme_name(struct Screen *scr, char *out, int cap)\n{");
        CHECK(fn != 0);
        if (fn) {
            char *end = strstr(fn, "\n}\n");
            if (end)
                *end = 0;
            CHECK(strstr(fn, "char name[IP_PSN_NAME]") != 0);
            /* the rules (32 x 188 bytes) stay off the stack: themewin.c calls this in the Amarchy main program (16384 bytes) */
            CHECK(strstr(fn, "static gt_screen_rules rules;") != 0);
        }
    }
}

/* ---- TE-10.8b: the Screens page of the Theme Editor ---- */
static const sd_setting *scr_row(int aux, int idx)
{
    int i;
    for (i = SDI_COUNT; i < SDI_COUNT + gte_nrows(); i++) {
        const sd_setting *s = gte_row(i);
        if (s->page == PG_PAGE_THEME_SCREENS && s->aux == aux && s->aux2 == idx)
            return s;
    }
    return 0;
}

static void scr_load(const char *text, const char *const *open, int nopen)
{
    static const char *const themes[] = { "nord", "gruvbox" };
    gt_se_set_themes(themes, 2);
    gt_se_load(text, text ? (long)strlen(text) : 0, open, nopen);
}

static void scr_label(const sd_setting *s, char *out)
{
    out[0] = 0;
    if (s)
        gte_row_label(s, out, 96);
}

static void test_gthemes_screens_page_rows_follow_the_open_screens_and_edit_the_rules(void)
{
    const char *o[1];
    const sd_setting *s;
    char t[96], buf[1024];
    o[0] = "DOPUS.1";
    scr_load(0, o, 1);
    s = scr_row(GTSE_TICK, 0);
    CHECK(s != 0 && gte_row_visible(s));
    scr_label(s, t);
    CHECK_STR(t, "Workbench: On");
    s = scr_row(GTSE_TICK, 1);
    CHECK(s != 0 && gte_row_visible(s));
    scr_label(s, t);
    CHECK_STR(t, "DOPUS.1: Off");
    CHECK(scr_row(GTSE_TICK, 2) != 0 && !gte_row_visible(scr_row(GTSE_TICK, 2)));
    CHECK(!gte_row_visible(scr_row(GTSE_THEME, 2)));
    CHECK_INT(gte_row_step(s), GTR_OK);
    scr_label(s, t);
    CHECK_STR(t, "DOPUS.1: On");
    CHECK(gt_se_text(buf, sizeof buf) > 0);
    CHECK_STR(buf, "default workbench\nDOPUS.1 = :main\n");
    CHECK_INT(gte_row_set_text(scr_row(GTSE_THEME, 1), "Nord"), GTR_OK);
    CHECK_INT(gte_row_step(scr_row(GTSE_TAKE, 1)), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK_STR(buf, "default workbench\nDOPUS.1 = nord takeover\n");
    CHECK_INT(gte_row_set_text(scr_row(GTSE_THEME, 1), "nosuch"), GTR_BAD);
    CHECK_INT(gte_row_set_text(scr_row(GTSE_THEME, 0), "nord"), GTR_BAD);
    CHECK_INT(gte_row_step(scr_row(GTSE_TAKE, 0)), GTR_BAD);
    CHECK(gt_se_dirty());
}

static void test_gthemes_screens_page_default_buttons_and_closed_rules(void)
{
    const char *o[2];
    const sd_setting *d;
    char t[96], buf[1024];
    o[0] = "DOPUS.1";
    o[1] = "Term";
    scr_load("default none\nGone = nord\n", o, 2);
    d = scr_row(GTSE_DEFAULT, 0);
    CHECK(d != 0);
    scr_label(d, t);
    CHECK_STR(t, "Default: None");
    scr_label(scr_row(GTSE_TICK, 3), t);
    CHECK_STR(t, "Gone (not open): On");
    CHECK_INT(gt_se_action(GTA_SCR_ALL, 0), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK(strstr(buf, "DOPUS.1 = :main") != 0 && strstr(buf, "Term = :main") != 0 && strstr(buf, "Gone = nord") != 0);
    CHECK_INT(gt_se_action(GTA_SCR_INVERT, 0), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK(strstr(buf, "DOPUS.1 = :main") == 0);
    CHECK_INT(gte_row_step(d), GTR_OK);
    scr_label(d, t);
    CHECK_STR(t, "Default: All Screens");
    CHECK_INT(gte_row_step(d), GTR_OK);
    scr_label(d, t);
    CHECK_STR(t, "Default: Workbench Only");
    CHECK_INT(gt_se_action(GTA_SCR_NONE, 0), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK(strstr(buf, "Term = :main") == 0);
}

static void test_gthemes_screens_page_pattern_rules_add_move_edit_remove(void)
{
    const char *o[1];
    const sd_setting *s;
    char t[96], buf[1024];
    o[0] = "Term";
    scr_load("default workbench\n", o, 1);
    CHECK(scr_row(GTSE_PAT, 0) != 0 && !gte_row_visible(scr_row(GTSE_PAT, 0)));
    CHECK_INT(gt_se_add_rule("DOPUS#?"), GTR_OK);
    CHECK_INT(gt_se_add_rule("NoWildcard"), GTR_BAD);
    CHECK_INT(gt_se_add_rule("Cli#?"), GTR_OK);
    CHECK(gte_row_visible(scr_row(GTSE_PAT, 1)) && !gte_row_visible(scr_row(GTSE_PAT, 2)));
    gt_se_text(buf, sizeof buf);
    CHECK_STR(buf, "default workbench\nDOPUS#? = :main\nCli#? = :main\n");
    CHECK_INT(gt_se_action(GTA_SCR_DOWN, 0), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK_STR(buf, "default workbench\nCli#? = :main\nDOPUS#? = :main\n");
    CHECK_INT(gt_se_action(GTA_SCR_UP, 0), GTR_BAD);
    CHECK_INT(gte_row_set_text(scr_row(GTSE_PTHEME, 0), "Exclude"), GTR_OK);
    CHECK_INT(gte_row_step(scr_row(GTSE_PTAKE, 0)), GTR_BAD);
    CHECK_INT(gte_row_set_text(scr_row(GTSE_PTHEME, 0), "gruvbox"), GTR_OK);
    CHECK_INT(gte_row_step(scr_row(GTSE_PTAKE, 0)), GTR_OK);
    CHECK_INT(gte_row_set_text(scr_row(GTSE_PAT, 1), "Dir#?"), GTR_OK);
    s = scr_row(GTSE_PAT, 1);
    scr_label(s, t);
    CHECK_STR(t, "Rule 2 Pattern: Dir#?");
    gt_se_text(buf, sizeof buf);
    CHECK_STR(buf, "default workbench\nCli#? = gruvbox takeover\nDir#? = :main\n");
    CHECK_INT(gt_se_action(GTA_SCR_REMOVE, 0), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK_STR(buf, "default workbench\nDir#? = :main\n");
    CHECK_INT(gt_se_action(GTA_SCR_REMOVE, 5), GTR_BAD);
}

/* TE-10.9: Restore Original puts the rules model back to "no rule, default Workbench only" (the stock file), whatever was there */
static void test_gthemes_screens_restore_original_clears_every_rule(void)
{
    const char *o[2];
    const sd_setting *s;
    char t[96], buf[1024], stock[1024];
    int th = 0;
    o[0] = "DOPUS.1";
    o[1] = "Term";
    scr_load("default workbench\n", o, 2);
    gt_se_text(stock, sizeof stock);
    CHECK_STR(stock, "default workbench\n");
    scr_load("default all\nDOPUS.1 = nord\nTerm = gruvbox takeover\n", o, 2);
    CHECK_INT(gt_se_add_rule("DOPUS#?"), GTR_OK);
    CHECK_INT(gt_se_add_rule("Cli#?"), GTR_OK);
    CHECK_INT(gt_se_enabled(), 1);
    s = scr_row(GTA_SCR_RESTORE, 0);
    CHECK(s != 0 && s->kind == SD_GACTION);
    scr_label(s, t);
    CHECK_STR(t, "Restore Original");
    CHECK_INT(gt_se_action(GTA_SCR_RESTORE, 0), GTR_OK);
    gt_se_text(buf, sizeof buf);
    CHECK_STR(buf, stock);
    CHECK_INT(gt_se_enabled(), 0);
    CHECK_INT(gt_screen_pat_count(gt_se_rules()), 0);
    CHECK_INT(gt_screen_theme_of(gt_se_rules(), "DOPUS.1", "nord", t, sizeof t, &th), 0);   /* no screen but the Workbench is themed */
    CHECK_INT(gt_screen_theme_of(gt_se_rules(), "Term", "nord", t, sizeof t, &th), 0);
    CHECK_INT(gt_screen_theme_of(gt_se_rules(), "Workbench", "nord", t, sizeof t, &th), 1);
}

static int scr_reloads;
static void scr_reloader(void)
{
    static const char *const o[] = { "DOPUS.1", "TE10" };
    scr_reloads++;
    scr_load(0, o, 2);
}

/* A screen opened after the page loaded shows at the next menu open (gt_se_stale), and that reload never throws away unsaved edits. */
static void test_gthemes_screens_page_reloads_when_stale_and_keeps_unsaved_edits(void)
{
    const char *o[1];
    o[0] = "DOPUS.1";
    scr_load(0, o, 1);
    gt_se_loader = scr_reloader;
    scr_reloads = 0;
    CHECK(!gte_row_visible(scr_row(GTSE_TICK, 2)));
    CHECK_INT(scr_reloads, 0);
    gt_se_stale();
    CHECK(gte_row_visible(scr_row(GTSE_TICK, 2)));                      /* Workbench, DOPUS.1 and now TE10 */
    CHECK_INT(scr_reloads, 1);
    CHECK(gte_row_visible(scr_row(GTSE_TICK, 2)));
    CHECK_INT(scr_reloads, 1);                                          /* one reload per stale mark, not one per row */
    CHECK_INT(gte_row_set_index(scr_row(GTSE_TICK, 1), 1), GTR_OK);     /* an unsaved edit */
    gt_se_stale();
    CHECK(gte_row_visible(scr_row(GTSE_TICK, 2)));
    CHECK_INT(scr_reloads, 1);
    CHECK(gt_se_dirty());
    gt_se_loader = 0;
}


void suite_gthemes(void)
{
    test_drawn_windows_take_their_screens_theme();
    test_gthemes_te7_reset_prompt();
    test_gthemes_te6_live();
    test_gthemes_te3_save();
    test_gthemes_te3_custom_name();
    test_gthemes_te3_edit_applies();
    test_gthemes_te4_section4();
    test_gthemes_te4_pickers();
    test_gthemes_te4_dispatch();
    test_gthemes_te4_leave();
    test_gthemes_te4_rows();
    test_gthemes_te7_no_title_font_row();
    test_gthemes_te4_modes();
    test_gthemes_te3_import_export_delete();
    test_gthemes_nearest_role_pen_ties_low_and_caps_at_depth();
    test_gthemes_te8_budget();
    test_gthemes_te105_plan();
    test_gthemes_te8_contrast();
    test_gthemes_te8_stock_inactive_title_text_reads_150_on_its_bar();
    test_gthemes_te8_button_face_equals_requester_gets_no_warning();
    test_gthemes_te8_presets();
    test_gthemes_te8_reset_base();
    test_gthemes_te8_rows();
    test_gthemes_parse_clears_model();
    test_gthemes_te2_baseline();
    test_gthemes_te2_overrides();
    test_gthemes_model_golden();
    test_gthemes_model_refs();
    test_gthemes_model_cycle();
    test_gthemes_model_full_and_extras();
    test_gthemes_model_attach();
    test_gthemes_role_table();
    test_gthemes_inactive_key();
    test_gthemes_desk_keys();
    test_gthemes_ink_direct();
    test_gthemes_save_text();
    test_gthemes_select_name();
    test_gthemes_preview();
    test_gthemes_preview_term();
    test_gthemes_text_menu_preview_recolours_consoles();
    test_gthemes_enter_backs_one_level();
    test_gthemes_fallback();
    test_gthemes_parse();
    test_gthemes_term_text();
    test_gthemes_term_font();
    test_gthemes_boot_term();
    test_gthemes_text_grey();
    test_gthemes_roles();
    test_gthemes_user();
    test_gthemes_amconf();
    test_gthemes_scan();
    test_gthemes_picker_pens();
    test_gthemes_screen_rules();
    test_gthemes_screen_rows_fresh_rules_list_workbench_then_open_screens();
    test_gthemes_screen_main_token_carries_the_main_theme_for_a_named_screen();
    test_gthemes_screen_row_tick_on_off_under_each_default_and_rule();
    test_gthemes_screen_row_theme_and_takeover_edit_the_exact_rule();
    test_gthemes_screen_row_exact_rule_beats_a_pattern_rule_that_stands_first();
    test_gthemes_screen_rows_all_none_invert_flip_the_listed_screens_only();
    test_gthemes_screen_rows_list_rules_of_closed_screens_after_the_open_ones();
    test_gthemes_screen_rows_a_full_rule_table_refuses_and_changes_nothing();
    test_gthemes_screen_rows_survive_write_and_parse();
    test_gthemes_screens_page_rows_follow_the_open_screens_and_edit_the_rules();
    test_gthemes_screens_page_default_buttons_and_closed_rules();
    test_gthemes_screens_page_pattern_rules_add_move_edit_remove();
    test_gthemes_screens_restore_original_clears_every_rule();
    test_gthemes_screens_page_reloads_when_stale_and_keeps_unsaved_edits();
}
