/* desk_core: see desk_core.h */
#include "desk_core.h"
#include "../handler/be32.h"

#include <string.h>

/* ---- colour maths ---- */

int desk_luma(uc_u32 rgb)
{
    return (int)((((rgb >> 16) & 0xff) * 77UL + ((rgb >> 8) & 0xff) * 150UL + (rgb & 0xff) * 29UL) >> 8);
}

int desk_contrast(uc_u32 a, uc_u32 b)
{
    int la = desk_luma(a), lb = desk_luma(b), hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (hi + 13) * 100 / (lo + 13);
}

/* the guard floors, ratio times 100 of desk_contrast; measured over the 23 shipped themes (see test_desk.c) */
#define DK_FLOOR_TEXT 220 /* text on a ground: measured, the weakest shipped pair is rose-pine at 233 */
#define DK_FLOOR_FILL 150 /* the title text on the fill, and the fill on the ground */
#define DK_STEP_DARK 108  /* the darker tone against the background */
#define DK_FLOOR_SEL 150  /* the selection (button fill, scroller knob) against the surface: shipped light themes sit at 110-130 */

#define PT_DEPTH_MAX 3 /* the planes of a WBPattern tile */

#define DK_BLACK_LIFT 0x222222UL /* the backdrop over a black ground: dark grey, lighter than black */

/* the colour as a 12-bit (OCS) screen holds it: a nibble per channel, replicated. The weakest depth Amarchy runs on. */
static uc_u32 nib(uc_u32 c)
{
    return (((c >> 20) & 0xf) * 0x110000UL) | (((c >> 12) & 0xf) * 0x1100UL) | (((c >> 4) & 0xf) * 0x11UL);
}

static int qluma(uc_u32 c)
{
    return desk_luma(nib(c));
}

int desk_ground_black(const gtheme *t)
{
    return qluma(t->bg) == 0;
}

/* DESK-BACKDROP-DARKER: never the ground's own colour, on a 24-bit or a 12-bit palette. A ground that is black on the
 * screen keeps it and the backdrop lifts to dark grey; any other ground gets a tone strictly darker after the nibble cut. */
uc_u32 desk_darker(const gtheme *t)
{
    int pct;
    if (desk_ground_black(t))
        return DK_BLACK_LIFT;
    if (desk_luma(t->darker_bg) < desk_luma(t->bg) && qluma(t->darker_bg) < qluma(t->bg) &&
        desk_contrast(t->darker_bg, t->bg) >= DK_STEP_DARK && desk_contrast(t->darker_bg, t->fg) >= DK_FLOOR_TEXT)
        return t->darker_bg;
    for (pct = 80; pct > 0; pct -= 10)
        if (qluma(gt_scale(t->bg, pct)) < qluma(t->bg))
            return gt_scale(t->bg, pct);
    return 0x000000UL;
}

/* FLAT-COLOUR-OVERHAUL: the selection pen colour. The theme's selection when it stands DK_FLOOR_SEL off the surface (as the nibble-cut
 * screen shows both), else pushed away from the surface in 10 % steps: toward black on a light ground, toward white on a dark one. */
uc_u32 desk_selection(const gtheme *t)
{
    uc_u32 sel = t->selection, goal = desk_luma(t->bg) >= 128 ? 0x000000UL : 0xFFFFFFUL;
    int pct;
    for (pct = 0; pct <= 100; pct += 10) {
        uc_u32 c = pct ? 0 : sel;
        if (pct) {
            int sh;
            for (sh = 16; sh >= 0; sh -= 8) {
                int a = (int)((sel >> sh) & 255), b = (int)((goal >> sh) & 255);
                c |= (uc_u32)(a + (b - a) * pct / 100) << sh;
            }
        }
        if (desk_contrast(c, t->bg) >= DK_FLOOR_SEL && desk_contrast(nib(c), nib(t->bg)) >= DK_FLOOR_SEL)
            return c;
    }
    return goal;
}

uc_u32 desk_role(const gtheme *t, int role)
{
    switch (role) {
    case DK_BG: return t->bg;
    case DK_DARKER: return desk_darker(t);
    case DK_FG: return t->fg;
    case DK_ACCENT: return t->accent;
    case DK_SELECTION: return desk_selection(t);
    case DK_MUTED: return t->muted;
    case DK_GREEN: return t->ok;
    default: return t->fail;
    }
}

/* ---- byte helpers ---- */

static void put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static unsigned get16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

/* the IFF header of a prefs file: FORM size PREF PRHD 6 zeros; the chunks follow at DK_HDR */
#define DK_HDR 26L

static unsigned char *iff_header(unsigned char *out, long total)
{
    memset(out, 0, (size_t)DK_HDR);
    memcpy(out, "FORM", 4);
    be32_put(out + 4, (unsigned long)(total - 8));
    memcpy(out + 8, "PREF", 4);
    memcpy(out + 12, "PRHD", 4);
    be32_put(out + 16, 6UL);
    return out + DK_HDR;
}

/* the next chunk at *pos of a prefs file: its id in id, body offset and size; 0 at the end or when malformed */
static int next_chunk(const unsigned char *buf, long len, long *pos, char id[4], long *body, long *size)
{
    long p = *pos, n;
    if (p + 8 > len)
        return 0;
    memcpy(id, buf + p, 4);
    n = (long)be32_get(buf + p + 4);
    if (n < 0 || p + 8 + n > len)
        return 0;
    *body = p + 8;
    *size = n;
    *pos = p + 8 + n + (n & 1);
    return 1;
}

static int is_prefs(const unsigned char *buf, long len)
{
    return len >= DK_HDR && !memcmp(buf, "FORM", 4) && !memcmp(buf + 8, "PREF", 4);
}

/* ---- R1.19: pens ---- */

void desk_pens(unsigned char pens[DK_SLOTS])
{
    pens[DK_DETAIL] = DK_BG;
    pens[DK_BLOCK] = DK_DARKER;
    pens[DK_TEXT] = DK_FG;
    pens[DK_SHINE] = DK_FG;
    pens[DK_SHADOW] = DK_DARKER;
    pens[DK_FILL] = DK_ACCENT;
    pens[DK_FILLTEXT] = DK_BG;
    pens[DK_BACKGROUND] = DK_BG;
    pens[DK_HIGHLIGHTTEXT] = DK_FG;
    pens[DK_BARDETAIL] = DK_FG;
    pens[DK_BARBLOCK] = DK_BG;
    pens[DK_BARTRIM] = DK_DARKER;
    pens[DK_EXTRA] = DK_FG;
}

void desk_pens_flat(unsigned char pens[DK_SLOTS])
{
    desk_pens(pens);
    pens[DK_SHINE] = DK_BG;     /* FLAT-SOLID-FILL: highlight, shadow and the active frame fill all take the surface pen, */
    pens[DK_SHADOW] = DK_BG;    /* so every window is one solid rectangle and only title and glyph colour show focus */
    pens[DK_FILL] = DK_BG;
    pens[DK_EXTRA] = pens[DK_BARBLOCK];  /* FLAT-BAR-CONTOUR: slot 12 is BARCONTOURPEN (OS 3.2): the 1 px line along the top of the screen bar, stock FG = a bright "shine" line; the bar background hides it */
    pens[DK_FILLTEXT] = DK_FG;  /* the active title text is drawn in FILLTEXTPEN on FILLPEN (= DK_BG now): keep it bright */
}

/* FLAT-SOLID-FILL: the glyph pens of the flat_state block under the flat map: active glyph bright (foreground), inactive
 * glyph dim (accent, a mid grey: the darker slot is too close to the surface to read), box background = surface pen.
 * None may equal the surface pen the frame now takes (shine == shadow == fill == DK_BG), except back. */
void desk_flat_glyph_pens(unsigned char *text, unsigned char *line, unsigned char *back)
{
    *text = DK_FG;
    *line = DK_ACCENT;
    *back = DK_BG;
}

unsigned long desk_flat_state_calls;

int desk_icons_state(int on, int outline)
{
    return on ? (outline ? 2 : 1) : 0;
}

void desk_flat_state(int flat, int gadgets, flat_state *s)
{
    unsigned char t, l, b;
    desk_flat_state_calls++;
    desk_flat_glyph_pens(&t, &l, &b);
    s->enabled = (flat || gadgets) ? 1 : 0;
    s->pen_text = t;
    s->pen_line = l;
    s->pen_back = b;
    s->pen_outline = DK_ACCENT;
    s->pen_button = DK_SELECTION;       /* FLAT-BUTTON-FILL: the theme lighter_background; flat.c falls back to pen_back on a screen without pen 4 */
    s->pen_ptext = DK_BG;               /* dark label on the accent fill of a pressed button */
    s->pen_dimtext = DK_MUTED;          /* FLAT-BUTTON-DISABLED-DIM: dim label; pen_dim is amarchy-flat's own (it obtains it) and is left alone */
}

void desk_pens_for(int flat, unsigned char pens[DK_SLOTS])
{
    if (flat)
        desk_pens_flat(pens);
    else
        desk_pens(pens);
}

void desk_pen_plan_for(int depth, desk_pen_plan *p)
{
    p->button = DK_SELECTION;
    p->backdrop = depth >= PT_DEPTH_MAX ? DK_RED : DK_DARKER; /* FLAT-COLOUR-OVERHAUL: pen 5 keeps the muted text colour; below 3 planes no pen is free, the backdrop shares the darker pen */
    if (depth >= 6) {
        p->icon_first = 32; p->icon_count = 16; p->icon_slot = 0;
    } else if (depth >= 4) {
        p->icon_first = 8; p->icon_count = 8; p->icon_slot = 8;
    } else {
        p->icon_first = 0; p->icon_count = 0; p->icon_slot = 0;
    }
}

int desk_pens_reset(const unsigned short *cur, int ncur, int flat)
{
    unsigned char want[DK_SLOTS];
    int i;
    if (!cur || ncur <= 0)
        return 1;
    desk_pens_for(flat, want);
    for (i = 0; i < DK_SLOTS && i < ncur; i++)
        if (cur[i] != want[i])
            return 1;
    return 0;
}

int desk_reset_done(const void *before, const void *now, const unsigned short *live, int nlive, int flat)
{
    if (!now || nlive <= 0) /* the screen is closed for about 200 ms in the middle of the reset (rig 7, 3.2): not done, not "a new screen" */
        return 0;
    return now != before || !desk_pens_reset(live, nlive, flat);
}

int desk_iprefs_idle(int have, int waiting, unsigned long sigwait, unsigned long recvd, unsigned long idle_mask)
{
    if (!have)
        return 1;
    return waiting && idle_mask != 0 && sigwait == idle_mask && (recvd & idle_mask) == 0;
}

/* ---- R1.2: palette.prefs ---- */

#define PAL_BODY 400L
#define PAL_4PENS 16
#define PAL_8PENS 80
#define PAL_COLORS 144

/* an 8-bit channel to the 16-bit of ColorSpec: byte replication, so 0xff is 0xffff */
static unsigned wide(uc_u32 c, int shift)
{
    unsigned b = (unsigned)((c >> shift) & 0xff);
    return (b << 8) | b;
}

/* the palette.prefs body for ncolors colours (rgb16[i] = 16-bit r, g, b) and the pen map pens */
static long palette_fill(int ncolors, const unsigned char pens[DK_SLOTS], const unsigned short rgb16[][3], unsigned char *out, long cap)
{
    unsigned char *body;
    int i;
    if ((ncolors != 4 && ncolors != 8) || cap < DK_PALETTE_SIZE)
        return 0;
    body = iff_header(out, DK_PALETTE_SIZE);
    memcpy(body, "PALT", 4);
    be32_put(body + 4, (unsigned long)PAL_BODY);
    body += 8;
    memset(body, 0, (size_t)PAL_BODY);
    for (i = 0; i < DK_SLOTS; i++) {
        put16(body + PAL_4PENS + 2 * i, pens[i]);
        put16(body + PAL_8PENS + 2 * i, pens[i]);
    }
    put16(body + PAL_4PENS + 2 * DK_SLOTS, 0xffff);
    put16(body + PAL_8PENS + 2 * DK_SLOTS, 0xffff);
    for (i = 0; i < ncolors; i++) {
        unsigned char *e = body + PAL_COLORS + 8 * i;
        put16(e, (unsigned)i);
        put16(e + 2, rgb16[i][0]);
        put16(e + 4, rgb16[i][1]);
        put16(e + 6, rgb16[i][2]);
    }
    put16(body + PAL_COLORS + 8 * ncolors, 0xffff);
    return DK_PALETTE_SIZE;
}

long desk_palette(const gtheme *t, int ncolors, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap)
{
    unsigned short rgb16[8][3];
    int i;
    if (ncolors != 4 && ncolors != 8)
        return 0;
    for (i = 0; i < ncolors; i++) {
        uc_u32 c = desk_role(t, i);
        rgb16[i][0] = (unsigned short)wide(c, 16);
        rgb16[i][1] = (unsigned short)wide(c, 8);
        rgb16[i][2] = (unsigned short)wide(c, 0);
    }
    return palette_fill(ncolors, pens, (const unsigned short (*)[3])rgb16, out, cap);
}

/* FLAT-PEN5-MAGENTA: the screen pen a palette.prefs ColorSpec entry sets. IPrefs (3.2, rig 11 depth 8, measured) puts entries 0-3 on
 * pens 0-3 and entries 4-7 on the LAST four pens, 2^depth - 8 + entry; at depth 3 that is the entry itself, at depth 2 there is none. */
#define PAL_ENTRIES 8
int desk_palette_pen(int entry, int depth)
{
    int n;
    if (entry < 0 || entry >= PAL_ENTRIES || depth < 1)
        return -1;
    n = 1 << (depth > 8 ? 8 : depth);
    if (entry < 4)
        return entry < n ? entry : -1;
    return n >= PAL_ENTRIES ? n - PAL_ENTRIES + entry : -1;
}

int desk_palette_entry_of(int pen, int depth)
{
    int e;
    for (e = 0; e < PAL_ENTRIES; e++)
        if (desk_palette_pen(e, depth) == pen)
            return e;
    return -1;
}

int desk_live_role_pens(int depth, int out[4])
{
    int p, n = 0, have = 1 << (depth > 8 ? 8 : depth < 1 ? 1 : depth);
    for (p = 0; p < DK_ROLES && p < have; p++)
        if (desk_palette_entry_of(p, depth) < 0 && n < 4)
            out[n++] = p;
    return n;
}

/* FLAT-LIVEBASE-SAVED: the colour a live role pen has when the user's palette rules. A pen some entry reaches takes the saved
 * palette.prefs colour (desk_palette_rgb, the per-depth map); a pen none reaches (4-7 deeper than 3) is never in the file, it keeps the
 * graphics.library default (measured rig 11, OS 3.2: 0000ff ff00ff 00ffff ffffff). Never a snapshot of the live screen: an open menu
 * holds those pens with its own colours. */
uc_u32 desk_live_baseline(const unsigned char *saved, long savedlen, int pen, int depth)
{
    static const uc_u32 stock[4] = {0x0000FFUL, 0xFF00FFUL, 0x00FFFFUL, 0xFFFFFFUL};
    long c = saved ? desk_palette_rgb(saved, savedlen, pen, depth) : -1;
    if (c >= 0)
        return (uc_u32)c;
    return pen >= 4 && pen < 8 ? stock[pen - 4] : 0;
}

uc_u32 desk_pen_colour(const gtheme *t, int pen, int flat_backdrop)
{
    if (pen == desk_backdrop_pen())
        return flat_backdrop ? desk_darker(t) : t->muted; /* flat: the dark tone, whether flat or the desktop-theme part writes the tile */
    return desk_role(t, pen);
}

int desk_live_follow(const gtheme *t, int flat, unsigned mask, int depth, int pens[4], uc_u32 rgb[4])
{
    int n, i;
    if (!t || !(flat || ((mask >> DT_PALETTE) & 1)))
        return 0;
    n = desk_live_role_pens(depth, pens);
    for (i = 0; i < n; i++)
        rgb[i] = desk_pen_colour(t, pens[i], flat);
    return n;
}

long desk_palette_live(const unsigned short *rgb4, int nlive, int depth, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap)
{
    unsigned short rgb16[8][3];
    int i, ncolors = desk_palette_pen(4, depth) >= 0 ? 8 : 4;
    if (!rgb4 || nlive <= 0)
        return 0;
    for (i = 0; i < ncolors; i++) {
        int p = desk_palette_pen(i, depth);
        unsigned v = p >= 0 && p < nlive ? rgb4[p] : 0;
        unsigned r = (v >> 8) & 15, g = (v >> 4) & 15, b = v & 15;
        rgb16[i][0] = (unsigned short)(r * 0x1111);
        rgb16[i][1] = (unsigned short)(g * 0x1111);
        rgb16[i][2] = (unsigned short)(b * 0x1111);
    }
    return palette_fill(ncolors, pens, (const unsigned short (*)[3])rgb16, out, cap);
}

long desk_palette_pens(const unsigned char *cur, long curlen, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap)
{
    long pos = DK_HDR, body, size;
    char id[4];
    int i;
    if (!cur || !is_prefs(cur, curlen) || curlen > cap)
        return 0;
    while (next_chunk(cur, curlen, &pos, id, &body, &size))
        if (!memcmp(id, "PALT", 4) && size >= PAL_COLORS) {
            memcpy(out, cur, (size_t)curlen);
            for (i = 0; i < DK_SLOTS; i++) {
                put16(out + body + PAL_4PENS + 2 * i, pens[i]);
                put16(out + body + PAL_8PENS + 2 * i, pens[i]);
            }
            return curlen;
        }
    return 0;
}

/* FLAT-PENS-STALE-FILE: 1 when the palette.prefs bytes in cur do not hold exactly the pens in both pen tables (all DK_SLOTS slots), or
 * are not a palette file with a PALT chunk; 0 when a write of pens would change no byte. Compared against the file as it is on disk. */
int desk_palette_pens_stale(const unsigned char *cur, long curlen, const unsigned char pens[DK_SLOTS])
{
    static unsigned char out[DK_PALETTE_SIZE * 4];
    long n = desk_palette_pens(cur, curlen, pens, out, sizeof out);
    return n != curlen || memcmp(cur, out, (size_t)n);
}

/* FLAT-BACKDROP-SAME-AS-WINDOW: the pen the ROOT tile is drawn in, a dedicated pen of its own. Chosen by measurement: a WBPattern tile has
 * PT_DEPTH_MAX (3) bitplanes, so it can only name pens 0-7. Pens 0-3 are the stock icon pens (and DK_DARKER = 1 is BLOCKPEN/BARTRIMPEN
 * in the live DrawInfo array, black in stock icons), pen 4 is the flat button fill (DK_SELECTION), and the flat DrawInfo map only ever
 * uses pens 0, 1 and 2. Pen 5 (DK_MUTED) is inside the tile range, outside 0-4 and in no DrawInfo slot, so recolouring it to the theme dark
 * changes nothing but the backdrop. Never BACKGROUNDPEN: with no pattern the root fills with it, the window interior pen. */
int desk_backdrop_pen_at(int depth)
{
    desk_pen_plan p;
    desk_pen_plan_for(depth, &p); /* L6: the plan is the one source */
    return p.backdrop;
}

/* FLAT-VIS-1: the screen pen the Workbench draws the ROOT tile's backdrop pen in. A 3-plane WBPattern tile names pens 0-7, and
 * Workbench maps them the way IPrefs maps palette.prefs entries (desk_palette_pen): 0-3 as they are, 4-7 to the last four pens
 * 2^depth - 8 + pen. On a Workbench deeper than 3 planes the backdrop (tile pen 7) is screen pen 2^depth-8+7 (255 on 8 planes), the
 * pen palette.prefs entry 7 sets, not pen 7. Below 3 planes the backdrop is the darker pen itself. */
int desk_backdrop_screen_pen(int depth)
{
    int p = desk_palette_pen(desk_backdrop_pen_at(depth), depth);
    return p >= 0 ? p : desk_backdrop_pen_at(depth);
}

int desk_backdrop_pen(void)
{
    return desk_backdrop_pen_at(PT_DEPTH_MAX);
}

/* the ColorSpec of a pen in a palette.prefs (entry desk_palette_entry_of(pen, depth)): its offset in cur, -1 when none */
static long palette_entry(const unsigned char *cur, long curlen, int pen, int depth)
{
    long pos = DK_HDR, body, size, e;
    int want = desk_palette_entry_of(pen, depth);
    char id[4];
    if (want < 0 || !cur || !is_prefs(cur, curlen))
        return -1;
    while (next_chunk(cur, curlen, &pos, id, &body, &size))
        if (!memcmp(id, "PALT", 4) && size >= PAL_COLORS) {
            for (e = body + PAL_COLORS; e + 8 <= body + size && get16(cur + e) != 0xffff; e += 8)
                if (get16(cur + e) == (unsigned)want)
                    return e;
            return -1;
        }
    return -1;
}

static void palette_put_rgb(unsigned char *e, uc_u32 c)
{
    put16(e + 2, wide(c, 16));
    put16(e + 4, wide(c, 8));
    put16(e + 6, wide(c, 0));
}

/* the colour (0xRRGGBB, the high byte of each 16-bit channel) of a pen in a palette.prefs, -1 when it has none */
long desk_palette_rgb(const unsigned char *cur, long curlen, int pen, int depth)
{
    long e = palette_entry(cur, curlen, pen, depth);
    if (e < 0)
        return -1;
    return ((long)cur[e + 2] << 16) | ((long)cur[e + 4] << 8) | (long)cur[e + 6];
}

/* cur with the colour of one pen replaced; the pen map and the other colours stay. 0 when cur has no such entry or does not fit out. */
long desk_palette_set_rgb(const unsigned char *cur, long curlen, int pen, int depth, uc_u32 c, unsigned char *out, long cap)
{
    long e = palette_entry(cur, curlen, pen, depth);
    if (e < 0 || curlen > cap)
        return 0;
    memcpy(out, cur, (size_t)curlen);
    palette_put_rgb(out + e, c);
    return curlen;
}

long desk_palette_theme(const unsigned char *cur, long curlen, const gtheme *t, int depth, int flat_backdrop, unsigned char *out, long cap)
{
    int p;
    if (palette_entry(cur, curlen, 0, depth) < 0 || curlen > cap)
        return 0;
    memcpy(out, cur, (size_t)curlen);
    for (p = 0; p < DK_ROLES; p++) {
        long e = palette_entry(out, curlen, p, depth);
        if (e >= 0)
            palette_put_rgb(out + e, desk_pen_colour(t, p, flat_backdrop));
    }
    /* FLAT-VIS-1: the entry that sets the screen pen the backdrop is drawn in (entry 7 = pen 255 on a deep screen) */
    {
        int bp = desk_backdrop_pen_at(depth), sp = desk_backdrop_screen_pen(depth);
        long e = sp != bp ? palette_entry(out, curlen, sp, depth) : -1;
        if (e >= 0)
            palette_put_rgb(out + e, desk_pen_colour(t, bp, flat_backdrop));
    }
    return curlen;
}

int desk_pens_sync(int flat, const unsigned short *live, int nlive, unsigned char pens[DK_SLOTS])
{
    desk_pens_for(flat, pens);
    if (!live || nlive <= 0)
        return 0;
    if (flat)
        return desk_pens_reset(live, nlive, 1);
    /* off: undo only what the switch wrote (the live map is the flat one); a system never flat keeps its own pens */
    return !desk_pens_reset(live, nlive, 1);
}

/* ---- R1.3, R1.20: wbpattern.prefs ---- */

#define PT_DEPTH_MAX 3
#define PT_PLANE 32 /* 16 rows of 16 pixels = 2 bytes */
#define PT_DATA (PT_DEPTH_MAX * PT_PLANE)
#define PT_BODY (24 + PT_DATA)
#define PT_CHUNK (8 + PT_BODY)

/* one flat 16x16 pattern into data[PT_DATA]: pen for every pixel */
static void pattern_fill(unsigned char *data, int depth, int pen)
{
    int p, y, x;
    memset(data, 0, PT_DATA);
    for (p = 0; p < depth; p++)
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++) {
                if ((pen >> p) & 1)
                    data[p * PT_PLANE + y * 2 + x / 8] |= (unsigned char)(0x80 >> (x % 8));
            }
}

static void pattern_chunk(unsigned char *at, int which, int depth, const unsigned char *data)
{
    memset(at, 0, PT_CHUNK);
    memcpy(at, "PTRN", 4);
    be32_put(at + 4, (unsigned long)PT_BODY);
    put16(at + 8 + 16, (unsigned)which);
    put16(at + 8 + 18, 1); /* WBPF_PATTERN */
    at[8 + 21] = (unsigned char)depth;
    put16(at + 8 + 22, PT_DATA);
    memcpy(at + 8 + 24, data, PT_DATA);
}

/* the SCREEN chunk of the file now on disk, or 0 */
static const unsigned char *prev_screen(const unsigned char *prev, long prevlen)
{
    long pos = DK_HDR, body, size;
    char id[4];
    if (!prev || !is_prefs(prev, prevlen))
        return 0;
    while (next_chunk(prev, prevlen, &pos, id, &body, &size))
        if (!memcmp(id, "PTRN", 4) && size == PT_BODY && get16(prev + body + 16) == 2)
            return prev + body - 8;
    return 0;
}

long desk_wbpattern(const gtheme *t, int depth, const unsigned char *prev, long prevlen,
                    unsigned char *out, long cap)
{
    unsigned char data[PT_DATA], *at;
    const unsigned char *keep;
    (void)t;
    if (cap < DK_WBPATTERN_SIZE)
        return 0;
    if (depth > PT_DEPTH_MAX)
        depth = PT_DEPTH_MAX;
    if (depth < 1)
        depth = 1;
    at = iff_header(out, DK_WBPATTERN_SIZE);
    /* ROOT: one solid darker tone, no dots (dark_background is no pen under OD13) */
    pattern_fill(data, depth, desk_backdrop_pen_at(depth));
    pattern_chunk(at, 0, depth, data);
    /* DRAWER: the panel tone MT_BG, which is pen 0 (OD7) */
    pattern_fill(data, depth, DK_BG);
    pattern_chunk(at + PT_CHUNK, 1, depth, data);
    /* SCREEN: the chunk Wbpattern writes for "none" (flags 1, depth 0, 96 zero bytes), or the user's own */
    keep = prev_screen(prev, prevlen);
    if (keep) {
        memcpy(at + 2 * PT_CHUNK, keep, PT_CHUNK);
    } else {
        memset(data, 0, PT_DATA);
        pattern_chunk(at + 2 * PT_CHUNK, 2, 0, data);
    }
    return DK_WBPATTERN_SIZE;
}

/* ---- R1.21a: pointer.prefs ---- */

static void rgb_put(unsigned char *p, uc_u32 c)
{
    p[0] = (unsigned char)(c >> 16);
    p[1] = (unsigned char)(c >> 8);
    p[2] = (unsigned char)c;
}

long desk_pointer(const gtheme *t, unsigned char *buf, long len)
{
    long pos = DK_HDR, body, size, n = 0;
    char id[4];
    if (!is_prefs(buf, len))
        return -1;
    while (next_chunk(buf, len, &pos, id, &body, &size)) {
        unsigned depth, ncol, i;
        uc_u32 col[3];
        if (memcmp(id, "PNTR", 4))
            continue;
        if (size < 32)
            return -1;
        depth = get16(buf + body + 24);
        ncol = (1u << depth) - 1;
        if (depth > 8 || size < 32 + 3L * (long)ncol)
            return -1;
        col[0] = t->fg;
        col[1] = t->bg;
        col[2] = t->accent;
        for (i = 0; i < ncol && i < 3; i++)
            rgb_put(buf + body + 32 + 3 * i, col[i]);
        n++;
    }
    return n;
}

/* ---- R1.22a, R1.4: font.prefs ---- */

#define FP_TYPE 14
#define FP_FRONT 16
#define FP_BACK 17
#define FP_DRAW 18
#define FP_SPECIAL 19
#define FP_NAME 28
#define FP_NAMESIZE 128
#define FP_BODY 156
#define FP_WBFONT 0

/* the body offset of the WBFONT chunk, -1 when none, -2 when buf is no font.prefs */
static long wbfont_body(const unsigned char *buf, long len)
{
    long pos = DK_HDR, body, size;
    char id[4];
    if (!is_prefs(buf, len))
        return -2;
    while (next_chunk(buf, len, &pos, id, &body, &size))
        if (!memcmp(id, "FONT", 4) && size == FP_BODY && get16(buf + body + FP_TYPE) == FP_WBFONT)
            return body;
    return -1;
}

long desk_font_pens(unsigned char *buf, long len)
{
    long b = wbfont_body(buf, len);
    if (b == -2)
        return -1;
    if (b < 0)
        return 0;
    buf[b + FP_FRONT] = DK_FG;     /* the label */
    buf[b + FP_BACK] = DK_DARKER;  /* the backdrop */
    buf[b + FP_DRAW] = 0;          /* JAM1 */
    buf[b + FP_SPECIAL] = 1;       /* outline */
    return 1;
}

long desk_font_name(const gtheme *t, unsigned char *buf, long len, desk_font_exists_fn exists, void *ctx)
{
    long b = wbfont_body(buf, len);
    size_t n = strlen(t->font);
    if (b == -2)
        return -1;
    if (b < 0 || n == 0 || n >= FP_NAMESIZE || !exists || !exists(ctx, t->font))
        return 0;
    memset(buf + b + FP_NAME, 0, FP_NAMESIZE);
    memcpy(buf + b + FP_NAME, t->font, n);
    return 1;
}

/* ---- R1.6: contrast guard ---- */

static int refuse(char *why, int cap, const char *msg)
{
    size_t n = strlen(msg);
    if (cap > 0) {
        if (n >= (size_t)cap)
            n = (size_t)cap - 1;
        memcpy(why, msg, n);
        why[n] = 0;
    }
    return 1;
}

int desk_check(const gtheme *t, char *why, int cap)
{
    uc_u32 darker = desk_darker(t);
    int black = desk_luma(t->bg) == 0;
    if (desk_contrast(t->fg, t->bg) < DK_FLOOR_TEXT)
        return refuse(why, cap, "The text colour is too close to the background.");
    if (desk_contrast(t->bg, t->accent) < DK_FLOOR_FILL)
        return refuse(why, cap, "The title bar text is too close to the title bar colour.");
    if (desk_contrast(t->fg, darker) < DK_FLOOR_TEXT)
        return refuse(why, cap, "The icon labels are too close to the backdrop.");
    if (!black && desk_contrast(darker, t->bg) < DK_STEP_DARK)
        return refuse(why, cap, "The backdrop is too close to the window colour.");
    if (cap > 0)
        why[0] = 0;
    return 0;
}

int desk_plan(unsigned mask, const gtheme *t, char *why, int cap)
{
    if (!mask)
        return 0;
    if (desk_check(t, why, cap))
        return -1;
    return (int)mask;
}

/* R1.22a base: the font.prefs of a machine that has none (a fresh Workbench writes it only on the first Font save):
 * three FONT chunks (WBFONT, SYSFONT, SCREENFONT), topaz.font 8, pens 1/0, JAM2, the layout of the probe note */
long desk_font_default(unsigned char *out, long cap)
{
    long n = 518;
    int k;
    if (cap < n)
        return 0;
    memset(out, 0, (size_t)n);
    memcpy(out, "FORM", 4);
    out[7] = 0xfe, out[6] = 0x01;
    memcpy(out + 8, "PREFPRHD", 8);
    out[19] = 6;
    for (k = 0; k < 3; k++) {
        unsigned char *c = out + 26 + k * 164;
        memcpy(c, "FONT", 4);
        c[7] = 156;
        c[8 + 15] = (unsigned char)k;
        c[8 + 16] = 1;
        c[8 + 18] = 1;
        c[8 + 25] = 8;
        c[8 + 27] = 1;
        memcpy(c + 8 + 28, "topaz.font", 11);
    }
    return n;
}

/* ---- FLAT-P1-2: icontrol.prefs ---- */

#define IC_FLAGS 20L   /* ic_Flags (ULONG) in the ICTL body */
#define IC_VERSION 28L /* ic_Version (UWORD), ic_VersionMagic (UWORD) at 30 */
#define IC_GEOM 34L    /* ic_GUIGeometry[4] */
#define IC_VERSIONED 0x80000000UL /* ICB_VERSIONED */

void desk_geometry(int thin, unsigned char geometry[4])
{
    memset(geometry, 0, 4);
    if (thin)
        geometry[1] = 0xFF;
}

long desk_icontrol(const unsigned char *cur, long curlen, const unsigned char geometry[4], unsigned char *out, long cap)
{
    long pos = DK_HDR, o = 0, n, body, size, ictl_at = -1;
    char id[4];
    unsigned char *ic;
    if (cur && curlen > 0 && !is_prefs(cur, curlen))
        return 0;
    if (!cur || curlen <= 0) {
        if (cap < DK_HDR + 8 + DK_ICTL_SIZE)
            return 0;
        iff_header(out, DK_HDR + 8 + DK_ICTL_SIZE);
        memcpy(out + DK_HDR, "ICTL", 4);
        be32_put(out + DK_HDR + 4, (unsigned long)DK_ICTL_SIZE);
        memset(out + DK_HDR + 8, 0, (size_t)DK_ICTL_SIZE);
        n = DK_HDR + 8 + DK_ICTL_SIZE;
    } else {
        /* copy the file, the ICTL chunk grown to the versioned size; every other chunk goes through unchanged */
        long end = curlen;
        if (cap < curlen + DK_ICTL_SIZE)
            return 0;
        memcpy(out, cur, (size_t)DK_HDR);
        o = DK_HDR;
        while (next_chunk(cur, end, &pos, id, &body, &size)) {
            long start = body - 8, chunk_end = body + size + (size & 1);
            if (chunk_end > end)
                chunk_end = end;
            if (!memcmp(id, "ICTL", 4) && ictl_at < 0) {
                long grown = size < DK_ICTL_SIZE ? DK_ICTL_SIZE : size;
                memcpy(out + o, "ICTL", 4);
                be32_put(out + o + 4, (unsigned long)grown);
                memset(out + o + 8, 0, (size_t)(grown + (grown & 1)));
                memcpy(out + o + 8, cur + body, (size_t)size);
                ictl_at = o + 8;
                o += 8 + grown + (grown & 1);
            } else {
                memcpy(out + o, cur + start, (size_t)(chunk_end - start));
                o += chunk_end - start;
            }
        }
        if (ictl_at < 0) {
            memcpy(out + o, "ICTL", 4);
            be32_put(out + o + 4, (unsigned long)DK_ICTL_SIZE);
            memset(out + o + 8, 0, (size_t)DK_ICTL_SIZE);
            ictl_at = o + 8;
            o += 8 + DK_ICTL_SIZE;
        }
        be32_put(out + 4, (unsigned long)(o - 8));
        n = o;
    }
    if (ictl_at < 0)
        ictl_at = DK_HDR + 8;
    ic = out + ictl_at;
    be32_put(ic + IC_FLAGS, be32_get(ic + IC_FLAGS) | IC_VERSIONED);
    put16(ic + IC_VERSION, 2);
    put16(ic + IC_VERSION + 2, 0);
    memcpy(ic + IC_GEOM, geometry, 4);
    return n;
}

/* 1 when cur is an icontrol.prefs whose GUI geometry is not stock (an earlier thin apply of ours wrote it) */
static int icontrol_ours(const unsigned char *cur, long len)
{
    long pos = DK_HDR, body, size;
    char id[4];
    if (!cur || len <= 0 || !is_prefs(cur, len))
        return 0;
    while (next_chunk(cur, len, &pos, id, &body, &size))
        if (!memcmp(id, "ICTL", 4) && size >= IC_GEOM + 4 && body + IC_GEOM + 4 <= len)
            return (cur[body + IC_GEOM] | cur[body + IC_GEOM + 1] | cur[body + IC_GEOM + 2] | cur[body + IC_GEOM + 3]) != 0;
    return 0;
}

int desk_icontrol_decide(int thin, const unsigned char *cur, long curlen, int have_snap, int *snapshot)
{
    int ours = icontrol_ours(cur, curlen);
    *snapshot = 0;
    if (thin) {
        if (ours)
            return DK_IC_NONE; /* already written: no second Workbench reset, and the snapshot keeps the pre-thin file */
        *snapshot = 1;
        return DK_IC_WRITE;
    }
    return ours && have_snap ? DK_IC_RESTORE : DK_IC_NONE;
}


/* ---- AM-ICON-NOBORDER: the Workbench icon border (Prefs/Workbench "Border Size", WBNC chunk, struct WorkbenchPrefs.wbp_Borderless) ---- */
#define WBNC_BORDERLESS 20L /* 3 ULONG + struct Rectangle (4 WORD) = 20, then BOOL (a WORD, NDK 3.2 exec/types.h:167) */
#define WBNC_MIN 22L
#define WBNC_GAUGE 38L      /* WorkbenchExtendedPrefs after the 30-byte WorkbenchPrefs: IconMemoryType ULONG 30, LockPens 34, DisableTitleBar 36, DisableVolumeGauge 38 */
#define WBNC_GAUGE_MIN 40L
#define WBNC_STOCK_BODY 50L

static long wbnc_find(const unsigned char *cur, long curlen, long *size)
{
    long pos = DK_HDR, body;
    char id[4];
    if (!cur || curlen <= 0 || !is_prefs(cur, curlen))
        return -1;
    while (next_chunk(cur, curlen, &pos, id, &body, size))
        if (!memcmp(id, "WBNC", 4))
            return *size >= WBNC_MIN ? body : -1;
    return -1;
}

int desk_wbborder_value(const unsigned char *cur, long curlen)
{
    long size, body = wbnc_find(cur, curlen, &size);
    return body < 0 ? -1 : (int)(get16(cur + body + WBNC_BORDERLESS) != 0);
}

/* The 3.2 Prefs/Workbench editor's own file for stock settings, measured on a fresh 3.2 rig: Use with nothing changed writes ENV:Sys/Workbench.prefs,
 * 84 bytes (the stock install has no such file). Body: stack 4096, restart 3 s, precision 16, emboss rect -3 -3 3 3, border word, name length 25,
 * NewIcons, colour icons, then the V45/V47 extension words. */
static const unsigned char wbnc_stock[WBNC_STOCK_BODY] = {
    0, 0, 0x10, 0, 0, 0, 0, 3, 0, 0, 0, 0x10, 0xff, 0xfd, 0xff, 0xfd, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0x19, 0, 1, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x32, 0, 0, 0, 0, 0, 0, 0, 0   /* TitleUpdateDelay 50 ms is the word at 40 (measured file, rig 12) */
};

static long wbnc_stock_file(int borderless, unsigned char *out, long cap)
{
    unsigned char *body;
    long total = DK_HDR + 8 + WBNC_STOCK_BODY;
    if (cap < total)
        return 0;
    body = iff_header(out, total);
    memcpy(body, "WBNC", 4);
    be32_put(body + 4, (unsigned long)WBNC_STOCK_BODY);
    memcpy(body + 8, wbnc_stock, WBNC_STOCK_BODY);
    put16(body + 8 + WBNC_BORDERLESS, borderless != 0);
    return total;
}

long desk_wbborder(int flat, const unsigned char *cur, long curlen, int saved, int *save_out, unsigned char *out, long cap)
{
    long size, body = wbnc_find(cur, curlen, &size);
    int now, want;
    *save_out = -1;
    if (saved == DESK_WB_CREATED) {         /* we made the file: off puts the stock border back in the CURRENT file (the user's other tweaks stay) and never deletes it */
        *save_out = -2;
        if (flat || body < 0 || curlen > cap)
            return 0;
        memcpy(out, cur, (size_t)curlen);
        put16(out + body + WBNC_BORDERLESS, 0);
        return curlen;
    }
    if (curlen <= 0) {                      /* no Workbench.prefs: the editor's stock file, border off */
        if (!flat)
            return 0;
        *save_out = DESK_WB_CREATED;
        return wbnc_stock_file(1, out, cap);
    }
    if (body < 0 || curlen > cap)
        return 0;
    now = get16(cur + body + WBNC_BORDERLESS) != 0;
    if (flat) {
        if (now)
            return 0;                       /* already no border: nothing to remember, no second reset */
        *save_out = now;
        want = 1;
    } else {
        if (saved < 0)
            return 0;                       /* we never changed it */
        *save_out = -2;                     /* forget the saved value */
        if (now == saved)
            return 0;
        want = saved;
    }
    memcpy(out, cur, (size_t)curlen);
    out[body + WBNC_BORDERLESS] = 0;
    out[body + WBNC_BORDERLESS + 1] = (unsigned char)want;
    return curlen;
}

/* WB-VOLUME-GAUGE-OFF: wbe_DisableVolumeGauge, the BOOL at body offset 38 of the WBNC chunk (V45 extension, 3.2 Prefs/Workbench "Disable volume gauge") */
static long wbgauge_find(const unsigned char *cur, long curlen)
{
    long size, body = wbnc_find(cur, curlen, &size);
    return body >= 0 && size >= WBNC_GAUGE_MIN && body + WBNC_GAUGE_MIN <= curlen ? body : -1;
}

int desk_wbgauge_value(const unsigned char *cur, long curlen)
{
    long body = wbgauge_find(cur, curlen);
    return body < 0 ? -1 : (int)(get16(cur + body + WBNC_GAUGE) != 0);
}

long desk_wbgauge(int flat, const unsigned char *cur, long curlen, int saved, int *save_out, unsigned char *out, long cap)
{
    long body = wbgauge_find(cur, curlen);
    int now, want;
    *save_out = -1;
    if (body < 0 || curlen > cap)           /* no file, 3.1, or a WBNC chunk without the V45 words: nothing to do */
        return 0;
    now = get16(cur + body + WBNC_GAUGE) != 0;
    if (flat) {
        if (now)
            return 0;                       /* already disabled by the user: nothing to remember or restore */
        *save_out = now;
        want = 1;
    } else {
        if (saved < 0)
            return 0;
        *save_out = -2;
        if (now == saved)
            return 0;
        want = saved;
    }
    memcpy(out, cur, (size_t)curlen);
    out[body + WBNC_GAUGE] = 0;
    out[body + WBNC_GAUGE + 1] = (unsigned char)want;
    return curlen;
}

/* ---- D-TGT, R1.16a ---- */

/* write order: the table order. Files: the ENV: copy is what the OS reads, the ENVARC: copy survives a reboot. */
static const desk_target targets[DT_COUNT] = {
    { "palette", "desktop-palette", { "ENVARC:Sys/palette.prefs", "ENV:Sys/palette.prefs" } },
    { "wbpattern", "desktop-backdrop", { "ENVARC:Sys/wbpattern.prefs", "ENV:Sys/wbpattern.prefs" } },
    { "pointer", "desktop-pointer", { "ENVARC:Sys/pointer.prefs", "ENV:Sys/pointer.prefs" } },
    { "font", "desktop-labels", { "ENVARC:Sys/font.prefs", "ENV:Sys/font.prefs" } },
    { "icontrol", "flat-border-size", { "ENVARC:Sys/icontrol.prefs", "ENV:Sys/icontrol.prefs" } },
};

const desk_target *desk_target_get(int target)
{
    return target >= 0 && target < DT_COUNT ? &targets[target] : 0;
}

int desk_target_by_name(const char *name)
{
    int i;
    for (i = 0; i < DT_COUNT; i++)
        if (!strcmp(name, targets[i].name))
            return i;
    return -1;
}

static int collect(unsigned mask, int reverse, const char **list, int cap)
{
    int n = 0, i, f;
    for (i = 0; i < DT_COUNT; i++) {
        int t = reverse ? DT_COUNT - 1 - i : i;
        if (!(mask & (1u << t)))
            continue;
        for (f = 0; f < 2; f++)
            if (targets[t].files[f] && n < cap)
                list[n++] = targets[t].files[f];
    }
    return n;
}

int desk_snapshot(unsigned mask, const char **list, int cap)
{
    return collect(mask, 0, list, cap);
}

int desk_restore(unsigned mask, const char **list, int cap)
{
    return collect(mask, 1, list, cap);
}

int desk_persist_pairs(unsigned mask, int wbprefs, const char **env, const char **arc, int cap)
{
    int n = 0, t;
    for (t = 0; t < DT_COUNT; t++)
        if (t != DT_ICONTROL && (mask & (1u << t)) && n < cap) {
            arc[n] = targets[t].files[0];
            env[n++] = targets[t].files[1];
        }
    if (wbprefs && n < cap) {
        arc[n] = DESK_WBPREFS_ARC;
        env[n++] = DESK_WBPREFS_ENV;
    }
    return n;
}

unsigned desk_off_mask(unsigned saved, unsigned desk_mask)
{
    return saved & ~desk_mask & ~(1u << DT_ICONTROL);
}

int desk_env_fix(int want_exists, int got_exists, int equal)
{
    if (!want_exists)
        return got_exists ? DESK_FIX_DELETE : DESK_FIX_NONE;
    return got_exists && equal ? DESK_FIX_NONE : DESK_FIX_COPY;
}

int desk_env_fix_run(const desk_fix_ops *ops, void *ud, int tries)
{
    int a, act;
    for (a = 0;; a++) {
        act = desk_env_fix(ops->want(ud), ops->got(ud), ops->equal(ud));
        if (act == DESK_FIX_NONE)
            return a;
        if (a == tries)
            return -1;
        if (a)
            ops->pause(ud);
        if (act == DESK_FIX_COPY)
            ops->copy(ud);
        else
            ops->del(ud);
    }
}

int desk_copy_name(const char *dir, const char *file, char *out, int cap)
{
    int n = (int)strlen(dir);
    if (n + 1 + (int)strlen(file) + 1 > cap)
        return 0;
    memcpy(out, dir, (size_t)n);
    out[n++] = '/';
    for (; *file; file++)
        out[n++] = (*file == ':' || *file == '/') ? '-' : *file;
    out[n] = 0;
    return 1;
}

int desk_pending_files(unsigned mask, int wbprefs, const char **list, int cap)
{
    int n = desk_snapshot(mask, list, cap);
    if (wbprefs && n < cap)
        list[n++] = DESK_WBPREFS_ENV;
    return n;
}

#define MARKER_HEAD "desk-pending 1\n"

long desk_marker_make(unsigned mask, char *out, long cap)
{
    long n = (long)strlen(MARKER_HEAD);
    int i;
    if (cap < n + 1)
        return 0;
    memcpy(out, MARKER_HEAD, (size_t)n);
    for (i = 0; i < DT_COUNT; i++) {
        long l = (long)strlen(targets[i].name);
        if (!(mask & (1u << i)))
            continue;
        if (n + l + 2 > cap)
            return 0;
        memcpy(out + n, targets[i].name, (size_t)l);
        out[n + l] = '\n';
        n += l + 1;
    }
    out[n] = 0;
    return n;
}

unsigned desk_marker_parse(const char *buf, long len)
{
    long hl = (long)strlen(MARKER_HEAD), pos;
    unsigned mask = 0;
    if (len < hl || memcmp(buf, MARKER_HEAD, (size_t)hl))
        return 0;
    pos = hl;
    while (pos < len) {
        char name[16];
        long n = 0;
        int t;
        while (pos + n < len && buf[pos + n] != '\n' && n < (long)sizeof name - 1) {
            name[n] = buf[pos + n];
            n++;
        }
        name[n] = 0;
        t = desk_target_by_name(name);
        if (t >= 0)
            mask |= 1u << t;
        pos += n;
        while (pos < len && buf[pos] != '\n')
            pos++;
        pos++;
    }
    return mask;
}

int desk_ver_major(const unsigned char *bin, long len)
{
    long i, j;
    for (i = 0; bin && i + 6 < len; i++) {
        if (memcmp(bin + i, "$VER: ", 6))
            continue;
        j = i + 6;
        while (j < len && bin[j] > ' ') /* the program name */
            j++;
        while (j < len && bin[j] == ' ')
            j++;
        if (j < len && bin[j] >= '0' && bin[j] <= '9') {
            int v = 0;
            while (j < len && bin[j] >= '0' && bin[j] <= '9' && v < 10000)
                v = v * 10 + (bin[j++] - '0');
            return v;
        }
    }
    return -1;
}

int desk_pointer_editor_ok(const unsigned char *bin, long len)
{
    return desk_ver_major(bin, len) >= 47;
}

int desk_palette_editor_ok(const unsigned char *bin, long len)
{
    return desk_ver_major(bin, len) >= 47;
}

unsigned desk_gate_mask(unsigned mask, int palette_ok, int *ws_reset)
{
    if (!palette_ok)
        mask &= ~(1u << DT_PALETTE);
    /* IControl's ic_GUIGeometry is read on 3.2 only (FLAT-R1: 3.1 accepts the file and changes nothing), so it follows the palette gate; every
     * change reopens the Workbench screen, so the caller closes its windows around it (the same reset as a wbpattern write on 3.1) */
    if (!palette_ok)
        mask &= ~(1u << DT_ICONTROL);
    /* only below 47 does a wbpattern write ask Intuition for a Workbench reset (3.2 measured: a WBPattern USE with a window open posts no requester) */
    *ws_reset = (!palette_ok && (mask & (1u << DT_WBPATTERN)) != 0) || (mask & (1u << DT_ICONTROL)) != 0;
    return mask;
}

/* ---- DESK-PREVIEW-PALETTE: the picker's highlight previews the Workbench colours ---- */

int desk_preview(desk_pv *p, const gtheme *t)
{
    int i;
    if (!p || !p->get || !p->set || p->npens < 1 || p->npens > DK_ROLES)
        return 0;
    if (!t) {                       /* restore */
        if (!p->have)
            return 0;
        for (i = 0; i < p->npens; i++)
            p->set(p->ctx, i, p->saved[i]);
        p->have = 0;
        return 1;
    }
    if (desk_check(t, 0, 0))
        return 0;
    if (!p->have) {
        for (i = 0; i < p->npens; i++)
            p->get(p->ctx, i, &p->saved[i]);
        p->have = 1;
    }
    for (i = 0; i < p->npens; i++)
        p->set(p->ctx, i, desk_role(t, i));
    return 1;
}

void desk_confirm_init(desk_confirm *c)
{
    c->pending = 0;
    c->mask = 0;
    c->deadline = 0;
}

void desk_confirm_begin(desk_confirm *c, long now, unsigned mask)
{
    c->mask = c->pending ? (c->mask | mask) : mask;
    c->pending = 1;
    c->deadline = now + DESK_CONFIRM_SECS;
}

int desk_confirm_tick(desk_confirm *c, long now)
{
    if (!c->pending || now < c->deadline)
        return DC_WAIT;
    c->pending = 0;
    return DC_REVERT_TIMEOUT;
}

int desk_confirm_answer(desk_confirm *c, int keep)
{
    if (!c->pending)
        return DC_WAIT;
    c->pending = 0;
    return keep ? DC_KEEP : DC_REVERT_USER;
}

long desk_confirm_left(const desk_confirm *c, long now)
{
    return c->pending && now < c->deadline ? c->deadline - now : 0;
}
