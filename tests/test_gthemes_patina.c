/* The Patina GUI themes (render/gthemes_core.c, menu_theme_roles_gui, the AM_GTHEME key; plan
 * 2026-10-26-patina-gui-themes.md checks H1..H8). The shipped files are read from dist/patina-themes. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../config/amconf_core.h"
#include "../render/desk_core.h"
#include "../render/flat_core.h"
#include "../render/gthemes_core.h"
#include "../render/iconpen_core.h"
#include "../patina/wbreset_core.h"
#include "../patina/leaf.h"
#include "../prefs/settings_def.h"
#include "../patina/menu_core.h"
#include "../patina/menu_theme.h"
#include "../patina/pen_cache.h"
#include "harness.h"
#include "memfs.h"


#define THEME_DIR "dist/patina-themes"

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
            CHECK(dopus_theme_commands(&t, out, sizeof out, 1, 0) > 0);
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
/* T9: walking Style > Theme recolours the menu in the row's theme; leaving the list restores the saved one */
static char tp_sent[8][GT_NAME];
static int tp_n;
static void tp_send(const char *name)
{
    if (tp_n < 8)
        strcpy(tp_sent[tp_n], name);
    tp_n++;
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

/* TE-COLOUR-LIST-ALL-SWATCHES: the colour chips of one drawn menu frame. A pen released right after its RectFill is free for
 * the next chip's colour, and the same pen number then recolours every chip already drawn with it (rig 172: seven chips,
 * one colour). The frame holds each chip pen until the next full redraw (gui.c chip_pens), through pen_cache. */
static int cp_releases[PM_N];
static void cp_release(void *u, long pen)
{
    (void)u;
    cp_releases[pen]++;
    pm_used[pen] = 0;
}

static long cp_seq(void *u, unsigned long rgb, int exact)
{
    static long next;
    (void)rgb; (void)exact;
    if (u)
        next = 0;
    return u ? 0 : next++;
}

static void test_gthemes_chip_pens_held_per_frame(void)
{
    static pen_cache pc;
    unsigned long cols[5] = { 0x000000UL, 0xCD0000UL, 0x00CD00UL, 0xCDCD00UL, 0x0000EEUL };
    long pen[5], again;
    int i;
    memset(pm_used, 0, sizeof pm_used);
    memset(&pc, 0, sizeof pc);
    memset(cp_releases, 0, sizeof cp_releases);
    /* the old pattern: obtain, draw, release at once -> every colour lands on one pen number */
    for (i = 0; i < 5; i++) {
        pen[i] = vr_obtain_rule(pm_best, 0, cols[i]);
        pm_release(0, pen[i]);
    }
    CHECK_INT(pen[0], pen[4]);
    /* held: five colours, five pens, each pen carries its colour when the frame is done */
    for (i = 0; i < 5; i++)
        pen[i] = pc_of(&pc, pm_best, 0, cols[i]);
    for (i = 0; i < 5; i++) {
        int j;
        CHECK(pen[i] >= 0);
        CHECK_INT(pm_col[pen[i]], cols[i]);
        for (j = 0; j < i; j++)
            CHECK(pen[i] != pen[j]);
    }
    again = pc_of(&pc, pm_best, 0, cols[2]);          /* the same rgb twice is one entry */
    CHECK_INT(again, pen[2]);
    CHECK_INT(pc.n, 5);
    pc_theme_changed(&pc, cp_release, 0);             /* the next full redraw: every pen back exactly once */
    for (i = 0; i < 5; i++)
        CHECK_INT(cp_releases[pen[i]], 1);
    CHECK_INT(pc.n, 0);
    /* the bound: PC_MAX distinct colours get a pen, the next gets -1 (no chip) and nothing leaks */
    cp_seq((void *)1, 0, 0);
    for (i = 0; i < PC_MAX; i++)
        CHECK(pc_of(&pc, cp_seq, 0, 0x100000UL + (unsigned long)i) >= 0);
    CHECK_INT(pc_of(&pc, cp_seq, 0, 0x200000UL), -1);
    CHECK_INT(pc.n, PC_MAX);
}

/* TE-COLOUR-LIST-ALL-SWATCHES follow-up: a scroll repaints the list (gui.c draw_list) without a full redraw. Frames of
 * MN_OSD_MAX_ENTRIES rows through more than PC_MAX distinct colours: with the release at each frame every chip gets a pen;
 * without it the cache fills and the chips stop (the defect). */
static void test_gthemes_chip_pens_scroll_frames(void)
{
    static pen_cache pc;
    int top, row, rows = 10, total = PC_MAX + 8;
    int missing_held = 0, missing_released = 0;
    memset(&pc, 0, sizeof pc);
    cp_seq((void *)1, 0, 0);
    for (top = 0; top + rows <= total; top++)           /* no release: the cache of 3b903f8d on a scroll */
        for (row = top; row < top + rows; row++)
            if (pc_of(&pc, cp_seq, 0, 0x300000UL + (unsigned long)row) < 0)
                missing_held++;
    CHECK(missing_held > 0);
    memset(&pc, 0, sizeof pc);
    memset(cp_releases, 0, sizeof cp_releases);
    for (top = 0; top + rows <= total; top++) {
        long seen[10];
        pc_theme_changed(&pc, cp_release, 0);           /* the frame start (draw_list): the pens go back, the numbers are free again */
        cp_seq((void *)1, 0, 0);
        for (row = top; row < top + rows; row++) {
            int j;
            seen[row - top] = pc_of(&pc, cp_seq, 0, 0x300000UL + (unsigned long)row);
            if (seen[row - top] < 0)
                missing_released++;
            for (j = 0; j < row - top; j++)
                CHECK(seen[j] != seen[row - top]);      /* no pen twice within one frame */
        }
        CHECK_INT(pc.n, rows);
    }
    CHECK_INT(missing_released, 0);
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
static char te_buf[UC_MAX_FILE];


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
    CHECK(gte_row_label(radius, buf, sizeof buf) > 0 && !strstr(buf, ": 7"));
}

/* TE-4.2: swatch, swatch-reference and element rows open the colour picker (palette entry or Custom) and step/set through the
 * lf_setting_* handlers, writing the edit buffer. TE-4 item 2: Rename/Remove Swatch look the swatch up by name. */
/* TE-4.5: every row of plan section 4 exists on its page with its full English label, and the reused rows (Desktop switches, Icon
 * Label Style, Terminal Palette) write the store of the row they copy; Icon Label Colour is an element row visible in Simple mode. */
static void test_gthemes_te4_rows(void)
{
    static const struct { const char *label; int page; } want[] = {
        { "Mode", PG_PAGE_THEME }, { "Base Theme", PG_PAGE_THEME }, { "Save As...", PG_PAGE_THEME },
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
    io.alloc = mf_prefs_alloc;
    io.release = mf_prefs_release;
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
    CHECK(r && r->kind == SD_CYCLE && r->store == SD_PATINA && r->nvalues == 2);
    if (r) {
        CHECK_INT(lf_setting_set(&io, r, 0, &failed), LF_SET_OK);       /* writes the real desktop-backdrop key */
        CHECK(amconf_load_with(&io, &am) != AMCONF_DEFAULTS);
        CHECK_STR(am.val[AM_DESKBACK], "off");
    }
    r = te4_row("Icon Label Style");
    CHECK(r && r->store == SD_PATINA && r->nvalues == 3);
    if (r) {
        CHECK_INT(lf_setting_set(&io, r, 1, &failed), LF_SET_OK);
        CHECK(amconf_load_with(&io, &am) != AMCONF_DEFAULTS);
        CHECK_STR(am.val[AM_ICONTEXT], "outline");
    }
}

/* Autosave: every edit of the buffer reaches the store hook (the one choke point, ed_touch), and the saved snapshot follows the buffer. */
static int as_writes;
static char as_name[GT_NAME];
static int as_store(const char *name, const gt_model *m)
{
    (void)m;
    as_writes++;
    strcpy(as_name, name);
    return 1;
}

static void test_gthemes_te4_autosave(void)
{
    amconf_io io;
    int failed = 0;
    const sd_setting *radius;
    unsigned long w0;
    memset(&io, 0, sizeof io);
    te4_open_builtin();
    radius = te4_row("Corner Radius");
    CHECK(radius != 0);
    gte_store = as_store;
    w0 = gte_store_writes;
    CHECK_INT(lf_setting_set_text(&io, radius, "7", &failed), 0);
    CHECK_INT(as_writes, 1);
    CHECK_STR(as_name, "nord");
    CHECK_INT((int)(gte_store_writes - w0), 1);
    CHECK(memcmp(&gt_ed.m, &gt_ed.saved, sizeof gt_ed.m) == 0);     /* no dirty state: saved follows the edit */
    gte_revert();                                                    /* back to the theme as it opened, saved too */
    CHECK_INT(as_writes, 2);
    CHECK(memcmp(&gt_ed.m, &gt_ed.saved, sizeof gt_ed.m) == 0);
    CHECK(gt_ed.m.look.corner_radius != 7);
    gte_store = 0;
}

static void test_gthemes_te4_dispatch(void)
{
    const sd_setting *sw = 0, *el = 0, *pal = 0;
    rd_ctx rc;
    int i;
    rc.calls = 0;
    /* theme_action: what each action does before it touches a file */
    CHECK_INT(gte_action_plan(GTA_OPEN, 0), 0);
    CHECK_INT(gte_action_plan(GTA_OPEN_OTHER, 0), GTP_NAME);
    CHECK_INT(gte_action_plan(GTA_RESET_BASE, 1), 0);
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
    CHECK(gt_ed.open);
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

/* TE-SWATCH-BUGS: the colour picked in a swatch row's level reaches the swatch (a built-in one and an added one), and the
 * Terminal Palette row stores to the same place as the Settings palette row (SD_UPTERM, the profile file). */
static const sd_setting *te4_swatch_row(int sw)
{
    int i;
    for (i = 0; i < gte_nrows(); i++) {
        const sd_setting *s = gte_row(SDI_COUNT + i);
        if (s->kind == SD_SWATCH && s->aux == sw)
            return s;
    }
    return 0;
}

static void test_gthemes_swatch_and_palette_rows_apply(void)
{
    memfs mfs;
    prefs_fs pfs;
    amconf_io io;
    const sd_setting *row, *pal = te4_row("Terminal Palette");
    int failed = 0, added;
    char hex[16], buf[32];
    mf_init(&mfs, &pfs);
    io.fs = &pfs;
    io.read = mf_read;
    io.dir = mf_dir;
    io.alloc = mf_prefs_alloc;
    io.release = mf_prefs_release;
    mf_put(&mfs, "ENVARC:up-term/up-term", "");
    te4_open_builtin();
    row = te4_swatch_row(0);
    CHECK(row != 0);
    CHECK_INT(lf_setting_set_colour(&io, row, SD_PICK_CUSTOM, "112233", &failed), LF_SET_OK);
    gte_row_text(row, buf, sizeof buf);
    CHECK_STR(buf, "#112233");
    added = gte_swatch_add("added", 0x808080UL);
    CHECK(added >= 0);
    row = te4_swatch_row(added);
    CHECK(row != 0);
    CHECK_INT(lf_setting_set_colour(&io, row, SD_PICK_CUSTOM, "445566", &failed), LF_SET_OK);
    gte_row_text(row, buf, sizeof buf);
    CHECK_STR(buf, "#445566");
    /* S88: Red, Green, Blue bars: step 1 and 16, clamp at 0 and 255, RRGGBB rebuilt, the picker prompt is 0 for a bar */
    CHECK_INT(lf_bar_value(&io, row, SD_PICK_BAR), 0x44);
    CHECK_INT(lf_bar_value(&io, row, SD_PICK_BAR + 2), 0x66);
    CHECK_INT(lf_bar_value(&io, row, 1), -1);
    CHECK_INT(lf_bar_step(&io, row, SD_PICK_BAR + 1, 1), LF_SET_OK);
    gte_row_text(row, buf, sizeof buf);
    CHECK_STR(buf, "#445666");
    CHECK_INT(lf_bar_step(&io, row, SD_PICK_BAR + 1, -1), LF_SET_OK);
    CHECK_INT(lf_bar_step(&io, row, SD_PICK_BAR, 16), LF_SET_OK);
    gte_row_text(row, buf, sizeof buf);
    CHECK_STR(buf, "#545566");
    CHECK_INT(lf_bar_step(&io, row, SD_PICK_BAR, 1000), LF_SET_OK);
    CHECK_INT(lf_bar_value(&io, row, SD_PICK_BAR), 255);
    CHECK_INT(lf_bar_step(&io, row, SD_PICK_BAR, -1000), LF_SET_OK);
    CHECK_INT(lf_bar_value(&io, row, SD_PICK_BAR), 0);
    gte_row_text(row, buf, sizeof buf);
    CHECK_STR(buf, "#005566");
    CHECK(lf_picker_prompt(row, SD_PICK_BAR) == 0);
    /* the Terminal Palette row is the Settings palette row: same store, same slot, same file */
    CHECK(pal != 0 && pal->kind == SD_PALETTE && pal->store == SD_UPTERM);
    CHECK(pal->store == sd_settings[SDI_PALETTE].store && pal->off == sd_settings[SDI_PALETTE].off);
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "aa11bb", &failed), LF_SET_OK);
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "AA11BB");
    /* S89: the palette entry's own bars (SD_PICK_PALBAR + 3 * n + channel): read, step 1 and 16, clamp, same file */
    CHECK_INT(lf_bar_value(&io, pal, SD_PICK_PALBAR + 15), 0xAA);
    CHECK_INT(lf_bar_value(&io, pal, SD_PICK_PALBAR + 17), 0xBB);
    CHECK_INT(lf_bar_value(&io, pal, SD_PICK_BAR), -1);
    CHECK(lf_bar_entry(pal, SD_PICK_PALBAR + 47) && !lf_bar_entry(pal, SD_PICK_PALBAR + 48));
    CHECK_INT(lf_bar_step(&io, pal, SD_PICK_PALBAR + 16, 16), LF_SET_OK);
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "AA21BB");
    CHECK_INT(lf_bar_step(&io, pal, SD_PICK_PALBAR + 15, 1000), LF_SET_OK);
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "FF21BB");
    CHECK_INT(lf_setting_set_colour(&io, pal, 5, "aa11bb", &failed), LF_SET_OK);   /* the checks below expect the entry as set above */
    CHECK_INT(lf_setting_set_colour(&io, &sd_settings[SDI_PALETTE], 6, "010203", &failed), LF_SET_OK);
    lf_palette_hex(&io, 5, hex);
    CHECK_STR(hex, "AA11BB");                  /* the other row leaves slot 5 alone, and its slot 6 is in the same file */
    lf_palette_hex(&io, 6, hex);
    CHECK_STR(hex, "010203");
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
    io.alloc = mf_prefs_alloc;
    io.release = mf_prefs_release;
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
    /* TI-3: Pointer Colour 1-3 are picker rows; Custom stores the literal colour in look.pointer */
    {
        const sd_setting *pp = 0;
        for (i = 0; i < gte_nrows(); i++)
            if (!strcmp(gte_row(SDI_COUNT + i)->key, "pointer1"))
                pp = gte_row(SDI_COUNT + i);
        CHECK(pp != 0 && SD_HAS_PICKER(pp) && gte_row_visible(pp));
        CHECK_INT(lf_setting_set_colour(&io, pp, SD_PICK_CUSTOM, "0a0b0c", &failed), LF_SET_OK);
        CHECK(gt_ed.m.look.pointer[1] == 0x0a0b0cUL);
        gte_row_text(pp, buf, sizeof buf);
        CHECK_STR(buf, "#0a0b0c");
        CHECK_INT(lf_setting_set_colour(&io, pp, SD_PICK_DEFAULT, 0, &failed), LF_SET_OK);   /* Default clears the override */
        CHECK(gt_ed.m.look.pointer[1] == 0);
        CHECK_INT(lf_setting_set_colour(&io, sw, SD_PICK_DEFAULT, 0, &failed), LF_SET_KIND);   /* a real swatch has none */
    }
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
    CHECK_STR(it[1].label, "Expression...");                        /* a reference row: typed expression behind its own entry */
    lf_setting_chips(&io, it, 3, chip);
    CHECK(chip[0] >= 0 && chip[1] == -1 && chip[2] == 0x0a0b0cL);
    /* swatch names are the first-level choices of reference rows: Follow group (elements), swatches, Expression... */
    {
        char nm[24];
        int n2 = gte_swatch_count();
        snprintf(nm, sizeof nm, "%d", SD_PICK_SWATCH + 1);
        memset(it, 0, sizeof it);
        it[0].row = MN_ROW_CHOICE; it[0].ix = ref->id; strcpy(it[0].seg, nm);
        it[1].row = MN_ROW_CHOICE; it[1].ix = el->id; strcpy(it[1].seg, "17");
        lf_setting_labels(&io, it, 2);
        CHECK_STR(it[0].label, gte_swatch_name(1));
        CHECK_STR(it[1].label, "Follow group");
        CHECK(lf_picker_prompt(ref, SD_PICK_SWATCH + 1) == 0);      /* a swatch pick sets without asking */
        CHECK_INT(lf_setting_set_colour(&io, ref, SD_PICK_SWATCH + 1, 0, &failed), LF_SET_OK);
        gte_row_text(ref, buf, sizeof buf);
        CHECK_STR(buf, gte_swatch_name(1));
        CHECK_INT(lf_setting_set_colour(&io, ref, SD_PICK_SWATCH + n2, 0, &failed), LF_SET_KIND);
        CHECK_INT(lf_setting_set_colour(&io, ref, SD_PICK_DEFAULT, 0, &failed), LF_SET_KIND);
        CHECK_INT(lf_setting_set_colour(&io, el, SD_PICK_SWATCH + 2, 0, &failed), LF_SET_OK);
        CHECK(gte_elem_overridden(el->aux));
        CHECK_INT(lf_setting_set_colour(&io, el, SD_PICK_DEFAULT, 0, &failed), LF_SET_OK);
        CHECK(!gte_elem_overridden(el->aux));
    }
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

/* ---- TE-7.1 (D1), S87: the editor autosaves every change; the Workbench reset is asked ONCE, by gte_close, for the reset-bound
 * fields that differ from the gte_open snapshot. ---- */
static unsigned te7_asked_mask;
static int te7_asks;
static void te7_ask(unsigned mask) { te7_asked_mask = mask; te7_asks++; }

static void test_gthemes_leave_needed(void)
{
    CHECK(lf_leave_needed("settings.theme", "settings"));
    CHECK(lf_leave_needed("settings.theme.bar", ""));
    CHECK(!lf_leave_needed("settings.theme.bar", "settings.theme"));
    CHECK(!lf_leave_needed("settings", ""));
}

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
    /* a live field: saved, no ask at close */
    CHECK_INT(lf_setting_set_text(&io, radius, "7", &failed), 0);
    CHECK(gt_save_edit(&fs, "nord", &gt_ed.m, "nord", tm_apply, why, sizeof why));
    gte_close("nord", wbr_bound_mask, te7_ask);
    CHECK_INT(te7_asks, 0);
    /* three reset-bound changes, each saved: no ask until close, then exactly one with that field */
    CHECK_INT(lf_setting_set_text(&io, bar, "30", &failed), 0);
    CHECK(gt_save_edit(&fs, "nord", &gt_ed.m, "nord", tm_apply, why, sizeof why));
    CHECK_INT(lf_setting_set_text(&io, bar, "31", &failed), 0);
    CHECK(gt_save_edit(&fs, "nord", &gt_ed.m, "nord", tm_apply, why, sizeof why));
    CHECK_INT(lf_setting_set_text(&io, bar, "32", &failed), 0);
    CHECK(gt_save_edit(&fs, "nord", &gt_ed.m, "nord", tm_apply, why, sizeof why));
    CHECK_INT(te7_asks, 0);
    gte_close("nord", wbr_bound_mask, te7_ask);
    CHECK_INT(te7_asks, 1);
    CHECK_INT((int)te7_asked_mask, WBR_B_BAR_HEIGHT);
    CHECK_INT((int)(gt_dos_reset_prompts - p0), 1);
    gte_close("nord", wbr_bound_mask, te7_ask);              /* closed again: nothing new */
    CHECK_INT(te7_asks, 1);
    /* a change then Revert: back at the snapshot, no ask */
    CHECK_INT(lf_setting_set_text(&io, bar, "40", &failed), 0);
    gte_revert();
    gte_close("nord", wbr_bound_mask, te7_ask);
    CHECK_INT(te7_asks, 1);
    /* a reset-bound edit while another theme is the current one: nothing running changes */
    CHECK_INT(lf_setting_set_text(&io, bar, "41", &failed), 0);
    gte_close("other", wbr_bound_mask, te7_ask);
    CHECK_INT(te7_asks, 1);
    tm_free(&m);
}


void suite_gthemes_patina(void)
{
    test_gthemes_leave_needed();
    test_gthemes_te7_reset_prompt();
    test_gthemes_te6_live();
    test_gthemes_te4_section4();
    test_gthemes_te4_pickers();
    test_gthemes_swatch_and_palette_rows_apply();
    test_gthemes_te4_dispatch();
    test_gthemes_te4_autosave();
    test_gthemes_te4_rows();
    test_gthemes_te4_modes();
    test_gthemes_te2_baseline();
    test_gthemes_te2_overrides();
    test_gthemes_role_table();
    test_gthemes_ink_direct();
    test_gthemes_enter_backs_one_level();
    test_gthemes_roles();
    test_gthemes_picker_pens();
    test_gthemes_chip_pens_held_per_frame();
    test_gthemes_chip_pens_scroll_frames();
}
