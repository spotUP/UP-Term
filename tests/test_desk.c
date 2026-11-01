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

/* byte-equal to a golden except the selection colour, which desk_selection() lifts for contrast against the probe's background */
static int golden_same(const unsigned char *out, const unsigned char *gold)
{
    unsigned char a[DK_PALETTE_SIZE];
    memcpy(a, out, DK_PALETTE_SIZE);
    memcpy(a + PAL_ENTRY(DK_SELECTION) + 2, gold + PAL_ENTRY(DK_SELECTION) + 2, 6);
    return !memcmp(a, gold, DK_PALETTE_SIZE);
}

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
        if (i >= PAL_ENTRY(DK_SELECTION) + 2 && i < PAL_ENTRY(DK_SELECTION) + 8)
            continue; /* the selection entry is guarded for contrast, checked below */
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
        if (s == DK_FILLTEXT) {  /* FLAT-SOLID-FILL: the active title text is bright under the flat map */
            CHECK_INT(flat[s], DK_FG);
            CHECK(stock[s] != flat[s]);
        } else if (s == DK_SHINE || s == DK_SHADOW || s == DK_FILL) {  /* FLAT-SOLID-FILL: one surface pen for highlight, shadow, fill */
            CHECK_INT(flat[s], DK_BG);
            CHECK(stock[s] != flat[s]);
        } else
            CHECK_INT(flat[s], stock[s]);
    }
    {   /* FLAT-SOLID-FILL: under the flat map the inactive glyph pen is visible (!= surface pen) and dim (!= active glyph pen) */
        unsigned char gt, gl, gb;
        desk_flat_glyph_pens(&gt, &gl, &gb);
        CHECK_INT(gb, flat[DK_BACKGROUND]);
        CHECK(gt != flat[DK_BACKGROUND]);
        CHECK(gl != flat[DK_BACKGROUND]);
        CHECK(gl != gt);
        CHECK(gl != flat[DK_SHINE] && gl != flat[DK_SHADOW] && gl != flat[DK_FILL]);
        CHECK(gt != flat[DK_FILL]);
        CHECK(flat[DK_FILLTEXT] != flat[DK_FILL]);  /* FLAT-SOLID-FILL: the active title text is readable on its fill */
        CHECK_INT(flat[DK_FILLTEXT], DK_FG);
    }
    for (k = 0; k < 4; k++) {
        char path[256];
        const unsigned char *pm = k < 2 ? stock : flat;
        snprintf(path, sizeof path, DESK_DATA "%s", names[k]);
        CHECK_INT(slurp(path, gold, sizeof gold), DK_PALETTE_SIZE);
        n = desk_palette(&t, k % 2 ? 4 : 8, pm, out, sizeof out);
        CHECK_INT(n, DK_PALETTE_SIZE);
        CHECK(golden_same(out, gold));
        for (s = 0; s < DK_SLOTS; s++) {
            pen = (out[PAL_BODY + 16 + 2 * s] << 8) | out[PAL_BODY + 17 + 2 * s];
            CHECK_INT(pen, pm[s]);
            pen = (out[PAL_BODY + 80 + 2 * s] << 8) | out[PAL_BODY + 81 + 2 * s];
            CHECK_INT(pen, pm[s]);
        }
    }
}


/* FLAT-P1-3: the palette.prefs the desk apply writes follows flat-system-look (the folded amarchy-flat-pens). desk_dos_apply builds it as
 * desk_pens_for(amconf_desk_flat_pens(prefs)) -> desk_palette (the same two calls here); the flat key moves only the
 * shine slot, the pen-reset verdict follows the chosen map, and the key alone writes nothing without the palette target. */
/* FLAT-PENS-LIVE: flat-system-look alone writes the flat pen roles and keeps the user's colours; off restores the stock roles only
 * when the live roles are the flat ones. The reachability twin is the rig (Palette.prefs after the toggle; STATUS flat= rises). */
static void test_desk_flat_pens_live(void)
{
    static unsigned char cur[BUF_MAX], out[BUF_MAX], got[BUF_MAX];
    unsigned char pens[DK_SLOTS], flat[DK_SLOTS], stock[DK_SLOTS];
    unsigned short live[DK_SLOTS], rgb4[8] = {0x000, 0xfff, 0x123, 0xa5c, 0, 0, 0, 0};
    gtheme t;
    long n;
    int i;
    probe_theme(&t);
    desk_pens_flat(flat);
    desk_pens(stock);
    CHECK_INT(desk_palette(&t, 8, stock, cur, sizeof cur), DK_PALETTE_SIZE);
    CHECK_INT(desk_palette_pens(cur, DK_PALETTE_SIZE, flat, out, sizeof out), DK_PALETTE_SIZE);
    CHECK(memcmp(cur, out, DK_PALETTE_SIZE));
    CHECK_INT(desk_palette(&t, 8, flat, got, sizeof got), DK_PALETTE_SIZE);
    CHECK(!memcmp(got, out, DK_PALETTE_SIZE));          /* pens = the flat map, every colour byte = the old file */
    CHECK_INT(desk_palette_pens(cur, 10, flat, out, sizeof out), 0);
    n = desk_palette_live(rgb4, 4, 2, flat, out, sizeof out);
    CHECK_INT(n, DK_PALETTE_SIZE);
    CHECK_INT(out[26 + 8 + 144 + 8 * 2 + 5], 0x22);  /* colour 2 green 0x2 -> 0x2222 */
    for (i = 0; i < DK_SLOTS; i++)
        live[i] = stock[i];
    CHECK_INT(desk_pens_sync(1, live, DK_SLOTS, pens), 1);       /* flat on, stock live: write */
    CHECK(!memcmp(pens, flat, DK_SLOTS));
    CHECK_INT(desk_pens_sync(0, live, DK_SLOTS, pens), 0);       /* flat off, never flat: leave the system's pens */
    for (i = 0; i < DK_SLOTS; i++)
        live[i] = flat[i];
    CHECK_INT(desk_pens_sync(1, live, DK_SLOTS, pens), 0);       /* already flat: no reset */
    CHECK_INT(desk_pens_sync(0, live, DK_SLOTS, pens), 1);       /* flat off, flat live: restore */
    CHECK(!memcmp(pens, stock, DK_SLOTS));
    CHECK_INT(desk_pens_sync(1, 0, 0, pens), 0);
}

/* AM-ICON-NOBORDER: flat-system-look sets the WBNC wbp_Borderless of Workbench.prefs to 1 and off puts the saved value back. The reachability twin
 * is the rig (Workbench.prefs after the toggle; desk_dos_wbborder_writes rises). */
static long wbnc_file(unsigned char *f, int borderless)
{
    memset(f, 0, 26 + 8 + 30);
    memcpy(f, "FORM", 4);
    f[7] = 26 + 8 + 30 - 8;
    memcpy(f + 8, "PREF", 4);
    memcpy(f + 12, "PRHD", 4);
    f[19] = 6;
    memcpy(f + 26, "WBNC", 4);
    f[26 + 7] = 30;
    f[34 + 2] = 0x10;                           /* stack size 4096 */
    f[34 + 7] = 4;                              /* restart time */
    f[34 + 28] = 1;                             /* colour icons */
    f[34 + 21] = (unsigned char)borderless;     /* wbp_Borderless, a WORD at body offset 20 */
    return 64;
}

static void test_desk_wbborder(void)
{
    static unsigned char cur[128], out[128], back[128];
    int keep;
    long n = wbnc_file(cur, 0);
    CHECK_INT(desk_wbborder_value(cur, n), 0);
    CHECK_INT(desk_wbborder(1, cur, n, -1, &keep, out, sizeof out), n);   /* on: border -> no border, 0 saved */
    CHECK_INT(keep, 0);
    CHECK_INT(desk_wbborder_value(out, n), 1);
    CHECK(!memcmp(cur, out, 34 + 20) && !memcmp(cur + 34 + 22, out + 34 + 22, 8));   /* every other byte kept */
    CHECK_INT(desk_wbborder(1, out, n, 0, &keep, back, sizeof back), 0);  /* already none: no second write */
    CHECK_INT(keep, -1);
    CHECK_INT(desk_wbborder(0, out, n, 0, &keep, back, sizeof back), n);  /* off: the saved 0 returns */
    CHECK_INT(keep, -2);
    CHECK(!memcmp(back, cur, (size_t)n));
    CHECK_INT(desk_wbborder(0, out, n, -1, &keep, back, sizeof back), 0); /* never changed by us: left alone */
    n = wbnc_file(cur, 1);
    CHECK_INT(desk_wbborder(1, cur, n, -1, &keep, out, sizeof out), 0);   /* the user already had none: not saved, not restored */
    CHECK_INT(keep, -1);
    CHECK_INT(desk_wbborder(0, cur, n, -1, &keep, out, sizeof out), 0);
    CHECK_INT(desk_wbborder(1, cur, 10, -1, &keep, out, sizeof out), 0);  /* no WBNC chunk (3.1): skipped */
    CHECK_INT(desk_wbborder_value(cur, 10), -1);
}

/* AM-ICON-NOBORDER: flat on with no Workbench.prefs creates the editor's stock file with the border off; off removes it again */
static unsigned char *user_stock(unsigned char *buf, long n)
{
    int keep;
    desk_wbborder(1, 0, 0, -1, &keep, buf, 128);
    buf[26 + 8 + 21] = 0;                                                  /* wbp_Borderless word low byte */
    (void)n;
    return buf;
}

static void test_desk_wbborder_absent(void)
{
    static unsigned char made[128], back[128], user[128];
    int keep;
    long n = desk_wbborder(1, 0, 0, -1, &keep, made, sizeof made);
    CHECK_INT(n, 84);                                                      /* the 3.2 editor's file measured on a fresh rig */
    CHECK_INT(keep, DESK_WB_CREATED);
    CHECK_INT(desk_wbborder_value(made, n), 1);
    CHECK(!memcmp(made, "FORM", 4) && !memcmp(made + 8, "PREF", 4) && !memcmp(made + 26, "WBNC", 4));
    CHECK_INT(desk_wbborder(0, 0, 0, -1, &keep, back, sizeof back), 0);    /* off with no file and nothing made: nothing */
    CHECK_INT(keep, -1);
    CHECK_INT(desk_wbborder(1, made, n, DESK_WB_CREATED, &keep, back, sizeof back), 0);   /* second apply: no second write */
    CHECK_INT(desk_wbborder(0, made, n, DESK_WB_CREATED, &keep, back, sizeof back), n);   /* off: stock border word written, the file stays */
    CHECK_INT(keep, -2);
    CHECK_INT(desk_wbborder_value(back, n), 0);
    CHECK(!memcmp(back, user_stock(user, n), (size_t)n));                  /* identical to the stock file with border 0 */
    CHECK_INT(desk_wbborder(0, 0, 0, DESK_WB_CREATED, &keep, back, sizeof back), 0);      /* the user deleted it meanwhile: nothing written, mark forgotten */
    CHECK_INT(keep, -2);
}

/* flat off keeps the user's other Workbench tweaks */
static void test_desk_wbborder_keeps_tweaks(void)
{
    static unsigned char made[128], tweaked[128], back[128];
    int keep;
    long n = desk_wbborder(1, 0, 0, -1, &keep, made, sizeof made);
    memcpy(tweaked, made, (size_t)n);
    tweaked[26 + 8 + 3] = 9;                                               /* WBNC stack size low byte: a setting the user changed while flat was on */
    CHECK_INT(desk_wbborder(0, tweaked, n, DESK_WB_CREATED, &keep, back, sizeof back), n);
    CHECK_INT(back[26 + 8 + 3], 9);                                        /* the tweak survives */
    CHECK_INT(desk_wbborder_value(back, n), 0);                            /* the border word is back */
    CHECK_INT(keep, -2);
}

/* WB-VOLUME-GAUGE-OFF: wbe_DisableVolumeGauge is the WORD at WBNC body offset 38; flat sets it, off puts the user's value back byte-identical, 3.1 files stay */
static void test_desk_wbgauge(void)
{
    static unsigned char cur[128], out[128], back[128], small[128];
    int keep;
    long n = desk_wbborder(1, 0, 0, -1, &keep, cur, sizeof cur);   /* the 3.2 editor's stock file: gauge shown */
    CHECK_INT(n, 84);
    CHECK_INT(desk_wbgauge_value(cur, n), 0);
    CHECK_INT(desk_wbgauge(1, cur, n, -1, &keep, out, sizeof out), n);
    CHECK_INT(keep, 0);
    CHECK_INT(desk_wbgauge_value(out, n), 1);
    CHECK_INT(out[26 + 8 + 38], 0);
    CHECK_INT(out[26 + 8 + 39], 1);
    CHECK(!memcmp(cur, out, 26 + 8 + 38) && !memcmp(cur + 26 + 8 + 40, out + 26 + 8 + 40, (size_t)(n - 26 - 8 - 40)));   /* every other chunk and byte untouched */
    CHECK_INT(desk_wbgauge(1, out, n, 0, &keep, back, sizeof back), 0);    /* already off: no second write */
    CHECK_INT(keep, -1);
    CHECK_INT(desk_wbgauge(0, out, n, 0, &keep, back, sizeof back), n);    /* off: the user's 0 returns */
    CHECK_INT(keep, -2);
    CHECK(!memcmp(back, cur, (size_t)n));
    CHECK_INT(desk_wbgauge(0, out, n, -1, &keep, back, sizeof back), 0);   /* never changed by us */
    memcpy(back, cur, (size_t)n);
    back[26 + 8 + 39] = 1;                                                 /* the user had the gauge off themselves */
    CHECK_INT(desk_wbgauge(1, back, n, -1, &keep, out, sizeof out), 0);
    CHECK_INT(keep, -1);
    CHECK_INT(desk_wbgauge(0, back, n, -1, &keep, out, sizeof out), 0);
    n = wbnc_file(small, 0);                                               /* a 3.1 style 30-byte WBNC: no gauge field */
    CHECK_INT(desk_wbgauge_value(small, n), -1);
    CHECK_INT(desk_wbgauge(1, small, n, -1, &keep, out, sizeof out), 0);
    CHECK_INT(desk_wbgauge(1, small, 10, -1, &keep, out, sizeof out), 0);  /* no WBNC chunk at all */
    CHECK_INT(desk_wbgauge(1, 0, 0, -1, &keep, out, sizeof out), 0);       /* no file */
}

static void test_desk_flat_pens_key(void)
{
    static unsigned char gold[BUF_MAX], out[BUF_MAX];
    static const char flat_on[] = "[profile default]\ndesktop-theme = on\nflat-system-look = on\n";
    static const char flat_off[] = "[profile default]\ndesktop-theme = on\n";
    static const char flat_nopal[] = "[profile default]\ndesktop-theme = on\ndesktop-palette = off\nflat-system-look = on\n";
    static const char flat_nodesk[] = "[profile default]\nflat-system-look = on\n";
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
        CHECK(golden_same(out, gold));
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
        CHECK_INT(amconf_desk_flat_pens(&p), 1);              /* the flat map no longer needs the desktop palette target */
        CHECK(!(amconf_desk_mask(&p) & (1u << DT_PALETTE)));
        amconf_parse(flat_nodesk, (long)strlen(flat_nodesk), &p);
        CHECK_INT(amconf_desk_flat_pens(&p), 1);              /* ... nor the master switch: desk_dos_apply writes the pens itself */
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

/* FLAT-COLOUR-OVERHAUL: the backdrop has its own pen, so pen 5 draws the theme muted colour, not the backdrop tone; and the selection
 * pen stands >= 1.5 off the surface on every shipped theme */
static void test_flat_colour_pens(void)
{
    gtheme all[SHIPPED_MAX];
    int n = load_all(all, SHIPPED_MAX), i, depth;
    CHECK_INT(n, 23);
    for (depth = 3; depth <= 8; depth++) {
        CHECK(desk_backdrop_pen_at(depth) != DK_MUTED);
        CHECK(desk_backdrop_pen_at(depth) != DK_SELECTION);
        CHECK(desk_backdrop_pen_at(depth) < 8);
    }
    CHECK_INT(desk_backdrop_pen_at(2), DK_DARKER); /* below 3 planes no pen is free: the shared darker pen holds the same tone */
    for (i = 0; i < n; i++) {
        uc_u32 sel = desk_selection(&all[i]);
        if (desk_contrast(all[i].selection, all[i].bg) >= 150)
            CHECK_INT(sel, all[i].selection); /* a good selection is left alone */
        CHECK(desk_contrast(sel, all[i].bg) >= 150);
        CHECK_INT(desk_pen_colour(&all[i], DK_MUTED, 1), (long)all[i].muted);
        CHECK(desk_pen_colour(&all[i], desk_backdrop_pen(), 1) == desk_darker(&all[i]));
    }
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
    CHECK_INT(ptrn(out, 0)[8 + 24], 0xff);          /* the backdrop pen (7 at 3 planes): every plane set */
    CHECK_INT(ptrn(out, 0)[8 + 24 + 32], 0xff);
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
    /* the Workbench backdrop has no dots: every ROOT pattern pixel is the backdrop pen (its low bits on a shallower screen, which has no such pen) */
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
                CHECK_INT(pen, desk_backdrop_pen_at(depth) & ((1 << depth) - 1));
            }
    }
}

static void test_flat_on_keeps_backdrop_pen_distinct_from_window_background(void)
{
    /* FLAT-BACKDROP-SAME-AS-WINDOW: flat pens put BACKGROUNDPEN on pen 0 (the window colour); the backdrop must use another pen,
     * painted in the theme dark, and follow the theme */
    static unsigned char pal[BUF_MAX], out[BUF_MAX], pat[BUF_MAX];
    unsigned char pens[DK_SLOTS];
    gtheme a, b;
    long n, m;
    int p, pen = 0, depth = 3;
    const unsigned char *data;
    probe_theme(&a);
    probe_theme(&b);
    a.darker_bg = 0x222222UL;
    b.darker_bg = 0x336699UL;
    desk_pens_flat(pens);
    CHECK(desk_backdrop_pen() != pens[DK_BACKGROUND]);
    /* the backdrop pen is its own: not a stock icon pen (0-3), not in the DrawInfo array flat writes, inside the 3-plane tile range */
    CHECK(desk_backdrop_pen() > 3 && desk_backdrop_pen() < (1 << depth));
    CHECK(desk_backdrop_pen() != DK_MUTED); /* FLAT-COLOUR-OVERHAUL */
    for (p = 0; p < DK_SLOTS; p++)
        CHECK(pens[p] != desk_backdrop_pen());
    n = desk_palette(&a, 8, pens, pal, sizeof pal);
    CHECK(n > 0);
    CHECK_INT(desk_palette_rgb(pal, n, 1, depth) >= 0, 1);
    m = desk_palette_set_rgb(pal, n, desk_backdrop_pen(), depth, desk_darker(&a), out, sizeof out);
    CHECK(m > 0);
    CHECK_INT(desk_palette_rgb(out, m, desk_backdrop_pen(), depth), desk_darker(&a));
    CHECK(desk_palette_rgb(out, m, desk_backdrop_pen(), depth) != desk_palette_rgb(out, m, pens[DK_BACKGROUND], depth));
    m = desk_palette_set_rgb(pal, n, desk_backdrop_pen(), depth, desk_darker(&b), out, sizeof out);
    CHECK_INT(desk_palette_rgb(out, m, desk_backdrop_pen(), depth), desk_darker(&b));
    /* pen 1 (BLOCKPEN, stock icon black) keeps its colour; so do all other pens */
    for (p = 0; p < 8; p++)
        if (p != desk_backdrop_pen())
            CHECK_INT(desk_palette_rgb(out, m, p, depth), desk_palette_rgb(pal, n, p, depth));
    CHECK(desk_darker(&a) != desk_darker(&b));
    CHECK_INT(desk_palette_rgb(pal, 0, desk_backdrop_pen(), depth), -1);
    CHECK_INT(desk_wbpattern(&a, depth, 0, 0, pat, sizeof pat), 410);
    data = ptrn(pat, 0) + 8 + 24;
    for (p = 0; p < depth; p++)
        if (data[p * 32] & 0x80)
            pen |= 1 << p;
    CHECK_INT(pen, desk_backdrop_pen());
    CHECK(pen != pens[DK_BACKGROUND]);
}

/* FLAT-PENS47-RESET (rig 14, 3.2, depth 8, wbpens WATCH): desk_dos_apply set pens 4-7 to the theme on screen A, the Workbench reopen 3
 * ticks later made screen B with graphics-default 0000ff ff00ff 00ffff ffffff and nothing set them again. The screen follow path
 * (iconpen_dos pens_sync, IP_SYNC_OBTAIN) asks desk_live_follow what a new screen needs. */
static void test_flat_reopen_keeps_theme_on_pens47(void)
{
    gtheme t;
    int lp[4], i;
    uc_u32 c[4];
    probe_theme(&t);
    t.darker_bg = 0x222222UL;
    /* flat on, flat owns the backdrop (no DT_WBPATTERN): pens 4-7, pen 5 the dark backdrop, the rest their role colours */
    CHECK_INT(desk_live_follow(&t, 1, 0, 8, lp, c), 4);
    for (i = 0; i < 4; i++) {
        CHECK_INT(lp[i], 4 + i);
        CHECK_INT((long)c[i], (long)desk_pen_colour(&t, 4 + i, 1));
    }
    CHECK_INT((long)c[3], 0x222222L);            /* pen 7: the backdrop's own pen */
    CHECK_INT((long)c[1], (long)t.muted);        /* pen 5 keeps the muted text colour */
    CHECK((long)c[1] != 0xff00ffL);
    /* the wallpaper is the desktop theme's: pen 5 keeps its role colour */
    CHECK_INT(desk_live_follow(&t, 1, 1u << DT_WBPATTERN, 8, lp, c), 4);
    CHECK_INT((long)c[1], (long)desk_pen_colour(&t, 5, 0));
    /* the desktop-theme palette alone also puts the theme on the live pens */
    CHECK_INT(desk_live_follow(&t, 0, 1u << DT_PALETTE, 8, lp, c), 4);
    /* flat off and no theme palette: the new screen keeps the user's colours; depth 3 has no live pens; no theme, nothing */
    CHECK_INT(desk_live_follow(&t, 0, 0, 8, lp, c), 0);
    CHECK_INT(desk_live_follow(&t, 0, 1u << DT_WBPATTERN, 8, lp, c), 0);
    CHECK_INT(desk_live_follow(&t, 1, 0, 3, lp, c), 0);
    CHECK_INT(desk_live_follow(0, 1, 0, 8, lp, c), 0);
}

/* FLAT-PEN5-MAGENTA (rig 11, 3.2, depth 8): IPrefs puts palette.prefs entries 4-7 on the last four pens of a deep screen, so a flat pick
 * that coloured "entry 5" left pen 5 (backdrop, button dim) at the graphics default ff00ff and overwrote pens 252-255. */
static void test_flat_pick_leaves_pen5_magenta_on_deep_workbench(void)
{
    static unsigned char cur[BUF_MAX], out[BUF_MAX];
    static unsigned short rgb4[256];
    unsigned char flat[DK_SLOTS];
    gtheme t;
    int lp[4], p;
    long n, m;
    probe_theme(&t);
    t.darker_bg = 0x222222UL;
    desk_pens_flat(flat);
    /* the one mapping: entries 0-3 -> pens 0-3 at every depth; 4-7 -> 252-255 at depth 8, themselves at depth 3, none at depth 2 */
    CHECK_INT(desk_palette_pen(5, 8), 253);
    CHECK_INT(desk_palette_pen(4, 4), 12);
    CHECK_INT(desk_palette_pen(5, 3), 5);
    CHECK_INT(desk_palette_pen(5, 2), -1);
    CHECK_INT(desk_palette_pen(2, 8), 2);
    CHECK_INT(desk_palette_entry_of(5, 8), -1);
    CHECK_INT(desk_palette_entry_of(253, 8), 5);
    CHECK_INT(desk_live_role_pens(8, lp), 4);
    CHECK(lp[0] == 4 && lp[1] == 5 && lp[2] == 6 && lp[3] == 7);
    CHECK_INT(desk_live_role_pens(3, lp), 0);
    CHECK_INT(desk_live_role_pens(2, lp), 0);
    /* the live snapshot of a depth-8 screen: entry 5 is pen 253 (0x222), not pen 5 (0xf0f) */
    for (p = 0; p < 256; p++)
        rgb4[p] = 0x0f0f;
    rgb4[252] = 0xe44; rgb4[253] = 0x222; rgb4[254] = 0x04d; rgb4[255] = 0xe90;
    n = desk_palette_live(rgb4, 256, 8, flat, cur, sizeof cur);
    CHECK_INT(n, DK_PALETTE_SIZE);
    CHECK_INT(desk_palette_rgb(cur, n, 253, 8), 0x222222L);
    CHECK_INT(desk_palette_rgb(cur, n, 5, 8), -1);
    CHECK_INT(desk_palette_set_rgb(cur, n, desk_backdrop_pen(), 8, 0x123456UL, out, sizeof out), 0);
    /* the flat pick: role pens 0-3 get the theme in the flat map's role order; 252-255 keep the user's colours; 4-7 are live */
    m = desk_palette_theme(cur, n, &t, 8, 1, out, sizeof out);
    CHECK_INT(m, n);
    for (p = 0; p < 4; p++)
        CHECK_INT(desk_palette_rgb(out, m, p, 8), (long)desk_role(&t, p));
    CHECK_INT(desk_palette_rgb(out, m, 1, 8), 0x222222L);
    for (p = 252; p < 256; p++)
        CHECK_INT(desk_palette_rgb(out, m, p, 8), desk_palette_rgb(cur, n, p, 8));
    CHECK_INT(desk_pen_colour(&t, desk_backdrop_pen(), 1), 0x222222L);
    CHECK_INT(desk_pen_colour(&t, 4, 1), (long)desk_selection(&t));
    CHECK_INT(desk_pen_colour(&t, 5, 1), (long)t.muted); /* FLAT-COLOUR-OVERHAUL: the muted pen keeps the muted colour under the own backdrop */
    CHECK(desk_backdrop_pen() != 5);
    CHECK_INT(desk_pen_colour(&t, desk_backdrop_pen(), 0), (long)t.muted);
    /* depth 3: the same pick reaches pens 4-7 through the file, the backdrop pen gets the theme dark */
    n = desk_palette_live(rgb4, 8, 3, flat, cur, sizeof cur);
    m = desk_palette_theme(cur, n, &t, 3, 1, out, sizeof out);
    CHECK_INT(desk_palette_rgb(out, m, desk_backdrop_pen(), 3), 0x222222L);
    CHECK_INT(desk_palette_rgb(out, m, 4, 3), (long)desk_selection(&t));
    CHECK_INT(desk_palette_rgb(out, m, 5, 3), (long)t.muted);
    CHECK_INT(desk_palette_theme(cur, 10, &t, 3, 1, out, sizeof out), 0);
}

/* flat OFF restores the saved palette even when the menu held pens 4-7: the baseline of a live role pen comes from the saved
 * palette.prefs through the per-depth map, and the live screen (where an open menu's ObtainBestPen colours sit on pens 4-7, rgb4 below)
 * has no say. Pens 4-7 deeper than 3 are in no entry: graphics default. At depth 3 the entries reach them: the saved colours. */
static void test_desk_live_baseline_ignores_menu(void)
{
    static unsigned char saved[BUF_MAX], flat[DK_SLOTS];
    unsigned short rgb4[256];
    int p;
    long n;
    desk_pens_flat(flat);
    for (p = 0; p < 256; p++)
        rgb4[p] = 0x0123; /* the menu's colours on every free pen */
    rgb4[252] = 0xe44; rgb4[253] = 0x222; rgb4[254] = 0x04d; rgb4[255] = 0xe90;
    n = desk_palette_live(rgb4, 256, 8, flat, saved, sizeof saved);
    CHECK_INT(n, DK_PALETTE_SIZE);
    CHECK_INT(desk_live_baseline(saved, n, 4, 8), 0x0000FFL);
    CHECK_INT(desk_live_baseline(saved, n, 5, 8), 0xFF00FFL);
    CHECK_INT(desk_live_baseline(saved, n, 6, 8), 0x00FFFFL);
    CHECK_INT(desk_live_baseline(saved, n, 7, 8), 0xFFFFFFL);
    CHECK_INT(desk_live_baseline(0, -1, 5, 8), 0xFF00FFL);
    n = desk_palette_live(rgb4, 8, 3, flat, saved, sizeof saved);
    CHECK_INT(desk_live_baseline(saved, n, 5, 3), 0x112233L);
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
/* FP-1 (R1.8): the confirm window. Keep ends it; Revert and the 15 s timeout both ask for a revert; a second apply restarts the clock and keeps the first snapshot's targets */
static void test_desk_confirm(void)
{
    desk_confirm c;
    desk_confirm_init(&c);
    CHECK_INT(desk_confirm_tick(&c, 100), DC_WAIT);             /* nothing pending: no revert, ever */
    CHECK_INT(desk_confirm_answer(&c, 0), DC_WAIT);
    desk_confirm_begin(&c, 100, 0x3);
    CHECK_INT(c.pending, 1);
    CHECK_INT(desk_confirm_left(&c, 100), DESK_CONFIRM_SECS);
    CHECK_INT(desk_confirm_tick(&c, 100 + DESK_CONFIRM_SECS - 1), DC_WAIT);
    CHECK_INT(desk_confirm_tick(&c, 100 + DESK_CONFIRM_SECS), DC_REVERT_TIMEOUT);
    CHECK_INT(c.pending, 0);
    CHECK_INT(desk_confirm_tick(&c, 500), DC_WAIT);             /* once only */
    desk_confirm_begin(&c, 200, 0x1);
    CHECK_INT(desk_confirm_answer(&c, 1), DC_KEEP);
    CHECK_INT(c.pending, 0);
    CHECK_INT(desk_confirm_tick(&c, 900), DC_WAIT);             /* kept: the timer no longer reverts */
    desk_confirm_begin(&c, 300, 0x1);
    CHECK_INT(desk_confirm_answer(&c, 0), DC_REVERT_USER);
    desk_confirm_begin(&c, 400, 0x1);
    desk_confirm_begin(&c, 410, 0x4);                           /* second apply: clock restarts, masks add up */
    CHECK_INT((int)c.mask, 0x5);
    CHECK_INT(desk_confirm_tick(&c, 410 + DESK_CONFIRM_SECS - 1), DC_WAIT);
    CHECK_INT(desk_confirm_left(&c, 420), DESK_CONFIRM_SECS - 10);
    CHECK_INT(desk_confirm_tick(&c, 410 + DESK_CONFIRM_SECS), DC_REVERT_TIMEOUT);
}

/* FP-2 (R1.10, OD14): what Keep copies to ENVARC:, what "off" restores, how the originals are named */
static void test_desk_persist(void)
{
    const char *env[8], *arc[8];
    char name[96], tiny[8];
    int n;
    n = desk_persist_pairs((1u << DT_PALETTE) | (1u << DT_FONT) | (1u << DT_ICONTROL), 1, env, arc, 8);
    CHECK_INT(n, 3);                                            /* icontrol writes both copies itself: not a pair */
    CHECK_STR(env[0], "ENV:Sys/palette.prefs");
    CHECK_STR(arc[0], "ENVARC:Sys/palette.prefs");
    CHECK_STR(env[1], "ENV:Sys/font.prefs");
    CHECK_STR(arc[1], "ENVARC:Sys/font.prefs");
    CHECK_STR(env[2], "ENV:Sys/Workbench.prefs");
    CHECK_STR(arc[2], "ENVARC:Sys/Workbench.prefs");
    CHECK_INT(desk_persist_pairs(0, 0, env, arc, 8), 0);
    CHECK_INT(desk_persist_pairs(0xffu, 1, env, arc, 2), 2);    /* cap respected */
    CHECK_INT((int)desk_off_mask(0x1fu, 0x5u), 0x1a & ~(1 << DT_ICONTROL)); /* saved 0..4, applied palette+pointer: wbpattern, font off; icontrol never */
    CHECK_INT((int)desk_off_mask(0x3u, 0x3u), 0);
    CHECK_INT((int)desk_off_mask(0, 0), 0);
    CHECK_INT((int)desk_off_mask(0x3u, 0), 3);
    CHECK_INT(desk_copy_name("ENVARC:Amarchy/desk-original", "ENV:Sys/palette.prefs", name, sizeof name), 1);
    CHECK_STR(name, "ENVARC:Amarchy/desk-original/ENV-Sys-palette.prefs");
    CHECK_INT(desk_copy_name("T:x", "ENV:Sys/palette.prefs", tiny, sizeof tiny), 0);
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

/* s28: the icon applier re-runs when the set changes while flat is on, and not when nothing changed */
static void test_desk_icons_state(void)
{
    CHECK_INT(desk_icons_state(0, 0), 0);
    CHECK_INT(desk_icons_state(0, 1), 0);        /* flat off: originals, whatever the style */
    CHECK_INT(desk_icons_state(1, 0), 1);
    CHECK_INT(desk_icons_state(1, 1), 2);
    CHECK(desk_icons_state(1, 0) != desk_icons_state(1, 1));
}

void suite_desk(void)
{
    test_desk_icons_state();
    test_desk_palette();
    test_desk_palette_flat();
    test_desk_live_baseline_ignores_menu();
    test_desk_flat_pens_key();
    test_desk_flat_pens_live();
    test_desk_wbborder();
    test_desk_wbborder_absent();
    test_desk_wbborder_keeps_tweaks();
    test_desk_wbgauge();
    test_desk_pens_reset();
    test_desk_reset_done();
    test_desk_iprefs_idle();
    test_desk_darker();
    test_flat_colour_pens();
    test_desk_backdrop();
    test_desk_wbpattern();
    test_desk_wbpattern_no_dots();
    test_flat_on_keeps_backdrop_pen_distinct_from_window_background();
    test_flat_pick_leaves_pen5_magenta_on_deep_workbench();
    test_flat_reopen_keeps_theme_on_pens47();
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
    test_desk_confirm();
    test_desk_persist();
}
