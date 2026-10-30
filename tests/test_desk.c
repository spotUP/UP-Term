/* The pure core of the Amarchy native desktop theme (amarchy/desk_core.c; plan 2026-10-29-amarchy-p1a-native-theme.md
 * items R1.2-R1.6, R1.16a, R1.18a-R1.22a, D-TGT). Goldens: tests/data/desk/ (probe note 2026-10-29-amarchy-p1a-prefs-probe.md).
 * The product entry point (desk.c, the menu rows) comes with the OS half; these tests drive the core directly. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/amconf_core.h"
#include "../amarchy/desk_core.h"
#include "../handler/be32.h"
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
    desk_pens(pens);
    CHECK_INT(gn, DK_PALETTE_SIZE);
    n = desk_palette(&t, 8, pens, out, sizeof out);
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
        desk_palette(&k, 8, pens, out4, sizeof out4);
        diff = 0;
        for (i = 0; i < DK_PALETTE_SIZE; i++)
            if (out4[i] != out[i]) {
                diff++;
                CHECK(i >= PAL_ENTRY(r) && i < PAL_ENTRY(r) + 8);
            }
        CHECK(diff > 0);
    }
    /* 4 colours: the same four entries, the terminator after them, nothing behind */
    n = desk_palette(&t, 4, pens, out4, sizeof out4);
    CHECK_INT(n, DK_PALETTE_SIZE);
    CHECK(!memcmp(out4 + PAL_ENTRY(0), out + PAL_ENTRY(0), 32));
    CHECK_INT(out4[PAL_ENTRY(4)], 0xff);
    CHECK_INT(out4[PAL_ENTRY(4) + 1], 0xff);
    CHECK_INT(out4[PAL_ENTRY(5)], 0);
    CHECK_INT(desk_palette(&t, 5, pens, out4, sizeof out4), 0);
    CHECK_INT(desk_palette(&t, 8, pens, out4, 100), 0);
}

/* FLAT-P1-1: the flat pen map differs from the stock one in the shine slot only (shine = background),
 * every slot reaches the PALT pen bytes, and the output equals the goldens made from the two maps */
static void test_desk_palette_flat(void)
{
    static unsigned char gold[BUF_MAX], out[BUF_MAX];
    unsigned char stock[DK_SLOTS], flat[DK_SLOTS];
    gtheme t;
    long n;
    int s, pen;
    static const char *const names[] = {"palette-p1a-pen8.prefs", "palette-p1a-pen4.prefs",
                                        "palette-flat-pen8.prefs", "palette-flat-pen4.prefs"};
    int k;
    probe_theme(&t);
    desk_pens(stock);
    desk_pens_flat(flat);
    for (s = 0; s < DK_SLOTS; s++) {
        CHECK(flat[s] < 4);
        if (s == DK_SHINE) {
            CHECK_INT(flat[s], DK_BG);
            CHECK(stock[s] != flat[s]);
        } else
            CHECK_INT(flat[s], stock[s]);
    }
    for (k = 0; k < 4; k++) {
        char path[256];
        const unsigned char *pm = k < 2 ? stock : flat;
        snprintf(path, sizeof path, DESK_DATA "%s", names[k]);
        CHECK_INT(slurp(path, gold, sizeof gold), DK_PALETTE_SIZE);
        n = desk_palette(&t, k % 2 ? 4 : 8, pm, out, sizeof out);
        CHECK_INT(n, DK_PALETTE_SIZE);
        CHECK(!memcmp(out, gold, DK_PALETTE_SIZE));
        for (s = 0; s < DK_SLOTS; s++) {
            pen = (out[PAL_BODY + 16 + 2 * s] << 8) | out[PAL_BODY + 17 + 2 * s];
            CHECK_INT(pen, pm[s]);
            pen = (out[PAL_BODY + 80 + 2 * s] << 8) | out[PAL_BODY + 81 + 2 * s];
            CHECK_INT(pen, pm[s]);
        }
    }
}


/* FLAT-P1-3: the palette.prefs the desk apply writes follows amarchy-flat-pens. desk_dos_apply builds it as
 * desk_pens_for(amconf_desk_flat_pens(prefs)) -> desk_palette (the same two calls here); the flat key moves only the
 * shine slot, the pen-reset verdict follows the chosen map, and the key alone writes nothing without the palette target. */
static void test_desk_flat_pens_key(void)
{
    static unsigned char gold[BUF_MAX], out[BUF_MAX];
    static const char flat_on[] = "[profile default]\ndesktop-theme = on\namarchy-flat-pens = on\n";
    static const char flat_off[] = "[profile default]\ndesktop-theme = on\n";
    static const char flat_nopal[] = "[profile default]\ndesktop-theme = on\ndesktop-palette = off\namarchy-flat-pens = on\n";
    static const char flat_nodesk[] = "[profile default]\namarchy-flat-pens = on\n";
    static const char *const names[] = {"palette-flat-pen8.prefs", "palette-p1a-pen8.prefs"};
    const char *const txt[] = {flat_on, flat_off};
    gtheme t;
    int k;
    probe_theme(&t);
    for (k = 0; k < 2; k++) {
        am_prefs p;
        unsigned char pens[DK_SLOTS], want[DK_SLOTS];
        unsigned short live[DK_SLOTS];
        char path[256];
        int s;
        amconf_parse(txt[k], (long)strlen(txt[k]), &p);
        CHECK_INT(amconf_desk_flat_pens(&p), k == 0);
        CHECK(amconf_desk_mask(&p) & (1u << DT_PALETTE));
        desk_pens_for(amconf_desk_flat_pens(&p), pens);
        snprintf(path, sizeof path, DESK_DATA "%s", names[k]);
        CHECK_INT(slurp(path, gold, sizeof gold), DK_PALETTE_SIZE);
        CHECK_INT(desk_palette(&t, 8, pens, out, sizeof out), DK_PALETTE_SIZE);
        CHECK(!memcmp(out, gold, DK_PALETTE_SIZE));
        if (k == 0)
            desk_pens_flat(want);
        else
            desk_pens(want);
        CHECK(!memcmp(pens, want, DK_SLOTS));
        /* the reset verdict: a live screen holding this map needs no reset, the other map does */
        for (s = 0; s < DK_SLOTS; s++)
            live[s] = want[s];
        CHECK_INT(desk_pens_reset(live, DK_SLOTS, k == 0), 0);
        CHECK_INT(desk_pens_reset(live, DK_SLOTS, k != 0), 1);
    }
    {
        am_prefs p;
        amconf_parse(flat_nopal, (long)strlen(flat_nopal), &p);
        CHECK_INT(amconf_desk_flat_pens(&p), 0);              /* palette target off: no palette write, no flat map */
        CHECK(!(amconf_desk_mask(&p) & (1u << DT_PALETTE)));
        amconf_parse(flat_nodesk, (long)strlen(flat_nodesk), &p);
        CHECK_INT(amconf_desk_flat_pens(&p), 0);              /* master switch off: nothing is written */
        CHECK_INT(amconf_desk_mask(&p), 0u);
    }
}

/* R1.17: IPrefs resets the Workbench screen only when the live DrawInfo pens differ from the ones the Palette USE brings */
/* DESK-31: the 3.2 reset closes the screen for ~200 ms (rig 7 probe); a screen of 0 there made the apply reopen its windows mid-reset */
/* DESK-31 (3.1, rig 7 Run 10): the windows came back on the first poll whose screen title matched the idle one, which was the
 * fresh screen of IPrefs' first Font reset; its second reset then raised "System Request" (Retry|Cancel). The signal is IPrefs
 * itself: back in its idle Wait() with no notification pending. */
static void test_desk_iprefs_idle(void)
{
    unsigned long idle = 0x00010000UL | 0x1000UL; /* notify port signal | SIGBREAKF_CTRL_C */
    CHECK_INT(desk_iprefs_idle(0, 0, 0, 0, 0), 1);                 /* no IPrefs: nothing to wait for */
    CHECK_INT(desk_iprefs_idle(1, 1, idle, 0, idle), 1);           /* idle Wait, nothing pending */
    CHECK_INT(desk_iprefs_idle(1, 1, idle, 0x00010000UL, idle), 0); /* the editor's notification is pending: not begun yet */
    CHECK_INT(desk_iprefs_idle(1, 0, idle, 0, idle), 0);           /* running (ready): mid-reset */
    CHECK_INT(desk_iprefs_idle(1, 1, 0x100UL, 0, idle), 0);         /* waiting on SIGF_DOS (Delay, packet) inside the reset */
    CHECK_INT(desk_iprefs_idle(1, 1, 0x80000000UL, 0, idle), 0);    /* waiting on its requester's IDCMP port */
    CHECK_INT(desk_iprefs_idle(1, 1, 0, 0, 0), 0);                 /* no idle mask captured: never call it idle */
    /* Run 10 timeline, one sample per phase: editor exited (signal pending), reset 1 (running), between resets (DOS wait),
     * reset 2 (running), done. The windows may return only at the last sample. */
    {
        unsigned long w[5] = {0, 0, 0x100UL, 0, 0}, r[5] = {0x00010000UL, 0, 0, 0, 0};
        int run[5] = {0, 1, 0, 1, 0}, i, first = -1;
        for (i = 0; i < 5 && first < 0; i++)
            if (desk_iprefs_idle(1, !run[i], run[i] ? 0 : (w[i] ? w[i] : idle), r[i], idle))
                first = i;
        CHECK_INT(first, 4);
    }
}

static void test_desk_reset_done(void)
{
    unsigned char want[DK_SLOTS];
    unsigned short same[DK_SLOTS], other[DK_SLOTS];
    int i;
    char a, b;
    desk_pens(want);
    for (i = 0; i < DK_SLOTS; i++) {
        same[i] = want[i];
        other[i] = (unsigned short)(want[i] + 1);
    }
    CHECK_INT(desk_reset_done(&a, 0, 0, 0, 0), 0);             /* screen closed mid-reset */
    CHECK_INT(desk_reset_done(&a, 0, other, DK_SLOTS, 0), 0);
    CHECK_INT(desk_reset_done(&a, &a, other, DK_SLOTS, 0), 0); /* same screen, old pens: not yet */
    CHECK_INT(desk_reset_done(&a, &a, same, DK_SLOTS, 0), 1);  /* in place, desk pens */
    CHECK_INT(desk_reset_done(&a, &b, other, DK_SLOTS, 0), 1); /* a new screen (3.1) */
}

static void test_desk_pens_reset(void)
{
    unsigned char want[DK_SLOTS];
    unsigned short cur[DK_SLOTS + 2];
    int i;
    desk_pens(want);
    for (i = 0; i < DK_SLOTS; i++)
        cur[i] = want[i];
    CHECK_INT(desk_pens_reset(cur, DK_SLOTS, 0), 0);   /* same pens: colours load live, no reset */
    CHECK_INT(desk_pens_reset(cur, DK_SLOTS + 2, 0), 0);
    for (i = 0; i < DK_SLOTS; i++) {                /* knob: every slot moves the verdict */
        cur[i] ^= 1;
        CHECK_INT(desk_pens_reset(cur, DK_SLOTS, 0), 1);
        if (i > 0)
            CHECK_INT(desk_pens_reset(cur, i, 0), 0);  /* slots beyond dri_NumPens are not compared */
        cur[i] ^= 1;
    }
    CHECK_INT(desk_pens_reset(cur, 0, 0), 1);          /* no DrawInfo: unknown, assume a reset */
    CHECK_INT(desk_pens_reset(0, DK_SLOTS, 0), 1);
}

static void test_desk_darker(void)
{
    gtheme all[SHIPPED_MAX], t;
    int n = load_all(all, SHIPPED_MAX), i;
    CHECK_INT(n, 23);
    for (i = 0; i < n; i++) {
        uc_u32 d = desk_darker(&all[i]);
        if (desk_ground_black(&all[i])) {
            CHECK(desk_luma(d) > desk_luma(all[i].bg)); /* black has nothing darker: the backdrop lifts to grey */
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

/* DESK-BACKDROP-DARKER: the backdrop pen (DK_DARKER) never equals the shell background pen (DK_BG) on the palette the
 * Workbench screen really holds: read back from desk_palette at 4 and 8 colours, at 24 bit (AGA) and at 12 bit (OCS, a
 * nibble per channel). Darker for every bg, except a bg that is black on the screen: the shell keeps it, the backdrop is a
 * dark grey that is lighter. */
static uc_u32 pal_pen(const unsigned char *f, int pen, int bits)
{
    const unsigned char *e = f + 26 + 8 + 144 + 8 * pen;
    uc_u32 c = 0;
    int i;
    for (i = 0; i < 3; i++) {
        unsigned v = e[2 + 2 * i]; /* high byte of the 16-bit channel */
        if (bits == 4)
            v = (v >> 4) * 17u;
        c = (c << 8) | v;
    }
    return c;
}

static void test_desk_backdrop(void)
{
    gtheme all[SHIPPED_MAX];
    int n = load_all(all, SHIPPED_MAX), i, nc, bits;
    for (i = 0; i < n; i++)
        for (nc = 4; nc <= 8; nc += 4)
            for (bits = 4; bits <= 8; bits += 4) {
                unsigned char f[DK_PALETTE_SIZE], pens[DK_SLOTS];
                uc_u32 bg, dk;
                int blk = (((all[i].bg >> 20) & 15) | ((all[i].bg >> 12) & 15) | ((all[i].bg >> 4) & 15)) == 0;
                desk_pens(pens);
                CHECK_INT(desk_palette(&all[i], nc, pens, f, sizeof f), DK_PALETTE_SIZE);
                bg = pal_pen(f, DK_BG, bits);
                dk = pal_pen(f, DK_DARKER, bits);
                if (dk == bg || (!blk && desk_luma(dk) >= desk_luma(bg)) || (blk && desk_luma(dk) <= desk_luma(bg)))
                    printf("backdrop theme %s colours %d bits %d: bg %06lx backdrop %06lx\n", all[i].name, nc,
                           bits * 3, (unsigned long)bg, (unsigned long)dk);
                CHECK(dk != bg);
                if (!blk)
                    CHECK(desk_luma(dk) < desk_luma(bg));
                else
                    CHECK(desk_luma(dk) > desk_luma(bg));
            }
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
    n = desk_wbpattern(&t, 3, 0, 0, out, sizeof out);
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
    /* a 4-colour screen: depth 2 in the chunk, no pixel of a third plane */
    n = desk_wbpattern(&t, 2, 0, 0, again, sizeof again);
    CHECK_INT(n, 410);
    CHECK(ptrn(again, 0)[8 + 21] <= 2);
    CHECK_INT(ptrn(again, 0)[8 + 24 + 64], 0);
    /* the user's SCREEN chunk survives */
    memcpy(prev, out, 410);
    prev[26 + 256 + 8 + 21] = 3;
    prev[26 + 256 + 8 + 24 + 5] = 0xa5;
    desk_wbpattern(&t, 3, prev, 410, again, sizeof again);
    CHECK_INT(ptrn(again, 2)[8 + 24 + 5], 0xa5);
    CHECK_INT(ptrn(again, 2)[8 + 21], 3);
    CHECK_INT(desk_wbpattern(&t, 3, 0, 0, again, 100), 0);
}

static void test_desk_wbpattern_no_dots(void)
{
    /* the Workbench backdrop has no dots: every ROOT pattern pixel is DK_DARKER at every depth */
    static unsigned char out[BUF_MAX];
    gtheme t;
    int depth, p, y, x;
    probe_theme(&t);
    for (depth = 1; depth <= 3; depth++) {
        const unsigned char *data;
        CHECK_INT(desk_wbpattern(&t, depth, 0, 0, out, sizeof out), 410);
        data = ptrn(out, 0) + 8 + 24;
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++) {
                int pen = 0;
                for (p = 0; p < depth; p++)
                    if (data[p * 32 + y * 2 + x / 8] & (0x80 >> (x % 8)))
                        pen |= 1 << p;
                CHECK_INT(pen, DK_DARKER);
            }
    }
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

/* FLAT-P1-2: the icontrol.prefs writer on a real capture (tests/data/desk/icontrol-os32.prefs: ENVARC:Sys/icontrol.prefs saved once by SYS:Prefs/IControl 47.x on a stock --os 32 rig, 72 bytes; stock 3.2 already writes ICB_VERSIONED and version 2, geometry all zero) */
static void test_desk_icontrol(void)
{
    static unsigned char gold[BUF_MAX], out[BUF_MAX], out2[BUF_MAX], old[BUF_MAX], iexc[BUF_MAX];
    unsigned char geom[4], thin[4], stock[4];
    long gn = slurp(DESK_DATA "icontrol-os32.prefs", gold, sizeof gold), n, n2, i, diff = 0, ictl = 0, p;
    desk_geometry(0, stock);
    desk_geometry(1, thin);
    CHECK(gn >= 26 + 8 + 38);
    CHECK(memcmp(stock, thin, 4) != 0);
    for (p = 12; p + 8 <= gn; p += 8 + (long)be32_get(gold + p + 4) + (long)(be32_get(gold + p + 4) & 1))
        if (!memcmp(gold + p, "ICTL", 4))
            ictl = p + 8;
    CHECK(ictl > 0);
    /* the captured file is the baseline: thin changes the 4 geometry bytes (the real file has the version fields already; the pre-V2 case below covers the upgrade) */
    n = desk_icontrol(gold, gn, thin, out, sizeof out);
    CHECK_INT(n, gn);
    for (i = 0; i < n; i++)
        if (out[i] != gold[i]) {
            diff++;
            CHECK(i >= ictl + 20 && i < ictl + 38);
        }
    CHECK(diff > 0);
    CHECK_INT(out[ictl + 20] & 0x80, 0x80);
    CHECK_INT(out[ictl + 29], 2);
    CHECK_INT(out[ictl + 30] | out[ictl + 31], 0);
    CHECK(!memcmp(out + ictl + 34, thin, 4));
    CHECK_INT(be32_get(out + 4) + 8, (unsigned long)n);
    /* writing it again, or stock over it, changes only the geometry bytes; stock over the baseline is the baseline plus the version fields */
    n2 = desk_icontrol(out, n, thin, out2, sizeof out2);
    CHECK(n2 == n && !memcmp(out, out2, (size_t)n));
    n2 = desk_icontrol(out, n, stock, out2, sizeof out2);
    CHECK_INT(n2, n);
    for (i = 0; i < n; i++)
        if (out[i] != out2[i])
            CHECK(i >= ictl + 34 && i < ictl + 38);
    /* a pre-V2 file (ICTL of 28 bytes, no versioned tail) is upgraded and keeps every field it had */
    memcpy(old, gold, 26);
    memcpy(old + 26, "ICTL", 4);
    old[30] = old[31] = old[32] = 0;
    old[33] = 28;
    memcpy(old + 34, gold + ictl, 28);
    old[34 + 20] &= 0x7f;
    be32_put(old + 4, 4 + 14 + 8 + 28);
    n = desk_icontrol(old, 34 + 28, thin, out, sizeof out);
    CHECK_INT(n, 34 + 38);
    CHECK_INT(be32_get(out + 30), 38);
    CHECK(!memcmp(out + 34, old + 34, 20));
    CHECK_INT(be32_get(out + 34 + 20), be32_get(old + 34 + 20) | 0x80000000UL);
    CHECK_INT(out[34 + 29], 2);
    CHECK(!memcmp(out + 34 + 24, old + 34 + 24, 4));
    CHECK(!memcmp(out + 34 + 34, thin, 4));
    CHECK_INT(be32_get(out + 4) + 8, (unsigned long)n);
    /* an IEXC chunk (odd size, so a pad byte) before and after the ICTL chunk goes through byte for byte */
    memset(iexc, 0, sizeof iexc);
    p = 0;
    memcpy(iexc, gold, 26);
    p = 26;
    memcpy(iexc + p, "IEXC", 4); be32_put(iexc + p + 4, 5); memcpy(iexc + p + 8, "Dopus", 5); iexc[p + 13] = 0; p += 14;
    memcpy(iexc + p, gold + 26, (size_t)(gn - 26)); p += gn - 26;
    memcpy(iexc + p, "IEXC", 4); be32_put(iexc + p + 4, 3); memcpy(iexc + p + 8, "*ab", 3); iexc[p + 11] = 0; p += 12;
    be32_put(iexc + 4, (unsigned long)(p - 8));
    n = desk_icontrol(iexc, p, thin, out, sizeof out);
    CHECK_INT(n, p);
    CHECK(!memcmp(out + 26, iexc + 26, 14));
    CHECK(!memcmp(out + n - 12, iexc + p - 12, 12));
    /* no file: a new one with defaults and the geometry */
    n = desk_icontrol(0, 0, thin, out, sizeof out);
    CHECK_INT(n, 26 + 8 + 38);
    CHECK(!memcmp(out + 26, "ICTL", 4) && !memcmp(out + 26 + 8 + 34, thin, 4));
    geom[0] = 0;
    CHECK_INT(desk_icontrol(0, 0, thin, out, 20), 0);
    CHECK_INT(desk_icontrol((const unsigned char *)"not a prefs file at all, no", 27, thin, out, sizeof out), 0);
    (void)geom;
    /* decisions: thin writes once (snapshot first), stock after thin restores the snapshot, stock on a stock file does nothing */
    {
        int snap = -1;
        n = desk_icontrol(gold, gn, thin, out, sizeof out);
        CHECK_INT(desk_icontrol_decide(1, gold, gn, 0, &snap), DK_IC_WRITE);
        CHECK_INT(snap, 1);
        CHECK_INT(desk_icontrol_decide(1, 0, 0, 0, &snap), DK_IC_WRITE);
        CHECK_INT(desk_icontrol_decide(1, out, n, 1, &snap), DK_IC_NONE);
        CHECK_INT(snap, 0);
        CHECK_INT(desk_icontrol_decide(0, out, n, 1, &snap), DK_IC_RESTORE);
        CHECK_INT(snap, 0);
        CHECK_INT(desk_icontrol_decide(0, out, n, 0, &snap), DK_IC_NONE);
        CHECK_INT(desk_icontrol_decide(0, gold, gn, 1, &snap), DK_IC_NONE);
        CHECK_INT(desk_icontrol_decide(0, 0, 0, 1, &snap), DK_IC_NONE);
    }
}

static void test_desk_targets(void)
{
    const char *list[16], *marker;
    char buf[200];
    unsigned mask;
    int n, i;
    long ml;
    CHECK_INT(DT_COUNT, 5);
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
    CHECK_INT(desk_snapshot((1u << DT_COUNT) - 1, list, 16), 10);
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

/* R1.17: the 3.1 Pointer editor (40.x) reset-loops on the 3.2 prefs; only 47+ is fed them */
static void test_desk_pointer_gate(void)
{
    static const char v31[] = "xx\0\0$VER: Pointer 40.2 (4.11.93)\0yy";
    static const char v32[] = "$VER: pointer 47.26 (24.3.2021)";
    static const char none[] = "no version string here";
    CHECK_INT(desk_ver_major((const unsigned char *)v31, sizeof v31), 40);
    CHECK_INT(desk_ver_major((const unsigned char *)v32, sizeof v32 - 1), 47);
    CHECK_INT(desk_pointer_editor_ok((const unsigned char *)v31, sizeof v31), 0);
    CHECK_INT(desk_pointer_editor_ok((const unsigned char *)v32, sizeof v32 - 1), 1);
    CHECK_INT(desk_pointer_editor_ok((const unsigned char *)none, sizeof none), 0);
    CHECK_INT(desk_pointer_editor_ok((const unsigned char *)"$VER: P 47", 10), 1);
    CHECK_INT(desk_pointer_editor_ok(0, 0), 0);
}

/* DESK-31-CRASH: the 3.1 Palette editor (40.2) behind IPrefs 40.7 freezes the guest on any palette.prefs write; only 47+ runs */
static void test_desk_palette_gate(void)
{
    static const char v31[] = "xx\0\0$VER: Palette 40.2 (4.11.93)\0yy";
    static const char v32[] = "$VER: Palette 47.21 (24.3.2021)";
    CHECK_INT(desk_palette_editor_ok((const unsigned char *)v31, sizeof v31), 0);
    CHECK_INT(desk_palette_editor_ok((const unsigned char *)v32, sizeof v32 - 1), 1);
    CHECK_INT(desk_palette_editor_ok((const unsigned char *)"no version", 10), 0);
    CHECK_INT(desk_palette_editor_ok(0, 0), 0);
    {
        int ws = 9;
        /* FLAT-P1-2: the geometry is read on 3.2 only, and writing it reopens the Workbench screen */
        CHECK_INT(desk_gate_mask(1u << DT_ICONTROL, 0, &ws), 0);
        CHECK_INT(ws, 0);
        CHECK_INT(desk_gate_mask(1u << DT_ICONTROL, 1, &ws), 1u << DT_ICONTROL);
        CHECK_INT(ws, 1);
        /* 3.1: no palette write; the wbpattern write that remains makes Intuition ask for a Workbench reset (second cause) */
        CHECK_INT(desk_gate_mask(15, 0, &ws), 14);
        CHECK_INT(ws, 1);
        CHECK_INT(desk_gate_mask(1, 0, &ws), 0);
        CHECK_INT(ws, 0);
        CHECK_INT(desk_gate_mask(15, 1, &ws), 15);
        CHECK_INT(ws, 0); /* 3.2 measured: a WBPattern USE with a window open posts no reset requester */
        CHECK_INT(desk_gate_mask(1u << DT_PALETTE, 1, &ws), 1u << DT_PALETTE);
        CHECK_INT(ws, 0);
    }
}

/* DESK-PREVIEW-PALETTE: the picker's hook (gt_preview_step) drives the Workbench pens: every highlighted theme writes its desk_role
 * colours, Esc puts back the colours saved when the picker opened. dp_stub = 1 stubs the desk step out (the old behaviour). */
static uc_u32 dp_pen[8];
static int dp_stub, dp_writes;
static desk_pv dp_pv;
static gtheme dp_theme[SHIPPED_MAX];
static int dp_n;
static void dp_get(void *c, int pen, uc_u32 *rgb) { (void)c; *rgb = dp_pen[pen]; }
static void dp_set(void *c, int pen, uc_u32 rgb) { (void)c; dp_pen[pen] = rgb; dp_writes++; }
static const char *dp_cur;
static void dp_send(const char *name)
{
    int i;
    if (dp_stub)
        return;
    for (i = 0; i < dp_n; i++)
        if (!strcmp(dp_theme[i].name, name))
            desk_preview(&dp_pv, strcmp(name, dp_cur) ? &dp_theme[i] : 0);
}
static void test_desk_preview(void)
{
    char drawn[GT_NAME], row[GT_NAME + 8];
    int pass, i, k, shown = 0;
    dp_n = load_all(dp_theme, SHIPPED_MAX);
    CHECK(dp_n > 3);
    dp_cur = "tokyo-night";
    for (pass = 0; pass < 2; pass++) {
        dp_stub = pass;                       /* pass 0 real; pass 1 proves the checks fail without the desk step */
        for (i = 0; i < 8; i++)
            dp_pen[i] = 0x101010UL * (unsigned)(i + 1);   /* the Workbench's own colours when the picker opens */
        memset(&dp_pv, 0, sizeof dp_pv);
        dp_pv.npens = 8;
        dp_pv.get = dp_get;
        dp_pv.set = dp_set;
        strcpy(drawn, dp_cur);
        dp_writes = 0;
        for (k = 0; k < dp_n; k++) {          /* Down through every theme */
            int ok = 1;
            if (!strcmp(dp_theme[k].name, dp_cur) || desk_check(&dp_theme[k], 0, 0))
                continue;
            snprintf(row, sizeof row, "gtheme:%s", dp_theme[k].name);
            gt_preview_step(row, dp_cur, drawn, sizeof drawn, dp_send);
            for (i = 0; i < 8; i++)
                ok &= dp_pen[i] == desk_role(&dp_theme[k], i);
            if (!pass) {
                CHECK(ok);                    /* pens 0..7 are the highlighted theme's roles */
                shown++;
            } else
                CHECK(!ok);
        }
        gt_preview_step(0, dp_cur, drawn, sizeof drawn, dp_send);   /* Esc */
        for (i = 0; i < 8; i++)
            CHECK(dp_pen[i] == 0x101010UL * (unsigned)(i + 1) || pass);
        CHECK(pass ? dp_writes == 0 : dp_writes > 0);
    }
    CHECK(shown > 3);
}

void suite_desk(void)
{
    test_desk_palette();
    test_desk_palette_flat();
    test_desk_flat_pens_key();
    test_desk_pens_reset();
    test_desk_reset_done();
    test_desk_iprefs_idle();
    test_desk_darker();
    test_desk_backdrop();
    test_desk_wbpattern();
    test_desk_wbpattern_no_dots();
    test_desk_pointer();
    test_desk_pointer_gate();
    test_desk_palette_gate();
    test_desk_font();
    test_desk_font_default();
    test_desk_guard();
    test_desk_plan();
    test_desk_icontrol();
    test_desk_targets();
    test_desk_preview();
}
