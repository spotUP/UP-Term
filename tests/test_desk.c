/* The pure core of the Amarchy native desktop theme (amarchy/desk_core.c; plan 2026-10-29-amarchy-p1a-native-theme.md
 * items R1.2-R1.6, R1.16a, R1.18a-R1.22a, D-TGT). Goldens: tests/data/desk/ (probe note 2026-10-29-amarchy-p1a-prefs-probe.md).
 * The product entry point (desk.c, the menu rows) comes with the OS half; these tests drive the core directly. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/desk_core.h"
#include "harness.h"

#define DESK_DATA "tests/data/desk/"
#define SHIPPED_DIR "dist/amarchy-themes"
#define SHIPPED_MAX 64
#define BUF_MAX 8192

static long slurp(const char *path, unsigned char *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

/* every shipped theme, parsed; returns the count (counted from the directory, not from a list) */
static int load_all(gtheme *out, int cap)
{
    DIR *d = opendir(SHIPPED_DIR);
    struct dirent *e;
    int n = 0;
    if (!d)
        return 0;
    while ((e = readdir(d)) != 0 && n < cap) {
        size_t l = strlen(e->d_name);
        char path[300], buf[UC_MAX_FILE];
        long got;
        if (l < 7 || strcmp(e->d_name + l - 6, ".theme"))
            continue;
        snprintf(path, sizeof path, SHIPPED_DIR "/%s", e->d_name);
        got = slurp(path, (unsigned char *)buf, sizeof buf);
        if (got <= 0)
            continue;
        gt_builtin(&out[n]);
        gt_parse(buf, got, &out[n]);
        snprintf(out[n].name, GT_NAME, "%.*s", (int)(l - 6), e->d_name);
        n++;
    }
    closedir(d);
    return n;
}

/* the probe's colours: palette-gui-save.prefs holds black, white, #6688bb, #ee4444, #55dd55, #0044dd, #ee9900 */
static void probe_theme(gtheme *t)
{
    gt_builtin(t);
    t->bg = 0x13a907UL; /* entry 0 of the golden was picked by hand, it is masked in the comparison */
    t->darker_bg = 0x000000UL;
    t->fg = 0xffffffUL;
    t->accent = 0x6688bbUL;
    t->selection = 0xee4444UL;
    t->muted = 0x55dd55UL;
    t->ok = 0x0044ddUL;
    t->fail = 0xee9900UL;
}

#define PAL_BODY 34
#define PAL_ENTRY(i) (PAL_BODY + 144 + 8 * (i))

static void test_desk_palette(void)
{
    static unsigned char gold[BUF_MAX], out[BUF_MAX], out4[BUF_MAX];
    unsigned char pens[DK_SLOTS];
    gtheme t, k;
    long gn = slurp(DESK_DATA "palette-gui-save.prefs", gold, sizeof gold), n, i;
    int s, r, diff;
    probe_theme(&t);
    CHECK_INT(gn, DK_PALETTE_SIZE);
    n = desk_palette(&t, 8, out, sizeof out);
    CHECK_INT(n, DK_PALETTE_SIZE);
    /* byte-exact to the golden outside the pen lists (the mapping differs from the stock pens) and entry 0 */
    diff = 0;
    for (i = 0; i < n && i < gn; i++) {
        if (i >= PAL_BODY + 16 && i < PAL_BODY + 144)
            continue;
        if (i >= PAL_ENTRY(0) + 2 && i < PAL_ENTRY(0) + 8)
            continue;
        if (out[i] != gold[i])
            diff++;
    }
    CHECK_INT(diff, 0);
    /* the pen lists: R1.19 mapping in both, terminated */
    desk_pens(pens);
    for (s = 0; s < DK_SLOTS; s++) {
        CHECK_INT((out[PAL_BODY + 16 + 2 * s] << 8) | out[PAL_BODY + 17 + 2 * s], pens[s]);
        CHECK_INT((out[PAL_BODY + 80 + 2 * s] << 8) | out[PAL_BODY + 81 + 2 * s], pens[s]);
    }
    CHECK_INT(out[PAL_BODY + 16 + 2 * DK_SLOTS], 0xff);
    CHECK_INT(pens[DK_FILL], DK_ACCENT);
    CHECK_INT(pens[DK_FILLTEXT], DK_BG);
    CHECK_INT(pens[DK_SHINE], DK_FG);
    CHECK_INT(pens[DK_SHADOW], DK_DARKER);
    CHECK_INT(pens[DK_BARBLOCK], DK_BG);
    CHECK_INT(pens[DK_BARDETAIL], DK_FG);
    CHECK_INT(pens[DK_TEXT], DK_FG);
    CHECK_INT(pens[DK_HIGHLIGHTTEXT], DK_FG);
    /* knob: changing the colour of the pen a slot uses moves exactly that colour entry */
    for (s = 0; s < DK_SLOTS; s++) {
        r = pens[s];
        k = t;
        switch (r) {
        case DK_BG: k.bg ^= 0x080808UL; break;
        case DK_DARKER: k.darker_bg = 0x010101UL; break;
        case DK_FG: k.fg ^= 0x101010UL; break;
        case DK_ACCENT: k.accent ^= 0x101010UL; break;
        case DK_SELECTION: k.selection ^= 0x101010UL; break;
        case DK_MUTED: k.muted ^= 0x101010UL; break;
        case DK_GREEN: k.ok ^= 0x101010UL; break;
        default: k.fail ^= 0x101010UL; break;
        }
        desk_palette(&k, 8, out4, sizeof out4);
        diff = 0;
        for (i = 0; i < DK_PALETTE_SIZE; i++)
            if (out4[i] != out[i]) {
                diff++;
                CHECK(i >= PAL_ENTRY(r) && i < PAL_ENTRY(r) + 8);
            }
        CHECK(diff > 0);
    }
    /* 4 colours: the same four entries, the terminator after them, nothing behind */
    n = desk_palette(&t, 4, out4, sizeof out4);
    CHECK_INT(n, DK_PALETTE_SIZE);
    CHECK(!memcmp(out4 + PAL_ENTRY(0), out + PAL_ENTRY(0), 32));
    CHECK_INT(out4[PAL_ENTRY(4)], 0xff);
    CHECK_INT(out4[PAL_ENTRY(4) + 1], 0xff);
    CHECK_INT(out4[PAL_ENTRY(5)], 0);
    CHECK_INT(desk_palette(&t, 5, out4, sizeof out4), 0);
    CHECK_INT(desk_palette(&t, 8, out4, 100), 0);
}

static void test_desk_darker(void)
{
    gtheme all[SHIPPED_MAX], t;
    int n = load_all(all, SHIPPED_MAX), i;
    CHECK_INT(n, 23);
    for (i = 0; i < n; i++) {
        uc_u32 d = desk_darker(&all[i]);
        if (desk_luma(all[i].bg) == 0) {
            CHECK_INT(d, all[i].bg); /* black has nothing darker: exempt from the strict rule */
            continue;
        }
        CHECK(desk_luma(d) < desk_luma(all[i].bg));
        CHECK(desk_contrast(d, all[i].bg) >= 108);
        CHECK(desk_contrast(d, all[i].fg) >= 220);
    }
    /* a key that is not darker is replaced by the computed 80 % tone */
    gt_builtin(&t);
    t.bg = 0x202020UL;
    t.darker_bg = 0x303030UL;
    CHECK_INT(desk_darker(&t), gt_scale(0x202020UL, 80));
    t.darker_bg = 0x101010UL;
    CHECK_INT(desk_darker(&t), 0x101010UL);
}

static const unsigned char *ptrn(const unsigned char *f, int which)
{
    return f + 26 + 128 * which;
}

static void test_desk_wbpattern(void)
{
    static unsigned char out[BUF_MAX], prev[BUF_MAX], again[BUF_MAX];
    gtheme t;
    long n;
    int w;
    probe_theme(&t);
    n = desk_wbpattern(&t, 3, 0, 0, 0, out, sizeof out);
    CHECK_INT(n, 410);
    CHECK(!memcmp(out, "FORM", 4) && !memcmp(out + 8, "PREF", 4) && !memcmp(out + 12, "PRHD", 4));
    CHECK_INT(out[7], 402 & 0xff);
    for (w = 0; w < 3; w++) {
        const unsigned char *c = ptrn(out, w);
        CHECK(!memcmp(c, "PTRN", 4));
        CHECK_INT(c[7], 120);
        CHECK_INT(c[8 + 17], w);       /* Which */
        CHECK_INT(c[8 + 19], 1);       /* WBPF_PATTERN */
        CHECK_INT(c[8 + 20], 0);       /* Revision */
        CHECK_INT(c[8 + 23], 96);      /* DataLength */
    }
    CHECK_INT(ptrn(out, 0)[8 + 21], 3);
    /* ROOT solid: pen 1 = plane 0 all set; DRAWER: pen 0 = all clear, so it differs from the root (OD7) */
    CHECK_INT(ptrn(out, 0)[8 + 24], 0xff);
    CHECK_INT(ptrn(out, 0)[8 + 24 + 32], 0);
    CHECK_INT(ptrn(out, 1)[8 + 24], 0);
    CHECK(memcmp(ptrn(out, 0) + 8 + 24, ptrn(out, 1) + 8 + 24, 96));
    /* SCREEN: the empty chunk Wbpattern writes for none */
    CHECK_INT(ptrn(out, 2)[8 + 21], 0);
    CHECK_INT(ptrn(out, 2)[8 + 24], 0);
    /* tiled: the dot grid of pen 0 on the root, pen 1 elsewhere */
    n = desk_wbpattern(&t, 3, 1, 0, 0, again, sizeof again);
    CHECK_INT(n, 410);
    CHECK_INT(ptrn(again, 0)[8 + 24], 0x7f);       /* row 0: pixel 0 is pen 0 */
    CHECK_INT(ptrn(again, 0)[8 + 24 + 1], 0x7f);   /* pixel 8 */
    CHECK_INT(ptrn(again, 0)[8 + 24 + 2], 0xff);   /* row 1 solid */
    CHECK(!memcmp(ptrn(again, 1), ptrn(out, 1), 128));
    /* a 4-colour screen: depth 2 in the chunk, no pixel of a third plane */
    n = desk_wbpattern(&t, 2, 1, 0, 0, again, sizeof again);
    CHECK_INT(n, 410);
    CHECK(ptrn(again, 0)[8 + 21] <= 2);
    CHECK_INT(ptrn(again, 0)[8 + 24 + 64], 0);
    /* the user's SCREEN chunk survives */
    memcpy(prev, out, 410);
    prev[26 + 256 + 8 + 21] = 3;
    prev[26 + 256 + 8 + 24 + 5] = 0xa5;
    desk_wbpattern(&t, 3, 0, prev, 410, again, sizeof again);
    CHECK_INT(ptrn(again, 2)[8 + 24 + 5], 0xa5);
    CHECK_INT(ptrn(again, 2)[8 + 21], 3);
    CHECK_INT(desk_wbpattern(&t, 3, 0, 0, 0, again, 100), 0);
}

static void test_desk_pointer(void)
{
    static unsigned char gold[BUF_MAX * 2], out[BUF_MAX * 2];
    gtheme t;
    long n = slurp(DESK_DATA "pointer-os322.prefs", gold, sizeof gold), chunks, i, diff = 0, bitmap = 0;
    probe_theme(&t);
    CHECK_INT(n, 6012);
    memcpy(out, gold, (size_t)n);
    chunks = desk_pointer(&t, out, n);
    CHECK_INT(chunks, 41);
    for (i = 0; i < n; i++)
        if (out[i] != gold[i]) {
            diff++;
            /* only inside the RGB table: body offset 32..40 of a 146-byte chunk step starting at 26 */
            if ((i - 26) % 146 < 8 + 32 || (i - 26) % 146 >= 8 + 41)
                bitmap++;
        }
    CHECK(diff > 0 && diff <= chunks * 9);
    CHECK_INT(bitmap, 0);
    CHECK_INT(out[26 + 8 + 32], 0xff);      /* triple 1: foreground */
    CHECK_INT(out[26 + 8 + 35], 0x13);      /* triple 2: background */
    CHECK_INT(out[26 + 8 + 38], 0x66);      /* triple 3: accent */
    CHECK_INT(desk_pointer(&t, out, 20), -1);
    out[0] = 'X';
    CHECK_INT(desk_pointer(&t, out, n), -1);
}

static int font_known(void *ctx, const char *name)
{
    return !strcmp(name, (const char *)ctx);
}

static void test_desk_font_default(void)
{
    static unsigned char d[BUF_MAX];
    CHECK_INT(desk_font_default(d, sizeof d), 518);
    CHECK_INT(desk_font_default(d, 100), 0);
    CHECK_INT(desk_font_pens(d, 518), 1); /* a valid font.prefs with one WBFONT chunk */
    CHECK_STR((const char *)d + 26 + 8 + 28, "topaz.font");
    CHECK_INT(d[26 + 8 + 15], 0);
    CHECK_INT(d[26 + 164 + 8 + 15], 1);
    CHECK_INT(d[26 + 328 + 8 + 15], 2);
}

static void test_desk_font(void)
{
    static unsigned char gold[BUF_MAX], out[BUF_MAX];
    gtheme t;
    long n = slurp(DESK_DATA "font-wbfont-outline.prefs", gold, sizeof gold), i;
    int k, wb = -1;
    probe_theme(&t);
    CHECK_INT(n, 518);
    for (k = 0; k < 3; k++)
        if (gold[26 + 8 + k * 164 + 15] == 0)
            wb = 26 + 8 + k * 164;
    CHECK(wb > 0);
    memcpy(out, gold, (size_t)n);
    CHECK_INT(desk_font_pens(out, n), 1);
    CHECK_INT(out[wb + 16], DK_FG);
    CHECK_INT(out[wb + 17], DK_DARKER);
    CHECK_INT(out[wb + 18], 0);
    CHECK_INT(out[wb + 19], 1);
    CHECK(out[wb + 16] != out[wb + 17]); /* label pen != backdrop pen */
    for (i = 0; i < n; i++)
        if (out[i] != gold[i])
            CHECK(i >= wb + 16 && i <= wb + 19);
    CHECK_INT(desk_font_pens(out, 10), -1);
    /* font=: an existing font is written, an unknown one leaves the file untouched */
    memcpy(out, gold, (size_t)n);
    strcpy(t.font, "topaz.font");
    CHECK_INT(desk_font_name(&t, out, n, font_known, (void *)"nothing.font"), 0);
    CHECK(!memcmp(out, gold, (size_t)n));
    CHECK_INT(desk_font_name(&t, out, n, font_known, (void *)"topaz.font"), 1);
    CHECK_STR((const char *)out + wb + 28, "topaz.font");
    CHECK_INT(out[wb + 24 + 1], gold[wb + 24 + 1]); /* the size stays */
    memcpy(out, gold, (size_t)n);
    t.font[0] = 0;
    CHECK_INT(desk_font_name(&t, out, n, font_known, (void *)""), 0);
    CHECK(!memcmp(out, gold, (size_t)n));
}

static void test_desk_guard(void)
{
    gtheme all[SHIPPED_MAX], t;
    char why[120];
    int n = load_all(all, SHIPPED_MAX), i;
    CHECK_INT(n, 23);
    for (i = 0; i < n; i++) {
        CHECK_INT(desk_check(&all[i], why, sizeof why), 0);
        CHECK_STR(why, "");
    }
    /* foreground == background: refused with a message */
    t = all[0];
    t.fg = t.bg;
    CHECK_INT(desk_check(&t, why, sizeof why), 1);
    CHECK(why[0] != 0);
    /* title text on the title bar */
    t = all[0];
    t.accent = t.bg;
    CHECK_INT(desk_check(&t, why, sizeof why), 1);
    /* the labels against the backdrop */
    t = all[0];
    t.bg = 0x808080UL;
    t.darker_bg = 0x707070UL;
    t.fg = 0xc0c0c0UL;
    CHECK_INT(desk_check(&t, why, 0), 1);
}

/* R1.6: desk_plan is the apply decision of desk_dos_apply (a passing theme must not be refused) */
static void test_desk_plan(void)
{
    gtheme all[SHIPPED_MAX], t;
    char why[120];
    int n = load_all(all, SHIPPED_MAX);
    CHECK(n > 0);
    CHECK_INT(desk_plan(15, &all[0], why, sizeof why), 15);
    CHECK_INT(desk_plan(5, &all[0], why, sizeof why), 5);
    CHECK_INT(desk_plan(0, &all[0], why, sizeof why), 0);
    t = all[0];
    t.fg = t.bg;
    CHECK_INT(desk_plan(15, &t, why, sizeof why), -1);
    CHECK(why[0] != 0);
    CHECK_INT(desk_plan(0, &t, why, sizeof why), 0);
}

static void test_desk_targets(void)
{
    const char *list[16], *marker;
    char buf[200];
    unsigned mask;
    int n, i;
    long ml;
    CHECK_INT(DT_COUNT, 4);
    for (i = 0; i < DT_COUNT; i++)
        CHECK_INT(desk_target_by_name(desk_target_get(i)->name), i);
    CHECK_INT(desk_target_by_name("nothing"), -1);
    /* a switched-off target writes nothing and is absent */
    CHECK_INT(desk_snapshot(0, list, 16), 0);
    n = desk_snapshot(1u << DT_POINTER, list, 16);
    CHECK_INT(n, 2);
    CHECK_STR(list[0], "ENVARC:Sys/pointer.prefs");
    CHECK_STR(list[1], "ENV:Sys/pointer.prefs");
    n = desk_snapshot((1u << DT_PALETTE) | (1u << DT_FONT), list, 16);
    CHECK_INT(n, 4);
    for (i = 0; i < n; i++)
        CHECK(strstr(list[i], "pointer") == 0 && strstr(list[i], "wbpattern") == 0);
    CHECK_INT(desk_snapshot((1u << DT_COUNT) - 1, list, 16), 8);
    CHECK(strstr(list[0], "palette") != 0);
    CHECK_INT(desk_snapshot((1u << DT_COUNT) - 1, list, 3), 3);
    /* restore: the reverse order */
    n = desk_restore((1u << DT_PALETTE) | (1u << DT_FONT), list, 16);
    CHECK_INT(n, 4);
    CHECK(strstr(list[0], "font") != 0 && strstr(list[3], "palette") != 0);
    /* the pending marker names its targets; a crashed run restores them whatever the switches say now */
    mask = (1u << DT_POINTER) | (1u << DT_FONT);
    ml = desk_marker_make(mask, buf, sizeof buf);
    CHECK(ml > 0);
    CHECK_INT(desk_marker_parse(buf, ml), mask);
    n = desk_restore(desk_marker_parse(buf, ml), list, 16);
    marker = list[0];
    CHECK_INT(n, 4);
    CHECK(strstr(marker, "font") != 0 && strstr(list[2], "pointer") != 0);
    CHECK_INT(desk_marker_parse("garbage", 7), 0);
    CHECK_INT(desk_marker_parse("desk-pending 1\nfont\nbogus\n", 25), 1u << DT_FONT);
    CHECK_INT(desk_marker_make(mask, buf, 10), 0);
}

void suite_desk(void)
{
    test_desk_palette();
    test_desk_darker();
    test_desk_wbpattern();
    test_desk_pointer();
    test_desk_font();
    test_desk_font_default();
    test_desk_guard();
    test_desk_plan();
    test_desk_targets();
}
