/* The Amiga renderer; see amiga_render.h. */
#include "amiga_render.h"
#include "painter.h"
#include "vtinput.h"
#ifdef VTCON_PROF
#define TimerBase vtwin_timer
extern struct Device *vtwin_timer;
#include <proto/timer.h>
ULONG vr_prof[9]; /* PROF=1: EClock ticks in painter_run, WaitBlit; painter calls; ticks in fill(), fills;
                   * ticks in the composed frames' blits (planar: copies), blits; ticks waiting for the
                   * beam to leave the rows a planar copy writes (WaitTOF), waits */
#endif
#include <string.h>

#include <exec/memory.h>
#include <graphics/gfxmacros.h>
#include <graphics/scale.h>
#include <graphics/rastport.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>

extern struct GfxBase *GfxBase;

#if defined(VTCON_PROF) || defined(VR_HOST_TEST)
#define VR_CPROF 1
/* The cursor pass's graphics.library calls (PROF=1 and the host test; ledger "cursor pass"). vr_gc counts every
 * Text, RectFill and pen set the renderer makes (the macros wrap the library calls below this line);
 * cursor_draw adds the change over one draw to vr_cp, while the pass runs (vr_cp_pass, set by vtwin_render
 * around its two cursor blocks). */
ULONG vr_gc[4]; /* Text, RectFill, pen sets (SetAPen, SetBPen, SetABPenDrMd), draw_rows calls by the cursor */
ULONG vr_cp[12]; /* off: Text, RectFill, pen, draw_rows; on: the same (4-7); cursor draws; draws of an unchanged cell;
                  * cursor off and on draws made outside the pass blocks (inside the flush: scroll, blink, colour change) */
ULONG vr_cp_pass;
#endif

/* cybergraphics.library V41 WriteLUTPixelArray (LVO -198; AROS
 * cybergraphics.conf, the CGX/P96 SDK's fd): 8-bit indices through a
 * table of 0x00RRGGBB, on true-colour screens. No SDK header here, so
 * the call is declared as vbcc's inline. */
#define VR_CTABFMT_XRGB8 0
#ifdef VR_HOST_TEST /* the host test (tests/test_render.c) links a fake of it */
LONG vr_cgx_write_lut(struct Library *, APTR src, UWORD sx, UWORD sy, UWORD smod, struct RastPort *rp,
                      APTR ctab, UWORD dx, UWORD dy, UWORD w, UWORD h, UBYTE fmt);
ULONG vr_cgx_fill(struct Library *, struct RastPort *rp, UWORD x, UWORD y, UWORD w, UWORD h, ULONG argb);
#else
LONG vr_cgx_write_lut(__reg("a6") struct Library *, __reg("a0") APTR src, __reg("d0") UWORD sx,
                      __reg("d1") UWORD sy, __reg("d2") UWORD smod, __reg("a1") struct RastPort *rp,
                      __reg("a2") APTR ctab, __reg("d3") UWORD dx, __reg("d4") UWORD dy,
                      __reg("d5") UWORD w, __reg("d6") UWORD h, __reg("d7") UBYTE fmt) = "\tjsr\t-198(a6)";
/* cybergraphics.library V41 FillPixelArray (LVO -150): a rectangle in a
 * 0xAARRGGBB colour, no pen, through the RastPort's layer. */
ULONG vr_cgx_fill(__reg("a6") struct Library *, __reg("a1") struct RastPort *rp, __reg("d0") UWORD x,
                  __reg("d1") UWORD y, __reg("d2") UWORD w, __reg("d3") UWORD h,
                  __reg("d4") ULONG argb) = "\tjsr\t-150(a6)";
#endif

#include "pen_rule.h"

#ifdef VR_CPROF
#ifdef VR_HOST_TEST /* the fake's functions */
#define VR_GFX(name) name
#else /* the inline pragmas' macros: #undef, then the same call under the count */
#undef Text
#undef RectFill
#undef SetAPen
#undef SetBPen
#undef SetABPenDrMd
#define VR_GFX(name) __##name
#endif
#ifdef VR_HOST_TEST
#define VR_GB
#else
#define VR_GB GfxBase,
#endif
#define Text(rp, s, n) (vr_gc[0]++, VR_GFX(Text)(VR_GB(rp), (s), (n)))
#define RectFill(rp, a, b, c, d) (vr_gc[1]++, VR_GFX(RectFill)(VR_GB(rp), (a), (b), (c), (d)))
#define SetAPen(rp, p) (vr_gc[2]++, VR_GFX(SetAPen)(VR_GB(rp), (p)))
#define SetBPen(rp, p) (vr_gc[2]++, VR_GFX(SetBPen)(VR_GB(rp), (p)))
#define SetABPenDrMd(rp, a, b, m) (vr_gc[2]++, VR_GFX(SetABPenDrMd)(VR_GB(rp), (a), (b), (m)))
/* a true-colour screen fills with FillPixelArray: a RectFill for the count */
#define vr_cgx_fill(lb, rp, x, y, w, h, c) (vr_gc[1]++, vr_cgx_fill(lb, rp, x, y, w, h, c))
#endif
#include "pen_match.h"

#define RUN_MAX 256

/* ---- palette ------------------------------------------------------------ */


/* the screen's ObtainBestPen for pen_rule.h: exact (a free entry gets the colour) or the nearest */
static long best_pen(void *u, unsigned long rgb, int exact)
{
    static const struct TagItem exact_tags[] = {
        { OBP_Precision, (ULONG)PRECISION_EXACT },
        { OBP_FailIfBad, TRUE },
        { TAG_DONE, 0 }
    };
    static const struct TagItem near_tags[] = {
        { OBP_Precision, PRECISION_IMAGE },
        { TAG_DONE, 0 }
    };
    ULONG cr = (rgb >> 16) & 0xFF, cg = (rgb >> 8) & 0xFF, cb = rgb & 0xFF;
    return ObtainBestPenA((struct ColorMap *)u, cr * 0x01010101UL, cg * 0x01010101UL, cb * 0x01010101UL,
                          (struct TagItem *)(exact ? exact_tags : near_tags));
}

/* A palette colour's pen: exact while the map has room, the nearest when it is full (pen_rule.h). One pen to
 * release either way. */
static LONG obtain(vr_render *r, ULONG rgb)
{
    if (!r->cm)
        return -1;
    return (LONG)vr_obtain_rule(best_pen, r->cm, rgb);
}

/* An ink is what a cell is drawn with: a screen pen, or on a true-colour
 * screen VR_INK_RGB | 0xRRGGBB, loaded into a scratch pen just before the
 * drawing call that uses it (ink_pen). */
#define VR_INK_RGB 0x80000000UL
/* An area's ink on a screen of few pens (W-T7): pens a (bits 0-7) and b
 * (8-15) in the 8 x 8 ordered pattern of level lev (16-21, 1..63: pen b on
 * lev / 64 of the pixels), cell-anchored (pat_bits). Only fills take it
 * (fill, block_run); anything that draws with one pen takes the pen that
 * covers more of the pattern (ink_solid). */
#define VR_INK_DITH 0x00400000UL
#define IS_DITH(ink) (((ink) & (VR_INK_RGB | VR_INK_DITH)) == VR_INK_DITH)
#define DITH_A(ink) ((UBYTE)((ink) & 0xFF))
#define DITH_B(ink) ((UBYTE)(((ink) >> 8) & 0xFF))
#define DITH_LEV(ink) ((int)(((ink) >> 16) & 63))

static ULONG ink_solid(ULONG ink)
{
    if (!IS_DITH(ink))
        return ink;
    return DITH_LEV(ink) < PM_LEVELS / 2 ? DITH_A(ink) : DITH_B(ink);
}

/* A direct colour on a true-colour screen whose colour map is used up (no scratch pen to load, no pen to
 * obtain exact), where a pen is wanted all the same -- a glyph, a shade pattern: the pen the screen finds
 * nearest, held until vr_free (the exact_key table, key 0x02RRGGBB). The areas of the colour are not
 * drawn with it: fill() gives them the colour itself through FillPixelArray. */
static UBYTE nearest_pen(vr_render *r, ULONG rgb)
{
    ULONG key = rgb | 0x02000000UL;
    int h = (int)(((rgb * 2654435761UL) >> 24) & (VR_EXACT_SLOTS - 1)), i;
    LONG p;
    for (i = 0; i < VR_EXACT_SLOTS; i++, h = (h + 1) & (VR_EXACT_SLOTS - 1)) {
        if (r->exact_key[h] == key)
            return r->exact_pen[h];
        if (!r->exact_key[h])
            break;
    }
    if (i == VR_EXACT_SLOTS || !r->cm)
        return r->pen_default_fg;
    p = ObtainBestPenA(r->cm, ((rgb >> 16) & 0xFF) * 0x01010101UL, ((rgb >> 8) & 0xFF) * 0x01010101UL,
                       (rgb & 0xFF) * 0x01010101UL, 0);
    if (p < 0)
        return r->pen_default_fg;
    r->exact_key[h] = key;
    r->exact_pen[h] = (UBYTE)p;
    return (UBYTE)p;
}

/* The pen that draws ink as pen A (slot 0) or pen B (slot 1). On a screen
 * of more than 8 bits a pen is only the colour of the next drawing, not a
 * palette entry: pixels drawn earlier keep their colour when it changes
 * (the method Phantasm described), so one exclusive pen per slot shows any
 * number of colours. */
static UBYTE ink_pen(vr_render *r, ULONG ink, int slot)
{
    ULONG rgb;
    ink = ink_solid(ink);
    if (!(ink & VR_INK_RGB)) {
        if ((UBYTE)ink & ~r->mask) {
            /* a pen with a plane not in use yet: those planes hold zeros
             * everywhere, so drawing may simply start to include them */
            r->mask |= (UBYTE)ink;
            if (r->mask_on)
                SetWriteMask(r->rp, r->mask);
        }
        r->seen |= (UBYTE)ink;
        return (UBYTE)ink;
    }
    if (r->scratch[slot] < 0)
        return nearest_pen(r, ink & 0xFFFFFFUL); /* no pen to load: the nearest the screen has */
    if (r->scratch_ink[slot] != ink) {
        rgb = ink & 0xFFFFFFUL;
        SetRGB32(&r->win->WScreen->ViewPort, (ULONG)r->scratch[slot],
                 ((rgb >> 16) & 0xFF) * 0x01010101UL, ((rgb >> 8) & 0xFF) * 0x01010101UL,
                 (rgb & 0xFF) * 0x01010101UL);
        r->scratch_ink[slot] = ink;
    }
    return (UBYTE)r->scratch[slot];
}

static void ink_a(vr_render *r, ULONG ink)
{
    SetAPen(r->rp, ink_pen(r, ink, 0));
}

static void ink_ab(vr_render *r, ULONG fg, ULONG bg)
{
    SetABPenDrMd(r->rp, ink_pen(r, fg, 0), ink_pen(r, bg, 1), JAM2);
}

/* A direct colour on a true-colour screen: a pen of its own while there
 * are pens to spare (drawing with it costs nothing extra), else the
 * scratch pens, reloaded per drawing call (a 1536-colour grid took 11.6 s
 * against 5.7 s for palette colours on the non-exact rig: noisy, not a
 * clean measurement of SetRGB32). The pens are kept in a hash of 0xRRGGBB
 * until vr_free. */
static ULONG truecolor_ink(vr_render *r, ULONG rgb)
{
    static const struct TagItem exact[] = {
        { OBP_Precision, (ULONG)PRECISION_EXACT },
        { OBP_FailIfBad, TRUE },
        { TAG_DONE, 0 }
    };
    ULONG key = rgb | 0x01000000UL;
    int h = (int)(((rgb * 2654435761UL) >> 24) & (VR_EXACT_SLOTS - 1)), i;
    LONG p;
    for (i = 0; i < VR_EXACT_SLOTS; i++, h = (h + 1) & (VR_EXACT_SLOTS - 1)) {
        if (r->exact_key[h] == key)
            return r->exact_pen[h];
        if (!r->exact_key[h])
            break;
    }
    if (i == VR_EXACT_SLOTS || r->n_exact >= VR_EXACT_MAX)
        return VR_INK_RGB | rgb;
    p = ObtainBestPenA(r->cm, ((rgb >> 16) & 0xFF) * 0x01010101UL, ((rgb >> 8) & 0xFF) * 0x01010101UL,
                       (rgb & 0xFF) * 0x01010101UL, (struct TagItem *)exact);
    if (p < 0) {
        r->n_exact = VR_EXACT_MAX; /* the screen has no pen left: scratch from now on */
        return VR_INK_RGB | rgb;
    }
    r->exact_key[h] = key;
    r->exact_pen[h] = (UBYTE)p;
    r->n_exact++;
    return (ULONG)p;
}

/* A pen showing exactly rgb (a colour the map has), held by this window
 * (one ReleasePen owed): that entry, or another of the same colour; -1 when
 * it is nobody's to share (an exclusive pen, which may change colour). */
static LONG hold_pen(vr_render *r, ULONG rgb)
{
    return best_pen(r->cm, rgb, 1);
}

/* Palette index c on a screen of few pens whose map has no entry left for
 * its colour (W-T7): the pen that looks nearest for text, and for areas the
 * two-pen mix when no pen comes close (pen_match.h), every pen used held.
 * The pens to choose from are read from the colour map; a pen that cannot
 * be held is left out. Returns the text's pen, or -1 (then the screen's
 * own nearest, as on any other screen). */
static LONG few_obtain(vr_render *r, int c, ULONG rgb)
{
    ULONG tab[16 * 3];
    unsigned long pal[16], usable;
    struct vr_dither *d = r->dith;
    int n = 1 << GetBitMapAttr(r->win->WScreen->RastPort.BitMap, BMA_DEPTH), i, k, tries;
    pm_pick pk;
    LONG solid, a, b;
    if (!d) {
        if (!(d = r->dith = (struct vr_dither *)AllocVec(sizeof(struct vr_dither), MEMF_ANY | MEMF_CLEAR)))
            return -1;
        for (i = 0; i < PM_LEVELS; i++)
            for (k = 0; k < 8; k++)
                d->pat[i][k] = pm_pattern(i, k);
    }
    if (n > 16)
        n = 16;
    GetRGB32(r->cm, 0, (ULONG)n, tab);
    for (i = 0; i < n; i++)
        pal[i] = ((tab[i * 3] >> 24) << 16) | ((tab[i * 3 + 1] >> 24) << 8) | (tab[i * 3 + 2] >> 24);
    usable = (1UL << n) - 1;
    for (tries = 0; tries < n; tries++) {
        pm_match(pal, n, usable, rgb, r->dither_on, &pk);
        if (pk.solid < 0)
            return -1;
        solid = hold_pen(r, pal[pk.solid]);
        if (solid < 0) {
            usable &= ~(1UL << pk.solid);
            continue;
        }
        if (!pk.lev) {
            d->lev[c] = 0;
            return solid;
        }
        a = hold_pen(r, pal[pk.a]);
        b = a >= 0 ? hold_pen(r, pal[pk.b]) : -1;
        if (a < 0 || b < 0) {
            /* that mix is not to be had: those pens left out, the choice again */
            ReleasePen(r->cm, (ULONG)solid);
            if (a >= 0)
                ReleasePen(r->cm, (ULONG)a);
            usable &= ~(1UL << (a < 0 ? pk.a : pk.b));
            continue;
        }
        d->a[c] = (UBYTE)a;
        d->b[c] = (UBYTE)b;
        d->lev[c] = (UBYTE)pk.lev;
        d->held[c] = 3;
        return solid;
    }
    return -1;
}

/* The pens few_obtain held for palette index c's dither, given back. */
static void dither_release(vr_render *r, int c)
{
    struct vr_dither *d = r->dith;
    if (!d)
        return;
    if (r->cm && (d->held[c] & 1))
        ReleasePen(r->cm, (ULONG)d->a[c]);
    if (r->cm && (d->held[c] & 2))
        ReleasePen(r->cm, (ULONG)d->b[c]);
    d->held[c] = 0;
    d->lev[c] = 0;
}

/* A palette colour (or a direct one on a palette screen): its index in the
 * xterm 256 */
static int cube_index(vt_color c)
{
    return c & VT_COLOR_RGB ? vt_rgb_to_256(VT_RGB_OF(c)) : (int)(c & 0xFF);
}

/* The ink for a resolved engine colour. */
static ULONG pen_for(vr_render *r, vt_color c, int is_bg)
{
    if (c == VT_COLOR_DEFAULT)
        return r->pen_default_fg;
    if (c == VT_COLOR_DEFAULT_BG)
        return r->pen_default_bg;
    if (vt_personality(r->t) == VT_AMIGA && !(c & VT_COLOR_RGB))
        return (UBYTE)(c & 0xFF); /* amiga colours are screen pens; a direct colour (SGR 38;2 / 48;2) is a colour */
    if (c & VT_COLOR_RGB) {
        if (r->truecolor)
            return truecolor_ink(r, VT_RGB_OF(c));
        /* a palette screen (AGA, 8-bit RTG): the nearest of the xterm 256
         * colours, drawn with that index's pen. At most 240 pens, each
         * obtained once and shared with SGR 38;5 -- a pen per new colour
         * ran out after 64 and the rest fell to one colour (AGA 256: 63
         * colours on screen for 320 asked, mean error 161) */
        c = (vt_color)cube_index(c);
    }
    c &= 0xFF;
    /* the 240 extended colours on a true-colour screen: exact, as a direct
     * colour -- a pen each from the screen's colour map ran out after
     * about 150, and "best pen" then gave Workbench colours (a 256-colour
     * cube on the rig: 52 of 240 cells wrong, (238,153,0) for (215,135,0)) */
    if (r->truecolor && (c >= 16 || r->scratch[0] < 0))
        return truecolor_ink(r, vt_palette_rgb(r->t, c)); /* the 16 ANSI colours too when no pen of one's own can be had */
    if (!r->have[c]) {
        ULONG rgb = vt_palette_rgb(r->t, c);
        /* exact first (pen_rule.h); on a screen of few pens a full map then
         * gets few_obtain's choice, not graphics' nearest */
        LONG p = r->few && r->cm ? best_pen(r->cm, rgb, 1) : -1;
        if (p < 0 && r->few && r->cm)
            p = few_obtain(r, (int)c, rgb);
        if (p < 0)
            p = obtain(r, rgb);
        if (p >= 0) {
            r->pens[c] = (UBYTE)p;
            r->obtained[c] = p;
            r->have[c] = 1;
        } else {
            r->have[c] = 2;
        }
    }
    if (r->have[c] == 1)
        return r->pens[c];
    return is_bg ? r->pen_default_bg : r->pen_default_fg;
}

/* The ink an area of colour c is filled with, its text drawn in `ink`
 * (pen_for's): on a screen of few pens the dither of its palette index when
 * it has one, else the same ink. */
static ULONG area_ink(vr_render *r, vt_color c, ULONG ink)
{
    int i;
    if (!r->dith || (ink & VR_INK_RGB) || c == VT_COLOR_DEFAULT || c == VT_COLOR_DEFAULT_BG ||
        (vt_personality(r->t) == VT_AMIGA && !(c & VT_COLOR_RGB)))
        return ink;
    i = cube_index(c);
    if (r->have[i] != 1 || !r->dith->lev[i] || r->pens[i] != ink)
        return ink;
    return VR_INK_DITH | ((ULONG)r->dith->lev[i] << 16) | ((ULONG)r->dith->b[i] << 8) | r->dith->a[i];
}

static void extract_glyphs(vr_render *r);
static void fb_sync(vr_render *r);

/* The colour an ink shows, 0xRRGGBB: a direct colour's own, a pen's from
 * the colour map. */
static ULONG ink_rgb(vr_render *r, ULONG ink)
{
    ink = ink_solid(ink);
    return ink & VR_INK_RGB ? ink & 0xFFFFFFUL : vr_pen_rgb(r, (UBYTE)ink);
}

/* The colour emoji painter's target call (render/emoji): the box's indices
 * through the blended table, WriteLUTPixelArray as the sixel images. */
static void emoji_lut(void *u, const vt_u8 *idx, int w, int h, const vt_u32 *ctab, int px, int py)
{
    vr_render *r = (vr_render *)u;
    vr_cgx_write_lut(r->cgx, (APTR)idx, 0, 0, (UWORD)w, r->rp, (APTR)ctab, (UWORD)px, (UWORD)py, (UWORD)w,
                     (UWORD)h, VR_CTABFMT_XRGB8);
}

/* ---- setup ---------------------------------------------------------------- */

void vr_init(vr_render *r, struct Window *win, struct TextFont *font, vt_term *t,
             enum vt_font_enc enc)
{
    struct DrawInfo *di;
    int i;
    r->win = win;
    r->rp = win->RPort;
    r->font = font;
    r->t = t;
    r->enc = enc;
    fb_sync(r);
    r->cm = win->WScreen ? win->WScreen->ViewPort.ColorMap : 0;
    r->cw = font->tf_XSize;
    r->ch = font->tf_YSize;
    r->base = font->tf_Baseline;
    r->pen_default_fg = 1;
    r->pen_default_bg = 0;
    /* a window's pixels start as pen 0; until the first full redraw says
     * otherwise every plane counts as used */
    r->mask = 0xFF;
    r->mask_on = 0;
    r->blank = r->was_blank = 0;
    r->full_pass = 0;
    r->in_pass = r->bs_valid = r->bs_ok = 0;
    r->seen = 0;
    r->bg_ink = r->pad_ink = r->pen_default_bg;
    r->planar = (UBYTE)((GetBitMapAttr(win->RPort->BitMap, BMA_FLAGS) & BMF_STANDARD) &&
                        GetBitMapAttr(win->RPort->BitMap, BMA_DEPTH) <= 8);
    /* 16 pens or fewer: palette colours no pen shows closely are dithered
     * (W-T7); 256-pen and true-colour screens never */
    r->few = (UBYTE)(win->WScreen && GetBitMapAttr(win->WScreen->RastPort.BitMap, BMA_DEPTH) <= 4);
    r->dith = 0;
    r->dither_on = 1;
    r->pad_extend = r->pad_dirty = 0;
    di = win->WScreen ? GetScreenDrawInfo(win->WScreen) : 0;
    if (di) {
        r->pen_default_fg = (UBYTE)di->dri_Pens[TEXTPEN];
        r->pen_default_bg = (UBYTE)di->dri_Pens[BACKGROUNDPEN];
        FreeScreenDrawInfo(win->WScreen, di);
    }
    for (i = 0; i < 256; i++) {
        r->have[i] = 0;
        r->obtained[i] = -1;
    }
    /* A true-colour screen (RTG, more than 8 bits a pixel) shows direct
     * colours exactly through two exclusive scratch pens (ink_pen); a
     * palette screen (AGA, 8-bit RTG) would recolour everything drawn in a
     * pen it redefines, so there they get the nearest pen instead. */
    r->truecolor = 0;
    r->cgx = 0;
    r->scratch[0] = r->scratch[1] = -1;
    r->scratch_ink[0] = r->scratch_ink[1] = 0;
    r->n_exact = 0;
    for (i = 0; i < VR_EXACT_SLOTS; i++)
        r->exact_key[i] = 0;
    r->dflt_obtained[0] = r->dflt_obtained[1] = -1;
    r->has_blink = 0;
    r->blink_slow_us = r->blink_fast_us = 0;
    r->blink_slow_off = r->blink_fast_off = 0;
    for (i = 0; i <= 10; i++)
        r->alt_font[i] = 0;
    if (r->cm && win->WScreen &&
        GetBitMapAttr(win->WScreen->RastPort.BitMap, BMA_DEPTH) > 8) {
        r->scratch[0] = ObtainPen(r->cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR);
        r->scratch[1] = ObtainPen(r->cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR);
        if (r->scratch[0] < 0 || r->scratch[1] < 0) {
            /* a colour map used up (a 24-bit Workbench has 256 entries or 32 and Directory Opus, the
             * Workbench's own icons and every other window hold them): no pen to load, but an RTG
             * screen shows a direct colour without any -- fills through FillPixelArray (W-T13) */
            if (r->scratch[0] >= 0)
                ReleasePen(r->cm, (ULONG)r->scratch[0]);
            if (r->scratch[1] >= 0)
                ReleasePen(r->cm, (ULONG)r->scratch[1]);
            r->scratch[0] = r->scratch[1] = -1;
        }
        r->cgx = (struct Library *)OpenLibrary((STRPTR)"cybergraphics.library", 41);
        r->truecolor = r->scratch[0] >= 0 || r->cgx != 0;
    }
    r->cursor_drawn = 0;
    r->cursor_colorful = 0;
    r->cursor_ink = VR_KEEP;
    r->sel_ink[0] = r->sel_ink[1] = VR_KEEP;
    r->cursor_pen = r->sel_pen[0] = r->sel_pen[1] = -1;
    r->bell_flash = 0;
    r->cursor_x = r->cursor_y = 0;
    r->view = 0;
    r->view_stale = 0;
    r->sel = 0;
    r->lay_rows = r->lay_cols = r->lay_x = r->lay_y = -1;
    SetFont(r->rp, font);
    r->glyphs = 0;
    if (r->planar)
        extract_glyphs(r); /* the planar path's glyphs (DIRECT=1: also the text's 8-pixel alignment) */
    r->n_direct = r->n_text = 0;
    r->img = 0;
    r->n_img_pens = 0;
    r->img_buf = 0;
    r->img_buf_size = 0;
    r->ex = 0;
    r->img_serial = 0;
    r->n_img_runs = 0;
    r->back = 0;
    r->front_rp = 0;
    r->sync_seen = vt_sync_frames(t); /* frames before this window are not its to compose */
    /* true colour: images straight from their indices (P96 and CGX both
     * offer cybergraphics.library); else the pens path below */
    if (!r->truecolor) {
        if (r->cgx)
            CloseLibrary(r->cgx);
        r->cgx = 0;
    }
    /* colour emoji (U4) through the same call; ce_has asks for RTG of 15
     * bits or more and the call, so AGA and 8-bit RTG keep Unifont's glyph */
    r->emoji.tg.rtg = !r->planar;
    r->emoji.tg.depth = (int)GetBitMapAttr(win->RPort->BitMap, BMA_DEPTH);
    r->emoji.tg.write_lut = r->cgx ? emoji_lut : 0;
    r->emoji.tg.user = r;
    r->emoji.cur = 0;
    ce_set_cell(&r->emoji, r->cw, r->ch);
    vr_layout(r);
}

void vr_set_defaults(vr_render *r, ULONG fg_rgb, ULONG bg_rgb)
{
    ULONG want[2];
    int i;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    want[0] = fg_rgb;
    want[1] = bg_rgb;
    for (i = 0; i < 2; i++) {
        LONG p;
        if (want[i] == VR_KEEP)
            continue;
        p = obtain(r, want[i] & 0xFFFFFF);
        if (p < 0)
            continue;
        /* one pen per default at a time: programs may change them (OSC 10/11) */
        if (r->dflt_obtained[i] >= 0 && r->cm)
            ReleasePen(r->cm, (ULONG)r->dflt_obtained[i]);
        r->dflt_obtained[i] = p;
        if (i == 0)
            r->pen_default_fg = (UBYTE)p;
        else
            r->pen_default_bg = (UBYTE)p;
    }
    /* a default blank looks different now, also inside a pass (OSC 11 read
     * after the pass drew the cursor off): worked out again (blank_look) */
    r->bs_valid = 0;
}

/* The ink for a profile colour, and the pen obtained for it that the
 * caller then owns (-1: none -- an exact true-colour pen stays exact_pen[]'s,
 * released by vr_free). VR_KEEP when no pen was left. */
static ULONG profile_ink(vr_render *r, ULONG rgb, LONG *own)
{
    LONG p;
    *own = -1;
    if (rgb == VR_KEEP)
        return VR_KEEP;
    if (r->truecolor)
        return truecolor_ink(r, rgb);
    p = obtain(r, rgb);
    if (p < 0)
        return VR_KEEP;
    *own = p;
    return (ULONG)p;
}

void vr_set_cursor_color(vr_render *r, ULONG rgb)
{
    LONG old = r->cursor_pen, pen;
    ULONG ink;
    if (!r->win)
        return; /* before vr_init, after vr_free */
    ink = profile_ink(r, rgb, &pen); /* VR_KEEP: inverted */
    /* the cursor drawn in the old colour goes before its pen does */
    if (r->cursor_drawn) {
        vr_cursor_off(r);
        r->cursor_ink = ink;
        r->cursor_pen = pen;
        vr_cursor_on(r); /* the new colour on the cell the cursor is at now */
    } else {
        r->cursor_ink = ink;
        r->cursor_pen = pen;
    }
    if (old >= 0 && r->cm)
        ReleasePen(r->cm, (ULONG)old);
}

/* The profile's selection colours. VR_KEEP for either keeps that half of the
 * swap, so setting only selection-bg themes the highlight and leaves the text
 * the colour the cell would otherwise have had. */
void vr_set_selection_colors(vr_render *r, ULONG fg_rgb, ULONG bg_rgb)
{
    LONG old[2];
    int i;
    if (!r->win)
        return; /* before vr_init, after vr_free */
    for (i = 0; i < 2; i++) {
        old[i] = r->sel_pen[i];
        r->sel_ink[i] = profile_ink(r, i ? bg_rgb : fg_rgb, &r->sel_pen[i]);
    }
    /* a selection on screen is drawn in the old pens: repaint it in the new
     * ones before those are given back */
    if (r->sel)
        vr_redraw(r);
    for (i = 0; i < 2; i++)
        if (old[i] >= 0 && r->cm)
            ReleasePen(r->cm, (ULONG)old[i]);
}

/* The colour a screen pen shows now, 0xRRGGBB. */
ULONG vr_pen_rgb(vr_render *r, UBYTE pen)
{
    ULONG c[3];
    if (!r->cm)
        return 0;
    GetRGB32(r->cm, pen, 1, c);
    return ((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24);
}

/* A program changed the palette or the default colours: every pen chosen
 * for a colour is given back and chosen again as cells are drawn. */
void vr_palette_changed(vr_render *r)
{
    int i;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    if (r->cm)
        for (i = 0; i < 256; i++)
            if (r->obtained[i] >= 0)
                ReleasePen(r->cm, (ULONG)r->obtained[i]);
    for (i = 0; i < 256; i++) {
        dither_release(r, i);
        r->have[i] = 0;
        r->obtained[i] = -1;
    }
}

void vr_set_dither(vr_render *r, int on)
{
    if ((r->dither_on != 0) == (on != 0))
        return;
    r->dither_on = (UBYTE)(on != 0);
    vr_palette_changed(r);
}

void vr_set_pad_extend(vr_render *r, int on)
{
    r->pad_extend = (UBYTE)(on != 0);
    r->pad_dirty = (UBYTE)(on != 0);
}

void vr_free(vr_render *r)
{
    int i;
    if (r->cm)
        for (i = 0; i < 256; i++)
            if (r->obtained[i] >= 0)
                ReleasePen(r->cm, (ULONG)r->obtained[i]);
    if (r->dith) {
        for (i = 0; i < 256; i++)
            dither_release(r, i);
        if (r->dith->tmpl) {
            WaitBlit(); /* a template blit may still read it */
            FreeVec(r->dith->tmpl);
        }
        FreeVec(r->dith);
        r->dith = 0;
    }
    for (i = 0; i < 2; i++)
        if (r->cm && r->scratch[i] >= 0)
            ReleasePen(r->cm, (ULONG)r->scratch[i]);
    for (i = 0; i < VR_EXACT_SLOTS; i++)
        if (r->cm && r->exact_key[i])
            ReleasePen(r->cm, (ULONG)r->exact_pen[i]);
    for (i = 0; i < 2; i++)
        if (r->cm && r->dflt_obtained[i] >= 0)
            ReleasePen(r->cm, (ULONG)r->dflt_obtained[i]);
    /* the profile inks: only the pens their setters obtained (an exact
     * true-colour ink was released with exact_pen[] above) */
    if (r->cm && r->cursor_pen >= 0)
        ReleasePen(r->cm, (ULONG)r->cursor_pen);
    for (i = 0; i < 2; i++)
        if (r->cm && r->sel_pen[i] >= 0)
            ReleasePen(r->cm, (ULONG)r->sel_pen[i]);
    r->dflt_obtained[0] = r->dflt_obtained[1] = -1;
    for (i = 0; i < VR_EXACT_SLOTS; i++)
        r->exact_key[i] = 0;
    r->n_exact = 0;
    r->scratch[0] = r->scratch[1] = -1;
    r->truecolor = 0;
    if (r->glyphs)
        FreeVec(r->glyphs);
    if (r->img) {
        for (i = 0; i < VR_IMG_SLOTS; i++)
            if (r->cm && r->img->key[i])
                ReleasePen(r->cm, (ULONG)r->img->pen[i]);
        FreeVec(r->img);
        r->img = 0;
    }
    if (r->ex) {
        if (r->ex->bm) {
            WaitBlit();
            FreeBitMap(r->ex->bm);
        }
        if (r->ex->buf)
            FreeVec(r->ex->buf);
        FreeVec(r->ex);
        r->ex = 0;
    }
    if (r->img_buf)
        FreeVec(r->img_buf);
    if (r->cgx)
        CloseLibrary(r->cgx);
    if (r->back) {
        if (r->back->bm)
            FreeBitMap(r->back->bm);
        if (r->back->pmem)
            FreeVec(r->back->pmem);
        FreeVec(r->back);
    }
    /* inert until the next vr_init: the handler closes an AUTO window and
     * opens another, and vt_new's reset flushes damage through this
     * renderer before vr_init runs -- it drew into the closed window's
     * RastPort and hung the handler (rig, 2026-09-30) */
    memset(r, 0, sizeof(*r));
}

void vr_set_font(vr_render *r, struct TextFont *font)
{
    if (!r->win || !font)
        return;
    r->font = font;
    r->cw = font->tf_XSize;
    r->ch = font->tf_YSize;
    r->base = font->tf_Baseline;
    SetFont(r->rp, font);
    if (r->outline)
        vo_set_cell(r->outline, r->cw, r->ch, r->base); /* its glyphs at the new cell */
    if (r->unifont)
        uf_set_cell(r->unifont, r->cw, r->ch);
    ce_set_cell(&r->emoji, r->cw, r->ch);
    if (r->glyphs)
        FreeVec(r->glyphs);
    r->glyphs = 0;
    if (r->planar)
        extract_glyphs(r);
}

void vr_set_outline(vr_render *r, struct vo_font *f)
{
    r->outline = f;
    if (f)
        vo_set_cell(f, r->cw, r->ch, r->base);
    fb_sync(r);
}

void vr_set_unifont(vr_render *r, struct uf_cache *c)
{
    r->unifont = c;
    if (c)
        uf_set_cell(c, r->cw, r->ch);
    fb_sync(r);
}

void vr_set_emoji(vr_render *r, struct ce_store *s)
{
    r->emoji.store = s;
    r->emoji.cur = 0;
    fb_sync(r);
}

int vr_can_colour(const vr_render *r)
{
    return ce_target_ok(&r->emoji.tg);
}

void vr_set_off(vr_render *r, int off)
{
    if (off && !r->off && r->win)
        vr_cursor_off(r); /* still ours to take away */
    r->off = (BYTE)(off != 0);
    /* another tab drew these pixels meanwhile: nothing is known about them */
    r->mask = 0xFF;
    r->blank = 0;
    if (off)
        r->cursor_drawn = 0;
    r->hidden = r->off || r->cols < 1 || r->rows < 1;
}

void vr_set_inset(vr_render *r, WORD top)
{
    r->inset_top = top;
}

/* The columns and rows the window can show right now. The grid follows a
 * new window size only when Intuition reports it (resize()); between a
 * size change and that report -- DECCOLM shrinking the window, the user
 * dragging it smaller -- drawing to r->cols wrote over the window's own
 * right border (owner, 2026-09-29, vttest's 132-column test). */
static WORD vis_cols(const vr_render *r)
{
    WORD c;
    if (!r->win || !r->cw)
        return 0; /* not set up yet (vr_init): nothing to draw into */
    c = (WORD)((r->win->Width - r->win->BorderRight - r->ox) / r->cw);
    return c < r->cols ? (c < 0 ? 0 : c) : r->cols;
}

/* The grid columns double-size cell x covers in the window: 2, or 1 for
 * the last cell of a row whose visible columns are odd -- the engine gives
 * a double-size row (cols + 1) / 2 cells, that one half in the grid. Past
 * the grid is the window's border: drawn 2 columns wide, the last cell
 * of UPDemo's headings painted over the right border (W-T10). */
static int dbl_cols(const vr_render *r, int x)
{
    int n = vis_cols(r) - 2 * x;
    return n < 2 ? n : 2;
}

static WORD vis_rows(const vr_render *r)
{
    WORD n;
    if (!r->win || !r->ch)
        return 0;
    n = (WORD)((r->win->Height - r->win->BorderBottom - r->oy) / r->ch);
    return n < r->rows ? (n < 0 ? 0 : n) : r->rows;
}

int vr_layout(vr_render *r)
{
    struct Window *w = r->win;
    WORD lx = r->lay_x > 0 ? r->lay_x : 0, ly = (r->lay_y > 0 ? r->lay_y : 0) + r->inset_top;
    WORD iw = w->Width - w->BorderLeft - w->BorderRight - lx;
    WORD ih = w->Height - w->BorderTop - w->BorderBottom - ly;
    WORD cols = iw / r->cw, rows = ih / r->ch;
    int changed;
    /* the Amiga console's page and line length cap what fits */
    if (r->lay_cols > 0 && r->lay_cols < cols)
        cols = r->lay_cols;
    if (r->lay_rows > 0 && r->lay_rows < rows)
        rows = r->lay_rows;
    r->ox = w->BorderLeft + lx;
    r->oy = w->BorderTop + ly;
#ifdef VTCON_DIRECT
    if (r->glyphs) {
        /* the planar path needs cells on a byte boundary of the screen:
         * start the text at the next 8-pixel column (at most 7 px more).
         * Only in a DIRECT=1 build: by default the path is used where the
         * text is on a byte boundary anyway (a borderless window, a lucky
         * place) and nobody loses a column to it. */
        WORD pad = (WORD)((8 - ((w->LeftEdge + r->ox) & 7)) & 7);
        if (cols * r->cw + pad > iw)
            cols = (iw - pad) / r->cw;
        r->ox += pad;
    }
#endif
    r->hidden = r->off || cols < 1 || rows < 1;
    if (cols < 1)
        cols = 1;
    if (rows < 1)
        rows = 1;
    changed = cols != r->cols || rows != r->rows;
    r->cols = cols;
    r->rows = rows;
    return changed;
}

/* ---- drawing ------------------------------------------------------------- */

static void fill_pen(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG pen);
static void fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG pen)
{
#ifdef VTCON_PROF
    struct EClockVal f0, f1;
    if (vtwin_timer)
        ReadEClock(&f0);
    fill_pen(r, x0, y0, x1, y1, pen);
    if (vtwin_timer) {
        ReadEClock(&f1);
        vr_prof[3] += f1.ev_lo - f0.ev_lo;
        vr_prof[4]++;
    }
#else
    fill_pen(r, x0, y0, x1, y1, pen);
#endif
}

static void back_fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, UBYTE pen);
static void back_fill_pat(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG ink);

/* A dither ink's pattern for window pixels x .. x + 7 of pixel row y, the
 * leftmost the top bit, set where pen b goes. Anchored to the cells: the
 * row of the 8 x 8 tile is the pixel row within the cell, the column the
 * pixel column within the cell -- the same bits in place, composed
 * (block_run, back_fill_pat), and after a scroll of whole cells. */
static UBYTE pat_bits(vr_render *r, ULONG ink, WORD x, WORD y)
{
    int gy = (y - r->oy) % r->ch, gx = (x - r->ox) % r->cw, i;
    UBYTE pr, v = 0;
    if (gy < 0)
        gy += r->ch;
    if (gx < 0)
        gx += r->cw;
    pr = r->dith->pat[DITH_LEV(ink)][gy & 7];
    if (r->cw == 8)
        return gx ? (UBYTE)((pr << gx) | (pr >> (8 - gx))) : pr;
    for (i = 0; i < 8; i++, gx = gx + 1 < r->cw ? gx + 1 : 0)
        if ((pr >> (7 - (gx & 7))) & 1)
            v |= (UBYTE)(0x80 >> i);
    return v;
}

/* A dither fill in place: the pattern as a template in chip RAM, drawn in
 * JAM2 with pen b for its set bits and pen a for the rest (BltTemplate puts
 * the template's first bit at x0, so the pixels are pat_bits' exactly; an
 * area pattern (SetAfPt) is aligned where graphics puts it, not to the
 * cells). Without the memory: the pen that covers more of the pattern. */
static void pat_fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG ink)
{
    struct vr_dither *d = r->dith;
    WORD w = (WORD)(x1 - x0 + 1), h = (WORD)(y1 - y0 + 1), bpr = (WORD)(((w + 15) >> 4) << 1), y, i;
    ULONG need = (ULONG)bpr * (ULONG)h;
    if (need > d->tmpl_size) {
        if (d->tmpl) {
            WaitBlit(); /* a template blit may still read it */
            FreeVec(d->tmpl);
        }
        d->tmpl = (UBYTE *)AllocVec(need, MEMF_CHIP);
        d->tmpl_size = d->tmpl ? need : 0;
    }
    if (!d->tmpl) {
        ink_a(r, ink);
        SetDrMd(r->rp, JAM1);
        RectFill(r->rp, x0, y0, x1, y1);
        return;
    }
    WaitBlit(); /* the last template blit has read it */
    for (y = 0; y < h; y++) {
        UBYTE *row = d->tmpl + (LONG)y * bpr;
        for (i = 0; i < bpr; i++)
            row[i] = pat_bits(r, ink, (WORD)(x0 + i * 8), (WORD)(y0 + y));
    }
    SetABPenDrMd(r->rp, ink_pen(r, DITH_B(ink), 0), ink_pen(r, DITH_A(ink), 1), JAM2);
    BltTemplate((PLANEPTR)d->tmpl, 0, bpr, r->rp, x0, y0, w, h);
    SetDrMd(r->rp, JAM1);
}

static void fill_pen(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG pen)
{
    if (x1 < x0 || y1 < y0)
        return;
    if (pen != r->bg_ink)
        r->blank = 0;
    if (r->cpu) {
        /* a planar composed pass: the CPU fills its planes (W-T4) */
        if (pen & VR_INK_RGB) {
            r->cpu_fail = 1;
            return;
        }
        if (IS_DITH(pen))
            back_fill_pat(r, x0, y0, x1, y1, pen);
        else
            back_fill(r, x0, y0, x1, y1, ink_pen(r, pen, 0));
        return;
    }
    if (IS_DITH(pen)) {
        pat_fill(r, x0, y0, x1, y1, pen);
        return;
    }
    if ((pen & VR_INK_RGB) && r->cgx) {
        /* a direct colour on a true-colour screen: filled as the colour
         * itself. Through a scratch pen it was a SetRGB32 first, 84 us a
         * call on the 68060 rig: UPDemo's colour scenes are fills (half
         * blocks, blanks), and SetRGB32 was 70-80 % of their drawing --
         * palette cycling 931 a frame, 113 ms a frame (ledger S2) */
        vr_cgx_fill(r->cgx, r->rp, (UWORD)x0, (UWORD)y0, (UWORD)(x1 - x0 + 1), (UWORD)(y1 - y0 + 1),
                    pen & 0xFFFFFFUL);
        SetDrMd(r->rp, JAM1); /* as after a pen fill: the drawing after it (lines, marks) expects it */
        return;
    }
    ink_a(r, pen);
    SetDrMd(r->rp, JAM1);
    RectFill(r->rp, x0, y0, x1, y1);
}

/* COMPLEMENT flips every plane: all of them are in use from here on */
static void all_planes(vr_render *r)
{
    r->mask = 0xFF;
    r->blank = 0;
    SetWriteMask(r->rp, 0xFF);
}

void vr_mask_begin(vr_render *r)
{
    r->in_pass = 1;   /* one flush: what a default blank looks like is worked out once (draw_rows) */
    r->bs_valid = 0;
    /* jump scroll: a pass right after one that scrolled moves twice as far
     * (up to a screenful); a pass that did not scroll resets the step */
    if (r->scrolled_pass) {
        int h = vis_rows(r) - 1;
        r->jump_step = r->jump_step ? r->jump_step * 2 : 2;
        if (r->jump_step > h)
            r->jump_step = h > 0 ? h : 0;
    } else {
        r->jump_step = 0;
    }
    r->scrolled_pass = 0;
    if (!r->win || !r->planar)
        return;
    r->mask_on = 1;
    SetWriteMask(r->rp, r->mask);
}

void vr_mask_end(vr_render *r)
{
    r->in_pass = 0;
    if (!r->mask_on)
        return;
    r->mask_on = 0;
    SetWriteMask(r->rp, 0xFF);
}

/* Drawing only graphics.library does, asked for inside a planar composed
 * pass (whose planes the CPU paths alone write): 1, and the pass is drawn
 * in place instead (draw_rows, compose_abandon). */
static int cpu_refuses(vr_render *r)
{
    if (!r->cpu)
        return 0;
    r->cpu_fail = 1;
    return 1;
}

static void line(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1)
{
    Move(r->rp, x0, y0);
    Draw(r->rp, x1, y1);
}

/* Box drawing: arms from the cell centre to its edges, 1 px light, 2 px
 * heavy, two 1 px lines double. Joins with the neighbours because every
 * arm reaches the cell edge. */
static void draw_box(vr_render *r, WORD px, WORD py, vt_u8 code, ULONG fg)
{
    WORD cx = px + r->cw / 2, cy = py + r->ch / 2;
    WORD x1 = px + r->cw - 1, y1 = py + r->ch - 1;
    int arm;
    ink_a(r, fg);
    SetDrMd(r->rp, JAM1);
    for (arm = 0; arm < 4; arm++) {
        int w = VT_BOX_ARM(code, arm);
        WORD ax0, ay0, ax1, ay1, dx = 0, dy = 0;
        if (!w)
            continue;
        switch (arm) {
        case 0: ax0 = cx; ay0 = py; ax1 = cx; ay1 = cy; dx = 1; break;
        case 1: ax0 = cx; ay0 = cy; ax1 = x1; ay1 = cy; dy = 1; break;
        case 2: ax0 = cx; ay0 = cy; ax1 = cx; ay1 = y1; dx = 1; break;
        default: ax0 = px; ay0 = cy; ax1 = cx; ay1 = cy; dy = 1; break;
        }
        if (w == 3) {
            line(r, ax0 - dx, ay0 - dy, ax1 - dx, ay1 - dy);
            line(r, ax0 + dx, ay0 + dy, ax1 + dx, ay1 + dy);
        } else {
            line(r, ax0, ay0, ax1, ay1);
            if (w == 2)
                line(r, ax0 + dx, ay0 + dy, ax1 + dx, ay1 + dy);
        }
    }
}

static const UWORD shade_pat[3][2] = {
    { 0x8888, 0x2222 }, /* light: 25 % */
    { 0xAAAA, 0x5555 }, /* medium: 50 % */
    { 0x7777, 0xDDDD }  /* dark: 75 % */
};

/* `cells` neighbouring cells of one look in a row (draw_rows joins them only for
 * a block that is a band across the whole cell: the upper and lower parts, see
 * block_joins); a lone cell is 1. */
static void draw_block(vr_render *r, WORD px, WORD py, vt_u8 code, ULONG fg, ULONG bg, int cells)
{
    WORD w = (WORD)(r->cw * cells), h = r->ch;
    if (code & 0x80) {
        ink_ab(r, fg, bg);
        SetAfPt(r->rp, (UWORD *)shade_pat[(code & 3) - 1], 1);
        RectFill(r->rp, px, py, px + w - 1, py + h - 1);
        SetAfPt(r->rp, 0, 0);
        return;
    }
    if (code & 0x40) {
        WORD hw = w / 2, hh = h / 2;
        if (code & 1)
            fill(r, px, py, px + hw - 1, py + hh - 1, fg);
        if (code & 2)
            fill(r, px + hw, py, px + w - 1, py + hh - 1, fg);
        if (code & 4)
            fill(r, px, py + hh, px + hw - 1, py + h - 1, fg);
        if (code & 8)
            fill(r, px + hw, py + hh, px + w - 1, py + h - 1, fg);
        return;
    }
    {
        int side = (code >> 4) & 3, k = code & 15;
        WORD bw = (WORD)((w * k + 4) / 8), bh = (WORD)((h * k + 4) / 8);
        switch (side) {
        case 0: fill(r, px, py, px + w - 1, py + bh - 1, fg); break;
        case 1: fill(r, px, py + h - bh, px + w - 1, py + h - 1, fg); break;
        case 2: fill(r, px, py, px + bw - 1, py + h - 1, fg); break;
        default: fill(r, px + w - bw, py, px + w - 1, py + h - 1, fg); break;
        }
    }
}

/* A block that is a band across the full cell width (upper or lower part): n of them side by
 * side are one band n cells wide. Quadrants, vertical bars and shades are not (their parts
 * depend on the cell's width, or the pattern on its place). */
static int block_joins(vt_glyph g)
{
    return g.kind == VT_GLYPH_BLOCK && !(g.code & 0xC0) && ((g.code >> 4) & 3) < 2;
}

static int exact_ink(const vr_render *r, ULONG ink);
static int draw_special_exact(vr_render *r, WORD px, WORD py, vt_glyph g, ULONG fg, ULONG bg, int cells);

static void draw_special(vr_render *r, WORD px, WORD py, vt_glyph g, ULONG fg, ULONG bg, int cells)
{
    WORD x1 = px + r->cw * (g.kind == VT_GLYPH_MISSING && g.code == 2 ? 2 : cells) - 1, y1 = py + r->ch - 1;
    if ((g.kind != VT_GLYPH_BLOCK || (g.code & 0x80)) && cpu_refuses(r))
        return; /* lines and shade patterns: not the CPU fills' */
    if (g.kind != VT_GLYPH_BLOCK && exact_ink(r, fg) && draw_special_exact(r, px, py, g, fg, bg, cells))
        return;
    r->blank = 0;
    fill(r, px, py, x1, y1, bg);
    switch (g.kind) {
    case VT_GLYPH_MISSING: {
        /* a character no font here has: an empty box its width, a pixel in
         * from the sides and an eighth of the height in from top and bottom */
        WORD bx0 = (WORD)(px + 1), bx1 = (WORD)(x1 - 1);
        WORD by0 = (WORD)(py + r->ch / 8), by1 = (WORD)(y1 - r->ch / 8);
        ink_a(r, fg);
        line(r, bx0, by0, bx1, by0);
        line(r, bx0, by1, bx1, by1);
        line(r, bx0, by0, bx0, by1);
        line(r, bx1, by0, bx1, by1);
        break;
    }
    case VT_GLYPH_BOX:
        draw_box(r, px, py, g.code, fg);
        break;
    case VT_GLYPH_BLOCK:
        draw_block(r, px, py, g.code, fg, bg, cells);
        break;
    case VT_GLYPH_DIAGONAL:
        ink_a(r, fg);
        if (g.code & 1)
            line(r, px, y1, x1, py);
        if (g.code & 2)
            line(r, px, py, x1, y1);
        break;
    case VT_GLYPH_HLINE: {
        WORD y = py + (WORD)((r->ch - 1) * g.code / 7);
        ink_a(r, fg);
        line(r, px, y, x1, y);
        break;
    }
    case VT_GLYPH_DIAMOND: {
        WORD cx = px + r->cw / 2, cy = py + r->ch / 2, rx = r->cw / 2 - 1, ry = r->ch / 2 - 1, d;
        ink_a(r, fg);
        for (d = 0; d <= ry; d++) {
            WORD half = (WORD)(rx * (ry - d) / (ry ? ry : 1));
            line(r, cx - half, cy - d, cx + half, cy - d);
            line(r, cx - half, cy + d, cx + half, cy + d);
        }
        break;
    }
    default:
        break;
    }
}

/* How a run of cells is drawn: equal styles make one Text() call. */
typedef struct vr_style {
    ULONG fg, bg, ul;   /* inks; ul the underline's (the text's unless SGR 58) */
    ULONG fgf, bgf;     /* the inks fg's and bg's areas are filled with: a dither on a screen of few pens (area_ink) */
    vt_attr attr;       /* the attributes that change the drawing */
    vt_u8 deco, font;
} vr_style;
static int painter_run(vr_render *r, const UBYTE *run, int n, WORD px, WORD py, const vr_style *st);
#ifdef VR_ASM
long vr_asm_row_scan(const vt_cell *c, long n, UBYTE *out);
#endif

#define DRAWN_ATTRS (VT_ATTR_BOLD | VT_ATTR_ITALIC | VT_ATTR_UNDERLINE | VT_ATTR_STRIKE | \
                     VT_ATTR_OVERLINE | VT_ATTR_SUPER | VT_ATTR_SUB | VT_ATTR_FRAMED | \
                     VT_ATTR_ENCIRCLED)
#define LINE_ATTRS (VT_ATTR_UNDERLINE | VT_ATTR_STRIKE | VT_ATTR_OVERLINE | VT_ATTR_FRAMED | \
                    VT_ATTR_ENCIRCLED)

static int same_style(const vr_style *a, const vr_style *b)
{
    return a->fg == b->fg && a->bg == b->bg && a->fgf == b->fgf && a->bgf == b->bgf && a->ul == b->ul &&
           a->attr == b->attr &&
           a->deco == b->deco && a->font == b->font;
}

static ULONG style_of(vt_attr attr)
{
    ULONG s = 0;
    if (attr & VT_ATTR_BOLD)
        s |= FSF_BOLD;
    if (attr & VT_ATTR_ITALIC)
        s |= FSF_ITALIC;
    return s; /* underlines are drawn by decorate(), in their style and colour */
}

static void hline(vr_render *r, WORD x0, WORD x1, WORD y)
{
    Move(r->rp, x0, y);
    Draw(r->rp, x1, y);
}

/* A broken line: on pixels, off pixels (dotted 1/1, dashed 3/2). Drawn as
 * short fills: the line pattern (SetDrPt) drew nothing on the rig's RTG
 * screen. */
static void broken_hline(vr_render *r, WORD x0, WORD x1, WORD y, WORD on, WORD off)
{
    WORD x;
    for (x = x0; x <= x1; x = (WORD)(x + on + off))
        RectFill(r->rp, x, y, (WORD)(x + on - 1 > x1 ? x1 : x + on - 1), y);
}

/* The lines around a run: underline in its style and colour, overline,
 * strike, frame, circle (a frame with its corners cut) and the ideogram
 * lines of SGR 60-64. */
static void decorate(vr_render *r, int n, WORD px, WORD py, const vr_style *st)
{
    WORD x1 = (WORD)(px + n * r->cw - 1), y1 = (WORD)(py + r->ch - 1);
    WORD u = (WORD)(py + r->base + 1), i;
    int ideo = (st->deco & VT_DECO_IDEO_MASK) >> VT_DECO_IDEO_SHIFT;
    /* two lines need a free row between them below the baseline: an 8-pixel
     * cell has one row there, so its double lines are drawn single */
    int room2 = u + 2 <= y1;
    if (u > y1)
        u = y1;
    SetDrMd(r->rp, JAM1);
    if (st->attr & VT_ATTR_UNDERLINE) {
        ink_a(r, st->ul);
        switch (st->deco & VT_DECO_UL_MASK) {
        case VT_UL_DOUBLE:
            hline(r, px, x1, u);
            if (room2)
                hline(r, px, x1, (WORD)(u + 2));
            break;
        case VT_UL_CURLY: {
            WORD lo = (WORD)(u + 1 <= y1 ? u + 1 : u - 1);
            Move(r->rp, px, u);
            for (i = px; i <= x1; i += 2)
                Draw(r->rp, i, ((i - px) >> 1) & 1 ? lo : u);
            break;
        }
        case VT_UL_DOTTED:
            broken_hline(r, px, x1, u, 1, 1);
            break;
        case VT_UL_DASHED:
            broken_hline(r, px, x1, u, 3, 2);
            break;
        default:
            hline(r, px, x1, u);
            break;
        }
    }
    ink_a(r, st->fg);
    if (st->attr & VT_ATTR_OVERLINE)
        hline(r, px, x1, py);
    if (st->attr & VT_ATTR_STRIKE)
        hline(r, px, x1, (WORD)(py + r->ch / 2));
    if (st->attr & VT_ATTR_FRAMED) {
        Move(r->rp, px, py);
        Draw(r->rp, x1, py);
        Draw(r->rp, x1, y1);
        Draw(r->rp, px, y1);
        Draw(r->rp, px, py);
    }
    if (st->attr & VT_ATTR_ENCIRCLED) {
        hline(r, (WORD)(px + 2), (WORD)(x1 - 2), py);
        hline(r, (WORD)(px + 2), (WORD)(x1 - 2), y1);
        line(r, px, (WORD)(py + 2), px, (WORD)(y1 - 2));
        line(r, x1, (WORD)(py + 2), x1, (WORD)(y1 - 2));
        WritePixel(r->rp, (WORD)(px + 1), (WORD)(py + 1));
        WritePixel(r->rp, (WORD)(x1 - 1), (WORD)(py + 1));
        WritePixel(r->rp, (WORD)(px + 1), (WORD)(y1 - 1));
        WritePixel(r->rp, (WORD)(x1 - 1), (WORD)(y1 - 1));
    }
    switch (ideo) {
    case VT_IDEO_DOUBLE_UNDERLINE:
        if (room2)
            hline(r, px, x1, (WORD)(y1 - 2));
        /* fall through */
    case VT_IDEO_UNDERLINE:
        hline(r, px, x1, y1);
        break;
    case VT_IDEO_DOUBLE_OVERLINE:
        if (room2)
            hline(r, px, x1, (WORD)(py + 2));
        /* fall through */
    case VT_IDEO_OVERLINE:
        hline(r, px, x1, py);
        break;
    case VT_IDEO_STRESS:
        for (i = 0; i < n; i++) {
            WORD cx = (WORD)(px + i * r->cw + r->cw / 2);
            RectFill(r->rp, (WORD)(cx - 1), (WORD)(y1 - 1), cx, y1);
        }
        break;
    default:
        break;
    }
}

/* Superscript and subscript: each glyph drawn into a one-plane mask,
 * scaled to two thirds and stamped in the text colour at the top (super)
 * or the bottom (sub) of its cell. Rare, so the masks are made per run. */
static int draw_script(vr_render *r, UBYTE *run, int n, WORD px, WORD py, const vr_style *st,
                       struct TextFont *font)
{
    WORD cw = r->cw, ch = r->ch, w2 = (WORD)((cw * 2 + 2) / 3), h2 = (WORD)((ch * 2 + 2) / 3);
    struct BitMap *full = AllocBitMap(cw, ch, 1, BMF_CLEAR, 0);
    struct BitMap *small = AllocBitMap(w2, h2, 1, BMF_CLEAR, 0);
    struct RastPort trp;
    struct BitScaleArgs bsa;
    int i;
    if (!full || !small) {
        if (full)
            FreeBitMap(full);
        if (small)
            FreeBitMap(small);
        return 0;
    }
    fill(r, px, py, (WORD)(px + n * cw - 1), (WORD)(py + ch - 1), st->bg);
    InitRastPort(&trp);
    trp.BitMap = full;
    SetFont(&trp, font);
    SetSoftStyle(&trp, style_of(st->attr), FSF_BOLD | FSF_ITALIC);
    SetAPen(&trp, 1);
    SetDrMd(&trp, JAM1);
    ink_a(r, st->fg);
    SetDrMd(r->rp, JAM1);
    for (i = 0; i < n; i++) {
        SetRast(&trp, 0);
        Move(&trp, 0, r->base);
        Text(&trp, (STRPTR)&run[i], 1);
        bsa.bsa_SrcX = bsa.bsa_SrcY = 0;
        bsa.bsa_SrcWidth = (UWORD)cw;
        bsa.bsa_SrcHeight = (UWORD)ch;
        bsa.bsa_XSrcFactor = (UWORD)cw;
        bsa.bsa_XDestFactor = (UWORD)w2;
        bsa.bsa_YSrcFactor = (UWORD)ch;
        bsa.bsa_YDestFactor = (UWORD)h2;
        bsa.bsa_SrcBitMap = full;
        bsa.bsa_DestBitMap = small;
        bsa.bsa_DestX = bsa.bsa_DestY = 0;
        bsa.bsa_Flags = 0;
        BitMapScale(&bsa);
        WaitBlit();
        BltTemplate((PLANEPTR)small->Planes[0], 0, (WORD)small->BytesPerRow, r->rp,
                    (WORD)(px + i * cw + (cw - w2) / 2),
                    (WORD)(st->attr & VT_ATTR_SUB ? py + ch - h2 : py), w2, h2);
    }
    WaitBlit();
    FreeBitMap(small);
    FreeBitMap(full);
    return 1;
}

static void flush_run(vr_render *r, UBYTE *run, int n, WORD px, WORD py, const vr_style *st);

/* The colour of pen p, as the screen's colour map has it: 0xRRGGBB. */
static ULONG pen_rgb(vr_render *r, ULONG p)
{
    ULONG tab[3];
    if (!r->cm)
        return 0;
    GetRGB32(r->cm, p, 1, tab);
    return ((tab[0] >> 24) << 16) | ((tab[1] >> 24) << 8) | (tab[2] >> 24);
}

/* Things drawn in a colour no pen holds (W-T13): a direct colour on a true-colour screen with no pen to load, so
 * a glyph or a line drawn through a pen is the nearest colour the screen has, never the colour. The drawing is
 * done by the caller's own code in a one-plane bitmap (pen 1 the ink, pen 0 the rest: every soft style, the lines
 * and the bold smear as for anything else), its bits made bytes, and written to the window through a two-entry
 * table of the exact colours -- WriteLUTPixelArray, as images are: no pen, the layer clips. exact_ink: is this
 * ink such a colour. */
static int exact_ink(const vr_render *r, ULONG ink)
{
    return (ink & VR_INK_RGB) && r->scratch[0] < 0 && r->cgx && !r->cpu;
}

/* The scratch bitmap for w by h pixels, cleared, and r->ex->rp drawing into it with the cell font; 0: no memory. */
static int exact_begin(vr_render *r, LONG w, LONG h)
{
    LONG need = (w + 15) & ~15L;
    if (!r->ex && !(r->ex = (struct vr_exact *)AllocVec(sizeof(struct vr_exact), MEMF_ANY | MEMF_CLEAR)))
        return 0;
    if (!r->ex->bm || r->ex->bm_w < need || r->ex->bm_h < h) {
        if (r->ex->bm) {
            WaitBlit();
            FreeBitMap(r->ex->bm);
        }
        r->ex->bm = AllocBitMap((ULONG)need, (ULONG)h, 1, BMF_CLEAR, 0);
        r->ex->bm_w = r->ex->bm ? need : 0;
        r->ex->bm_h = r->ex->bm ? h : 0;
        if (!r->ex->bm)
            return 0;
    }
    if (r->ex->buf_size < (ULONG)(need * h)) {
        if (r->ex->buf)
            FreeVec(r->ex->buf);
        r->ex->buf_size = (ULONG)(need * h);
        r->ex->buf = (UBYTE *)AllocVec(r->ex->buf_size, MEMF_ANY);
        if (!r->ex->buf) {
            r->ex->buf_size = 0;
            return 0;
        }
    }
    WaitBlit();
    memset(r->ex->bm->Planes[0], 0, (size_t)r->ex->bm->BytesPerRow * (size_t)r->ex->bm_h);
    InitRastPort(&r->ex->rp);
    r->ex->rp.BitMap = r->ex->bm;
    SetFont(&r->ex->rp, r->font);
    return 1;
}

/* What was drawn in the scratch bitmap (w by h) written to `win` at px, py: ink 1 as fg, ink 0 as bg (an ink of
 * the window's, a pen or a direct colour). */
static void exact_end(vr_render *r, struct RastPort *win, WORD px, WORD py, LONG w, LONG h, ULONG fg, ULONG bg)
{
    LONG need = (w + 15) & ~15L, x, y;
    const UBYTE *pl;
    WaitBlit();
    pl = (const UBYTE *)r->ex->bm->Planes[0];
    for (y = 0; y < h; y++) {
        const UBYTE *s = pl + y * r->ex->bm->BytesPerRow;
        UBYTE *d = r->ex->buf + y * need;
        for (x = 0; x < w; x++)
            d[x] = (UBYTE)((s[x >> 3] >> (7 - (x & 7))) & 1);
    }
    r->ex->ctab[0] = bg & VR_INK_RGB ? bg & 0xFFFFFFUL : pen_rgb(r, bg);
    r->ex->ctab[1] = fg & 0xFFFFFFUL;
    vr_cgx_write_lut(r->cgx, (APTR)r->ex->buf, 0, 0, (UWORD)need, win, (APTR)r->ex->ctab, (UWORD)px, (UWORD)py, (UWORD)w,
                     (UWORD)h, VR_CTABFMT_XRGB8);
}

/* A run of text in a direct colour: drawn by flush_run itself into the scratch bitmap. Returns 0 when it cannot
 * (no memory; an underline of another colour than the text's): the caller draws the run through pens. */
static int flush_run_exact(vr_render *r, UBYTE *run, int n, WORD px, WORD py, const vr_style *st)
{
    LONG w = (LONG)n * r->cw, h = r->ch;
    struct RastPort *win = r->rp;
    vr_style ms;
    if (st->ul != st->fg && (st->attr & VT_ATTR_UNDERLINE))
        return 0;
    if (!exact_begin(r, w, h))
        return 0;
    ms = *st;
    ms.fg = ms.fgf = ms.ul = 1;
    ms.bg = ms.bgf = 0;
    r->rp = &r->ex->rp;
    flush_run(r, run, n, 0, 0, &ms);
    r->rp = win;
    exact_end(r, win, px, py, w, h, st->fg, st->bg);
    return 1;
}

static void draw_special(vr_render *r, WORD px, WORD py, vt_glyph g, ULONG fg, ULONG bg, int cells);

/* A box-drawing, diagonal or missing-glyph cell in a direct colour, the same way */
static int draw_special_exact(vr_render *r, WORD px, WORD py, vt_glyph g, ULONG fg, ULONG bg, int cells)
{
    LONG w = (LONG)r->cw * (g.kind == VT_GLYPH_MISSING && g.code == 2 ? 2 : cells), h = r->ch;
    struct RastPort *win = r->rp;
    if (!exact_begin(r, w, h))
        return 0;
    r->rp = &r->ex->rp;
    draw_special(r, 0, 0, g, 1, 0, cells);
    r->rp = win;
    exact_end(r, win, px, py, w, h, fg, bg);
    r->blank = 0;
    return 1;
}

/* A run whose background is filled first and its glyphs then drawn over it in JAM1: a dither (W-T7), or a
 * direct colour on a true-colour screen with no scratch pen (JAM2 would paint the glyph cells in the
 * nearest pen, not the colour the fill gives the cells around them). */
static int bg_apart(const vr_render *r, const vr_style *st)
{
    return IS_DITH(st->bgf) || ((st->bgf & VR_INK_RGB) && r->scratch[1] < 0);
}

static void flush_run(vr_render *r, UBYTE *run, int n, WORD px, WORD py, const vr_style *st)
{
    int i;
    struct TextFont *font = r->font;
    if (!n)
        return;
    if (!(st->attr & LINE_ATTRS) && !(st->deco & VT_DECO_IDEO_MASK)) {
        /* a run of blanks (erases, clears, line ends) is a rectangle fill:
         * much cheaper than rendering spaces through the font */
        for (i = 0; i < n && run[i] == ' '; i++)
            ;
        if (i == n) {
            /* a blank screen's blank cells are there already (a flood of
             * newlines redrew 32 empty rows a frame) */
            if (!(r->was_blank && st->bgf == r->bg_ink))
                fill(r, px, py, px + n * r->cw - 1, py + r->ch - 1, st->bgf);
            return;
        }
    }
    if (exact_ink(r, st->fg) && flush_run_exact(r, run, n, px, py, st)) {
        r->n_text++;
        r->blank = 0;
        return;
    }
    if (bg_apart(r, st)) {
        /* text on a dithered background (W-T7): the background's pattern,
         * then the glyphs over it in JAM1 -- through graphics.library, so a
         * composed planar pass is drawn in place */
        if (cpu_refuses(r))
            return;
        fill(r, px, py, px + n * r->cw - 1, py + r->ch - 1, st->bgf);
    } else if (painter_run(r, run, n, px, py, st) || cpu_refuses(r)) {
        return;
    }
    r->n_text++;
    r->blank = 0;
    if (st->font && st->font <= 10 && r->alt_font[st->font])
        font = r->alt_font[st->font];
    if (!(st->attr & (VT_ATTR_SUPER | VT_ATTR_SUB)) || !draw_script(r, run, n, px, py, st, font)) {
        if (font != r->font)
            SetFont(r->rp, font);
        if (bg_apart(r, st)) {
            ink_a(r, st->fg);
            SetDrMd(r->rp, JAM1);
        } else {
            ink_ab(r, st->fg, st->bg);
        }
        /* Bold as the glyphs again 1 px right, in JAM1: the soft style's
         * smear made Text() one pixel wider than the run and painted the
         * next cell's first column in this run's background, which a
         * partial redraw (a blink, one damaged cell) then left behind
         * (rig, vttest menu 2: after bold inverse blinking text). */
        SetSoftStyle(r->rp, style_of(st->attr) & ~FSF_BOLD, FSF_BOLD | FSF_UNDERLINED | FSF_ITALIC);
        Move(r->rp, px, py + r->base);
        Text(r->rp, (STRPTR)run, n);
        if (st->attr & VT_ATTR_BOLD) {
            SetDrMd(r->rp, JAM1);
            Move(r->rp, (WORD)(px + 1), py + r->base);
            Text(r->rp, (STRPTR)run, n);
        }
        if (font != r->font)
            SetFont(r->rp, r->font);
    }
    if ((st->attr & LINE_ATTRS & ~VT_ATTR_UNDERLINE) || (st->attr & VT_ATTR_UNDERLINE) ||
        (st->deco & VT_DECO_IDEO_MASK))
        decorate(r, n, px, py, st);
}

/* The font's glyphs as bytes, for the planar path: only a font exactly 8
 * pixels wide (then a cell is one byte of every plane).
 *
 * The planar path is built only with DIRECT=1 (VTCON_DIRECT). Measured on
 * the cycle-exact rig, 68020, AGA hires 4 planes, 500-line type
 * (2026-09-29): drawing took 6.6 s direct against 3.4 s through Text() --
 * the CPU's chip writes wait for display DMA while Text()'s blits run
 * beside the CPU. It stays for setups where it may win (a 68000 on a
 * custom screen, as retro32-term measured) -- measure before enabling. */
static void extract_glyphs(vr_render *r)
{
    struct TextFont *tf = r->font;
    const UBYTE *data;
    const ULONG *loc;
    int c, row, h = tf->tf_YSize;
    r->glyphs = 0;
    if (tf->tf_XSize != 8 || (tf->tf_Flags & FPF_PROPORTIONAL) || h < 1 || h > 32)
        return;
    r->glyphs = (UBYTE *)AllocVec(256 * h, MEMF_ANY | MEMF_CLEAR);
    if (!r->glyphs)
        return;
    data = (const UBYTE *)tf->tf_CharData;
    loc = (const ULONG *)tf->tf_CharLoc;
    for (c = tf->tf_LoChar; c <= tf->tf_HiChar; c++) {
        ULONG bitoff = loc[c - tf->tf_LoChar] >> 16;
        WORD bits = (WORD)(loc[c - tf->tf_LoChar] & 0xFFFF);
        const UBYTE *src = data + (bitoff >> 3);
        WORD sh = (WORD)(bitoff & 7);
        UBYTE *dst = r->glyphs + c * h;
        for (row = 0; row < h; row++) {
            UBYTE g = (UBYTE)(src[0] << sh);
            if (sh)
                g |= (UBYTE)(src[1] >> (8 - sh));
            if (bits < 8)
                g &= (UBYTE)(0xFF00 >> bits);
            dst[row] = g;
            src += tf->tf_Modulo;
        }
    }
}

/* May the CPU write the screen's bitplanes under the text area now (at
 * any bit phase; aligned: on a byte boundary)? The caller holds the
 * window's layer lock. */
static int screen_planes_ok(vr_render *r, int aligned)
{
    struct Window *w = r->win;
    struct BitMap *bm = r->rp->BitMap;
    struct ClipRect *cr = w->WLayer ? w->WLayer->ClipRect : 0;
    WORD sx0 = w->LeftEdge + r->ox, sy0 = w->TopEdge + r->oy;
    WORD sx1 = sx0 + vis_cols(r) * r->cw - 1, sy1 = sy0 + vis_rows(r) * r->ch - 1;
    if (!bm || bm->Depth > 8 || (aligned && (sx0 & 7)))
        return 0;
    if (bm != r->chip_bm || bm->Planes[0] != r->chip_plane0) {
        /* Once a bitmap (it is asked for every run the painter draws: S1,
         * sgr-colour paid ~1 ms a run): RTG is not planar, and Picasso96
         * calls its bitmaps standard too (it hung the rig, 2026-09-29) --
         * a native display bitmap is planes in chip RAM, graphics card
         * memory never is. */
        int p, ok = (GetBitMapAttr(bm, BMA_FLAGS) & BMF_STANDARD) != 0;
        for (p = 0; ok && p < bm->Depth; p++)
            if (!bm->Planes[p] || !(TypeOfMem(bm->Planes[p]) & MEMF_CHIP))
                ok = 0;
        r->chip_bm = bm;
        r->chip_plane0 = bm->Planes[0];
        r->chip_ok = (UBYTE)ok;
    }
    if (!r->chip_ok)
        return 0;
    if (!cr || cr->Next || cr->obscured)
        return 0; /* covered in part: the layer draws for us */
    return cr->bounds.MinX <= sx0 && cr->bounds.MinY <= sy0 && cr->bounds.MaxX >= sx1 &&
           cr->bounds.MaxY >= sy1;
}

/* ...and cells written there now, at any bit phase (the painter, render/painter.h)? */
static int planes_ok(vr_render *r, int aligned)
{
    return r->glyphs && screen_planes_ok(r, aligned);
}

/* The planes the CPU paths write: the screen's (under the layer lock,
 * planes_ok), or a planar composed pass's own (r->cpu). Window pixel
 * (x, y) is bit x0 + x of row y0 + y; mask: the planes written. */
typedef struct vr_planes {
    UBYTE **planes;
    LONG bpr, x0, y0;
    WORD depth;
    UBYTE mask;
} vr_planes;

static void target(vr_render *r, vr_planes *t)
{
    if (r->cpu) {
        /* every plane: one not in the mask holds zeros on screen, and the
         * copy writes it only while the mask has it (copy_mask) */
        t->planes = r->back->planes;
        t->bpr = r->back->bpr;
        t->depth = r->back->depth;
        t->x0 = r->back->phase;
        t->y0 = 0;
        t->mask = 0xFF;
    } else {
        struct BitMap *bm = r->rp->BitMap;
        t->planes = (UBYTE **)bm->Planes;
        t->bpr = bm->BytesPerRow;
        t->depth = bm->Depth;
        t->x0 = r->win->LeftEdge;
        t->y0 = r->win->TopEdge;
        t->mask = r->mask;
    }
}

/* ...on a byte boundary of the screen (vr_asm_cell's one byte a cell). */
static int direct_ok(vr_render *r)
{
    return planes_ok(r, 1);
}

/* A run of plain text in one pair of pens, straight into the bitplanes at
 * whatever bit phase the window puts it (render/painter_68k.s: four cells
 * a long, BFINS). Text() drew a character at a time through the blitter
 * and the layer; on a stock A1200 a screenful of text took 130-260 ms
 * that way (S1 phase profile). 0 when the window is covered, the screen
 * is not planar or the font not 8 pixels wide: the caller uses Text(). */
static int painter_run(vr_render *r, const UBYTE *run, int n, WORD px, WORD py, const vr_style *st)
{
    struct Layer *layer = r->win->WLayer;
    vr_planes t;
    UBYTE pens;
    int ok;
    if (!r->planar || !r->glyphs || st->attr || st->deco || st->font || ((st->fg | st->bg) & VR_INK_RGB) ||
        st->fg > 255 || st->bg > 255)
        return 0;
    pens = (UBYTE)(st->fg | st->bg);
#ifdef VTCON_PROF
    struct EClockVal pe0, pe1;
    if (vtwin_timer)
        ReadEClock(&pe0);
#endif
    if (r->cpu) {
        ok = 1; /* the composed pass's own planes */
    } else {
        LockLayer(0, layer);
        ok = planes_ok(r, 0);
    }
    if (ok) {
        if (pens & ~r->mask) {
            /* a pen's plane not in use yet holds zeros: from here on it is
             * drawn too (as ink_pen does for the RastPort) */
            r->mask |= pens;
            if (r->mask_on)
                SetWriteMask(r->rp, r->mask);
        }
        r->seen |= pens;
        target(r, &t);
    }
    if (ok && !r->cpu) {
#ifdef VTCON_PROF
        {
            struct EClockVal w0, w1;
            if (vtwin_timer)
                ReadEClock(&w0);
            WaitBlit();
            if (vtwin_timer) {
                ReadEClock(&w1);
                vr_prof[1] += w1.ev_lo - w0.ev_lo;
            }
        }
#else
        WaitBlit(); /* the scroll and fills before it are in the planes first */
#endif
    }
    if (ok)
        vp_span_fast((vp_u8 **)t.planes, t.depth, t.bpr, (long)(t.x0 + px), (long)(t.y0 + py), r->glyphs,
                     r->font->tf_YSize, run, n, (int)st->fg, (int)st->bg, t.mask);
    if (!r->cpu)
        UnlockLayer(layer);
#ifdef VTCON_PROF
    if (vtwin_timer) {
        ReadEClock(&pe1);
        vr_prof[0] += pe1.ev_lo - pe0.ev_lo;
        vr_prof[2] += ok;
    }
#endif
    if (ok) {
        r->n_direct += n;
        r->blank = 0;
    }
    return ok;
}

/* One cell, straight into the planes: each plane byte row is the glyph,
 * its inverse, or a constant, as the two pens' bits say. */
#ifdef VR_ASM
void vr_asm_cell(UBYTE **planes, long depth, long off, long bpr, const UBYTE *rows, long h, long fg, long bg,
                 long mask);
#endif

static void direct_cell(vr_render *r, const vr_planes *t, int x, int y, UBYTE ch, UBYTE fg, UBYTE bg, vt_u8 attr)
{
    LONG bpr = t->bpr;
    LONG off = (t->y0 + r->oy + y * r->ch) * bpr + ((t->x0 + r->ox) >> 3) + x;
    const UBYTE *g0 = r->glyphs + ch * r->ch;
    UBYTE rows[32];
#ifndef VR_ASM
    int p;
#endif
    int k, h = r->ch;
    for (k = 0; k < h; k++) {
        UBYTE v = g0[k];
        if (attr & VT_ATTR_BOLD)
            v |= (UBYTE)(v >> 1); /* the algorithmic bold, as SetSoftStyle does */
        rows[k] = v;
    }
    if ((attr & VT_ATTR_UNDERLINE) && r->base + 1 < h)
        rows[r->base + 1] = 0xFF;
    if (attr & VT_ATTR_STRIKE)
        rows[h / 2] = 0xFF;
#ifdef VR_ASM
    /* the loop below in assembler (amiga_render_68k.s) */
    vr_asm_cell(t->planes, t->depth, off, bpr, rows, h, fg, bg, t->mask);
#else
    for (p = 0; p < t->depth; p++) {
        UBYTE *d = t->planes[p] + off;
        int f = (fg >> p) & 1, b = (bg >> p) & 1;
        if (!((t->mask >> p) & 1))
            continue; /* a plane not in use: it holds zeros, and both pens' bits are 0 */
        if (f == b) {
            UBYTE v = f ? 0xFF : 0x00;
            for (k = 0; k < h; k++, d += bpr)
                *d = v;
        } else if (f) {
            for (k = 0; k < h; k++, d += bpr)
                *d = rows[k];
        } else {
            for (k = 0; k < h; k++, d += bpr)
                *d = (UBYTE)~rows[k];
        }
    }
#endif
}

static int selected(const vr_render *r, int x, int gy)
{
    LONG ay = r->sel_ay, by = r->sel_by, t;
    int ax = r->sel_ax, bx = r->sel_bx, tx;
    if (!r->sel)
        return 0;
    gy += vt_lines_scrolled(r->t); /* to the selection's absolute rows */
    if (ay > by || (ay == by && ax > bx)) {
        tx = ax; ax = bx; bx = tx;
        t = ay; ay = by; by = t;
    }
    if (gy < ay || gy > by)
        return 0;
    if (gy == ay && x < ax)
        return 0;
    if (gy == by && x > bx)
        return 0;
    return 1;
}

/* Draw screen rows [y0, y1), columns [x0, x1): each shows grid row y - view. */
/* Cells for the planar path, collected per row: they are written in one go
 * with the layer locked, and the lock covers nothing else (a lock held
 * across pen allocation and Text() hung the rig, 2026-09-29). */
typedef struct dcell {
    WORD x;
    UBYTE ch, fg, bg, attr;
} dcell;

#define DCELL_MAX 160

/* Whether the planar path is the cheaper one for these cells: Text() draws
 * a run of equal colours in one call whatever its length, the planar path
 * pays per cell -- so it wins where the runs are short (a colour change
 * every few characters: conbench sgr-perchar took 1.7 ms a character
 * through Text() on the stock rig). A DIRECT=1 build takes it always. */
#define DIRECT_MIN_RUNS 4
static int direct_wins(const dcell *d, int n)
{
#ifdef VTCON_DIRECT
    return d && n > 0;
#else
    int i, runs = 1;
    for (i = 1; i < n; i++)
        if (d[i].fg != d[i - 1].fg || d[i].bg != d[i - 1].bg || d[i].attr != d[i - 1].attr ||
            d[i].x != d[i - 1].x + 1)
            if (++runs >= DIRECT_MIN_RUNS)
                return 1;
    return 0;
#endif
}

/* Write the collected cells of row y straight into the planes when the
 * layer allows it now; returns 0 when it does not (then the caller draws
 * them through the RastPort). */
static int direct_row(vr_render *r, int y, const dcell *d, int n)
{
    struct Layer *layer = r->win->WLayer;
    vr_planes t;
    int i, ok;
    UBYTE pens = 0;
    for (i = 0; i < n; i++)
        pens |= (UBYTE)(d[i].fg | d[i].bg);
    if (r->cpu) {
        ok = !((r->back->phase + r->ox) & 7); /* the composed planes: the screen's bit phase */
    } else {
        LockLayer(0, layer);
        ok = direct_ok(r);
    }
    if (ok) {
        if (pens & ~r->mask) {
            /* a pen's plane not in use yet: it holds zeros, the cells
             * below start writing it (as ink_pen does for the RastPort) */
            r->mask |= pens;
            if (r->mask_on)
                SetWriteMask(r->rp, r->mask);
        }
        r->seen |= pens;
        if (!r->cpu)
            WaitBlit(); /* earlier blits (scroll, fills, Text, the cursor) finish first */
        target(r, &t);
        for (i = 0; i < n; i++)
            direct_cell(r, &t, d[i].x, y, d[i].ch, d[i].fg, d[i].bg, d[i].attr);
    }
    if (!r->cpu)
        UnlockLayer(layer);
    r->n_direct += ok ? n : 0;
    if (ok)
        r->blank = 0;
    return ok;
}

/* The cell's colours swapped: the one reverse-video path of the selection,
 * the visual bell and the cursor (cursor_draw). */
static void style_swap(vr_style *st)
{
    ULONG tmp = st->fg;
    st->fg = st->bg;
    st->bg = tmp;
    tmp = st->fgf;
    st->fgf = st->bgf;
    st->bgf = tmp;
}

/* The style cell c draws with (selection and blink phase applied). */
static void cell_style(vr_render *r, const vt_cell *c, int selected_cell, vr_style *st)
{
    vt_color f, b, u;
    vt_resolve_colors(r->t, c, &f, &b);
    st->fg = pen_for(r, f, 0);
    st->bg = pen_for(r, b, 1);
    st->fgf = area_ink(r, f, st->fg);
    st->bgf = area_ink(r, b, st->bg);
    if (st->fg == st->bg && f != b)
        /* a screen with few pens (a 4-colour Workbench) gave the text the
         * background's pen: the text went invisible (rig 3.2, the line
         * editor's green command word). The plain text pen then, or the
         * background's opposite. Conceal and the blink's off phase hide
         * text on purpose below. Only the glyphs: an area of the colour
         * (a block's, fgf) is that colour's pen -- the default text pen
         * there drew a half block of two neighbouring colours half in it,
         * a quarter of UPDemo's plasma in the Workbench's grey (W-T7). */
        st->fg = st->bg == r->pen_default_fg ? r->pen_default_bg : r->pen_default_fg;
    if (selected_cell) {
        /* the profile's selection colours, else the swapped cell */
        if (r->sel_ink[0] != VR_KEEP)
            st->fg = st->fgf = r->sel_ink[0];
        if (r->sel_ink[1] != VR_KEEP)
            st->bg = st->bgf = r->sel_ink[1];
        if (r->sel_ink[0] == VR_KEEP && r->sel_ink[1] == VR_KEEP)
            style_swap(st);
    }
    if (r->bell_flash) {
        /* the visual bell's frame: the whole window reversed, the terminal's
         * own reverse-video mode untouched */
        style_swap(st);
    }
    u = vt_cell_underline_color(r->t, c);
    st->ul = u == VT_COLOR_DEFAULT ? st->fg : pen_for(r, u, 0);
    st->attr = (vt_attr)(c->attr & DRAWN_ATTRS);
    st->deco = c->deco;
    st->font = (vt_u8)vt_cell_font(r->t, c);
    if ((c->attr & VT_ATTR_BLINK) && vt_personality(r->t) != VT_PCANSI) {
        /* pcansi's blink is the iCE bright background, never a blink */
        r->has_blink = 1;
        if (c->attr & VT_ATTR_RAPID ? r->blink_fast_off : r->blink_slow_off) {
            st->fg = st->ul = st->bg; /* the off phase: text and lines hidden */
            st->fgf = st->bgf;
        }
    }
}

/* A cell the planar path can write: one byte a plane, nothing drawn on it. */
static int plain_style(const vr_style *st)
{
    return !(st->attr & ~(VT_ATTR_BOLD | VT_ATTR_UNDERLINE | VT_ATTR_STRIKE)) &&
           (st->deco & ~VT_DECO_UL_MASK) == 0 && (st->deco & VT_DECO_UL_MASK) <= VT_UL_SINGLE &&
           st->ul == st->fg && !st->font;
}

/* The character a cell draws: its own, or its cluster's composed with
 * the marks that fold into it (glyphmap's vt_compose_cell). */
static vt_u32 cell_char(vr_render *r, const vt_cell *c)
{
    vt_u32 cp[VT_CLUSTER_CPS];
    if (!VT_CELL_IS_CLUSTER(c))
        return c->ch;
    vt_compose_cell(cp, vt_cell_text(r->t, c, cp));
    return cp[0];
}

/* A DEC double-width or double-height row: each of its first half of cells
 * drawn two cells wide -- the glyph into a one-plane mask, scaled 2x wide
 * (and 2x tall for the height halves, of which the top or bottom half is
 * stamped). The whole row is drawn: its cells do not map 1:1 to pixels. */
static void draw_double_row(vr_render *r, int y, const vt_cell *c, int ncells, int size)
{
    WORD cw = r->cw, ch = r->ch, py = (WORD)(r->oy + y * ch);
    WORD sh = (WORD)(size == VT_LINE_DOUBLE_WIDTH ? ch : 2 * ch);
    struct BitMap *full = AllocBitMap(cw, ch, 1, BMF_CLEAR, 0);
    struct BitMap *big = AllocBitMap(2 * cw, sh, 1, BMF_CLEAR, 0);
    struct RastPort trp;
    struct BitScaleArgs bsa;
    int x, half = (r->cols + 1) / 2;
    vr_style st;
    if (cpu_refuses(r)) {
        if (full)
            FreeBitMap(full);
        if (big)
            FreeBitMap(big);
        return;
    }
    fill(r, r->ox, py, (WORD)(r->ox + vis_cols(r) * cw - 1), (WORD)(py + ch - 1), r->pen_default_bg);
    if (!full || !big) {
        if (full)
            FreeBitMap(full);
        if (big)
            FreeBitMap(big);
        return;
    }
    InitRastPort(&trp);
    trp.BitMap = full;
    SetAPen(&trp, 1);
    SetDrMd(&trp, JAM1);
    if (half > (vis_cols(r) + 1) / 2)
        half = (vis_cols(r) + 1) / 2;
    for (x = 0; x < half && x < ncells; x++) {
        WORD px = (WORD)(r->ox + x * 2 * cw), dw = (WORD)(dbl_cols(r, x) * cw);
        vt_glyph g;
        struct TextFont *font = r->font;
        UBYTE code;
        cell_style(r, &c[x], selected(r, x, y - r->view + r->jump), &st);
        fill(r, px, py, (WORD)(px + dw - 1), (WORD)(py + ch - 1), st.bg);
        g = vt_map_glyph(cell_char(r, &c[x]), r->enc);
        code = g.kind == VT_GLYPH_FONT ? g.code : (UBYTE)'?';
        if (st.font && st.font <= 10 && r->alt_font[st.font])
            font = r->alt_font[st.font];
        if (code != ' ') {
            SetFont(&trp, font);
            SetSoftStyle(&trp, style_of(st.attr), FSF_BOLD | FSF_ITALIC);
            SetRast(&trp, 0);
            Move(&trp, 0, r->base);
            Text(&trp, (STRPTR)&code, 1);
            bsa.bsa_SrcX = bsa.bsa_SrcY = 0;
            bsa.bsa_SrcWidth = (UWORD)cw;
            bsa.bsa_SrcHeight = (UWORD)ch;
            bsa.bsa_XSrcFactor = 1;
            bsa.bsa_XDestFactor = 2;
            bsa.bsa_YSrcFactor = (UWORD)ch;
            bsa.bsa_YDestFactor = (UWORD)sh;
            bsa.bsa_SrcBitMap = full;
            bsa.bsa_DestBitMap = big;
            bsa.bsa_DestX = bsa.bsa_DestY = 0;
            bsa.bsa_Flags = 0;
            BitMapScale(&bsa);
            WaitBlit();
            ink_a(r, st.fg);
            SetDrMd(r->rp, JAM1);
            BltTemplate((PLANEPTR)(big->Planes[0] +
                                   (size == VT_LINE_DOUBLE_BOTTOM ? (LONG)ch * big->BytesPerRow : 0)),
                        0, (WORD)big->BytesPerRow, r->rp, px, py, dw, ch);
        }
        if ((st.attr & LINE_ATTRS) || (st.deco & VT_DECO_IDEO_MASK))
            decorate(r, dbl_cols(r, x), px, py, &st);
    }
    WaitBlit();
    FreeBitMap(big);
    FreeBitMap(full);
    if (r->cursor_drawn && r->cursor_y == y)
        r->cursor_drawn = 0; /* the cursor cell was just painted over */
}

/* The glyph sources for what the bitmap font cannot show itself, as
 * vt_fallback_glyph asks them: the outline font (render/outline), then --
 * for a character with no stand-in -- Unifont's pages (render/unifont). */
static const vt_u8 *outline_src(void *f, vt_u32 cp, int cells, int *bpr)
{
    WORD b = 0;
    const UBYTE *m = vo_glyph((vo_font *)f, cp, cells, &b);
    *bpr = b;
    return m;
}

static const vt_u8 *unifont_src(void *c, vt_u32 cp, int cells, int *bpr)
{
    WaitBlit(); /* its one mask buffer may still be the last glyph's blit source */
    return uf_glyph(c, cp, cells, bpr);
}

static void fb_sync(vr_render *r)
{
    r->fb.enc = r->enc;
    r->fb.outline = r->outline ? outline_src : 0;
    r->fb.outline_src = r->outline;
    r->fb.unifont = r->unifont ? unifont_src : 0;
    r->fb.unifont_src = r->unifont;
    r->fb.colour = r->emoji.store ? ce_has : 0;
    r->fb.colour_src = &r->emoji;
}

/* The marks left over a drawn cell (vt_compose_cell could not fold them
 * into its character): the outline font's mark glyphs, in the text colour
 * on top. Without an outline font they are not drawn (copy keeps them). */
static void draw_marks(vr_render *r, WORD px, WORD py, const vt_u32 *mk, int n, int cells, const vr_style *st)
{
    int i;
    if (n > 0 && r->outline && cpu_refuses(r))
        return;
    for (i = 0; i < n && r->outline; i++) {
        WORD bpr;
        const UBYTE *m = vo_mark(r->outline, mk[i], cells, &bpr);
        if (!m)
            continue;
        ink_a(r, st->fg);
        SetDrMd(r->rp, JAM1);
        BltTemplate((PLANEPTR)m, 0, bpr, r->rp, px, py, (WORD)(cells * r->cw), r->ch);
        SetDrMd(r->rp, JAM2);
    }
}

/* An outline glyph over `cells` cells: the background, the mask in the
 * text colour (bold: again one pixel right, as Text() bold), the lines. */
static void draw_outline(vr_render *r, WORD px, WORD py, const UBYTE *m, WORD bpr, int cells,
                         const vr_style *st)
{
    WORD w = (WORD)(cells * r->cw);
    if (cpu_refuses(r))
        return;
    fill(r, px, py, (WORD)(px + w - 1), (WORD)(py + r->ch - 1), st->bg);
    ink_a(r, st->fg);
    SetDrMd(r->rp, JAM1);
    BltTemplate((PLANEPTR)m, 0, bpr, r->rp, px, py, w, r->ch);
    if (st->attr & VT_ATTR_BOLD)
        BltTemplate((PLANEPTR)m, 0, bpr, r->rp, (WORD)(px + 1), py, (WORD)(w - 1), r->ch);
    SetDrMd(r->rp, JAM2);
    if ((st->attr & LINE_ATTRS) || (st->deco & VT_DECO_IDEO_MASK))
        decorate(r, cells, px, py, st);
    r->n_outline++;
    r->blank = 0;
}

/* A colour emoji over two cells (render/emoji; ce_has found it): blended
 * over the cell's background -- st->bg is the effective one, inverse,
 * selection and the bell's flash already in it -- then the lines. Hidden
 * text (conceal, the blink's off phase: fg is bg) is the background
 * alone. 0 when nothing could be drawn (the caller draws the box). */
static int draw_colour(vr_render *r, WORD px, WORD py, const vr_style *st)
{
    if (cpu_refuses(r))
        return 1; /* drawn again in place */
    if (st->fg == st->bg)
        fill(r, px, py, (WORD)(px + 2 * r->cw - 1), (WORD)(py + r->ch - 1), st->bg);
    else if (!ce_paint(&r->emoji, px, py, ink_rgb(r, st->bg)))
        return 0;
    if ((st->attr & LINE_ATTRS) || (st->deco & VT_DECO_IDEO_MASK))
        decorate(r, 2, px, py, st);
    r->blank = 0;
    return 1;
}

/* ---- images (sixel) -------------------------------------------------------- */

/* The pen for an image colour on a palette screen: one obtained for it
 * (ObtainBestPen gives a free pen the colour, or the nearest one when the
 * screen has none left), kept until vr_free; past VR_IMG_MAX colours the
 * nearest of the xterm 256 the text uses. */
static UBYTE img_pen_for(vr_render *r, ULONG rgb)
{
    ULONG key = rgb | 0x01000000UL;
    int h = (int)(((rgb * 2654435761UL) >> 23) & (VR_IMG_SLOTS - 1)), i;
    LONG p;
    ULONG ink;
    for (i = 0; i < VR_IMG_SLOTS; i++, h = (h + 1) & (VR_IMG_SLOTS - 1)) {
        if (r->img->key[h] == key)
            return r->img->pen[h];
        if (!r->img->key[h])
            break;
    }
    if (i < VR_IMG_SLOTS && r->n_img_pens < VR_IMG_MAX && (p = obtain(r, rgb)) >= 0) {
        r->img->key[h] = key;
        r->img->pen[h] = (UBYTE)p;
        r->n_img_pens++;
        return (UBYTE)p;
    }
    ink = pen_for(r, VT_COLOR_RGB | rgb, 0);
    return (UBYTE)(ink & VR_INK_RGB ? r->pen_default_fg : ink);
}

/* The image's colours for this screen, once per image (and background). */
static void img_colours(vr_render *r, const vt_image_view *v)
{
    ULONG bgrgb = ink_rgb(r, r->bg_ink);
    int i;
    if (r->img_serial == v->serial && r->img_bgrgb == bgrgb)
        return;
    r->img_serial = v->serial;
    r->img_bgrgb = bgrgb;
    if (r->cgx) {
        r->img->ctab[0] = bgrgb; /* the pixels the image left unset */
        for (i = 1; i < v->npal; i++)
            r->img->ctab[i] = v->pal[i];
        return;
    }
    r->img->map[0] = r->bg_ink & VR_INK_RGB ? img_pen_for(r, bgrgb) : (UBYTE)r->bg_ink;
    r->img_planes = r->img->map[0];
    for (i = 1; i < v->npal; i++) {
        r->img->map[i] = img_pen_for(r, v->pal[i]);
        r->img_planes |= r->img->map[i];
    }
}

/* Cells [a, b) of screen row y show image v: its pixels there, clipped to
 * the image (a cell it does not fill keeps the background the text drew). */
static void img_run(vr_render *r, const vt_image_view *v, int a, int b, int y)
{
    LONG sx = (LONG)(a - v->col0) * v->cw, sy = v->py, w = (LONG)(b - a) * r->cw, h = r->ch;
    LONG stride, k, j;
    WORD dx = (WORD)(r->ox + a * r->cw), dy = (WORD)(r->oy + y * r->ch);
    const vt_u8 *src;
    UBYTE *d;
    if (sx < 0 || sx >= v->w || sy >= v->h)
        return;
    if (w > v->w - sx)
        w = v->w - sx;
    if (h > v->h - sy)
        h = v->h - sy;
    if (w <= 0 || h <= 0)
        return;
    if (!r->img) {
        /* the window's first image: its colour tables now (vr_images) */
        if (!(r->img = (struct vr_images *)AllocVec(sizeof(struct vr_images), MEMF_ANY | MEMF_CLEAR)))
            return;
        r->img_serial = 0; /* no table made yet */
    }
    img_colours(r, v);
    r->blank = 0;
    r->n_img_runs++;
    if (r->cgx) {
        vr_cgx_write_lut(r->cgx, (APTR)v->pix, (UWORD)sx, (UWORD)sy, (UWORD)v->w, r->rp, r->img->ctab, (UWORD)dx,
                         (UWORD)dy, (UWORD)w, (UWORD)h, VR_CTABFMT_XRGB8);
        return;
    }
    stride = (w + 15) & ~15L; /* WritePixelArray8 wants rows of 16 */
    if (r->img_buf_size < (ULONG)(stride * h)) {
        if (r->img_buf)
            FreeVec(r->img_buf);
        r->img_buf_size = (ULONG)(stride * h);
        r->img_buf = (UBYTE *)AllocVec(r->img_buf_size, MEMF_ANY);
        if (!r->img_buf) {
            r->img_buf_size = 0;
            return;
        }
    }
    for (k = 0; k < h; k++) {
        src = v->pix + (sy + k) * v->w + sx;
        d = r->img_buf + k * stride;
        for (j = 0; j < w; j++)
            d[j] = r->img->map[src[j]];
    }
    if ((UBYTE)(r->img_planes & ~r->mask)) {
        /* planes not in use held zeros: drawing may start to include them */
        r->mask |= r->img_planes;
        if (r->mask_on)
            SetWriteMask(r->rp, r->mask);
    }
    r->seen |= r->img_planes;
    if (GfxBase->LibNode.lib_Version >= 40) {
        WriteChunkyPixels(r->rp, dx, dy, dx + w - 1, dy + h - 1, r->img_buf, stride);
    } else {
        /* Kickstart 3.0: WritePixelArray8 and its one-row scratch rastport */
        struct RastPort tmp = *r->rp;
        tmp.Layer = 0;
        tmp.BitMap = AllocBitMap((ULONG)stride, 1, GetBitMapAttr(r->rp->BitMap, BMA_DEPTH), 0, r->rp->BitMap);
        if (tmp.BitMap) {
            WritePixelArray8(r->rp, dx, dy, dx + w - 1, dy + h - 1, r->img_buf, &tmp);
            FreeBitMap(tmp.BitMap);
        }
    }
}

/* The images on screen rows [y0, y1), cells [x0, x1): drawn over what
 * draw_rows put there, in the cells still marked as theirs, the oldest
 * first. draw_rows calls it only while the engine has images at all. */
static void draw_images(vr_render *r, int x0, int y0, int x1, int y1)
{
    vt_image_view v;
    const vt_cell *c;
    int y, i, x, a, xe, ncells, gy;
    for (y = y0; y < y1; y++) {
        gy = y - r->view + r->jump;
        c = vt_row(r->t, gy, &ncells);
        if (!c || (!r->view && vt_row_size(r->t, gy)))
            continue; /* a double-size row shows no images */
        for (i = 0; vt_row_image(r->t, gy, i, &v); i++) {
            xe = v.col0 + (v.w + v.cw - 1) / v.cw;
            if (xe > x1)
                xe = x1;
            if (xe > ncells)
                xe = ncells;
            for (x = v.col0 > x0 ? v.col0 : x0; x < xe;) {
                if (!(c[x].pad & VT_CELL_IMAGE)) {
                    x++;
                    continue;
                }
                for (a = x; x < xe && (c[x].pad & VT_CELL_IMAGE); x++)
                    ;
                if (cpu_refuses(r))
                    return;
                img_run(r, &v, a, x, y);
            }
        }
    }
}

/* ---- a synchronized frame composed off screen (W-T3, W-T4) -------------------- */

static void draw_rows_now(vr_render *r, int x0, int y0, int x1, int y1);
static void pad_flush(vr_render *r);
static void planar_present(vr_render *r);

/* What the composed pass drew so far into the window: one blit a rectangle. */
static void back_present(vr_render *r)
{
    int i;
#ifdef VTCON_PROF
    struct EClockVal b0, b1;
    if (vtwin_timer)
        ReadEClock(&b0);
    vr_prof[6] += r->back->n;
#endif
    for (i = 0; i < r->back->n; i++) {
        const WORD *b = r->back->rect[i];
        BltBitMapRastPort(r->back->bm, b[0], b[1], r->front_rp, b[0], b[1], (WORD)(b[2] - b[0] + 1),
                          (WORD)(b[3] - b[1] + 1), 0xC0);
    }
#ifdef VTCON_PROF
    if (vtwin_timer) {
        ReadEClock(&b1);
        vr_prof[5] += b1.ev_lo - b0.ev_lo;
    }
#endif
    r->back->n = 0;
}

/* Pixels x0..x1, y0..y1 of the back bitmap now hold what the window must show. */
static void back_add(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD *b;
    if (!r->front_rp || x1 < x0 || y1 < y0)
        return;
    if (r->back->n) {
        b = r->back->rect[r->back->n - 1];
        if (b[0] == x0 && b[2] == x1 && b[3] + 1 == y0) {
            b[3] = y1; /* the next rows of the same columns: one blit */
            return;
        }
    }
    if (r->back->n == VR_BACK_RECTS) {
        /* the list is full: these go now */
        if (r->cpu)
            planar_present(r);
        else
            back_present(r);
    }
    b = r->back->rect[r->back->n++];
    b[0] = x0;
    b[1] = y0;
    b[2] = x1;
    b[3] = y1;
}

/* The CPU's fill of window pixels x0..x1, y0..y1 in the composed planes,
 * every plane (target): pen's bit set or cleared, a byte at a time with
 * the two edge bytes masked. */
static void back_fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, UBYTE pen)
{
    struct vr_back *k = r->back;
    LONG b0, b1, i0, i1, i;
    UBYTE lm, rm;
    WORD y;
    int p;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= k->w)
        x1 = (WORD)(k->w - 1);
    if (y1 >= k->h)
        y1 = (WORD)(k->h - 1);
    if (x1 < x0 || y1 < y0)
        return;
    b0 = k->phase + x0;
    b1 = k->phase + x1;
    i0 = b0 >> 3;
    i1 = b1 >> 3;
    lm = (UBYTE)(0xFF >> (b0 & 7));
    rm = (UBYTE)(0xFF << (7 - (b1 & 7)));
    if (i0 == i1)
        lm = rm = (UBYTE)(lm & rm);
    for (p = 0; p < k->depth; p++) {
        UBYTE *row = k->planes[p] + (LONG)y0 * k->bpr;
        if ((pen >> p) & 1) {
            for (y = y0; y <= y1; y++, row += k->bpr) {
                row[i0] |= lm;
                for (i = i0 + 1; i < i1; i++)
                    row[i] = 0xFF;
                row[i1] |= rm;
            }
        } else {
            for (y = y0; y <= y1; y++, row += k->bpr) {
                row[i0] &= (UBYTE)~lm;
                for (i = i0 + 1; i < i1; i++)
                    row[i] = 0;
                row[i1] &= (UBYTE)~rm;
            }
        }
    }
}

/* back_fill with a dither ink: per byte of the planes the pattern's bits
 * (pat_bits), pen b's plane bit where set and pen a's elsewhere. */
static void back_fill_pat(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG ink)
{
    struct vr_back *k = r->back;
    UBYTE pa = ink_pen(r, DITH_A(ink), 0), pb = ink_pen(r, DITH_B(ink), 0), lm, rm, m, pat, v;
    LONG b0, b1, i0, i1, i;
    WORD y;
    int p;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= k->w)
        x1 = (WORD)(k->w - 1);
    if (y1 >= k->h)
        y1 = (WORD)(k->h - 1);
    if (x1 < x0 || y1 < y0)
        return;
    b0 = k->phase + x0;
    b1 = k->phase + x1;
    i0 = b0 >> 3;
    i1 = b1 >> 3;
    lm = (UBYTE)(0xFF >> (b0 & 7));
    rm = (UBYTE)(0xFF << (7 - (b1 & 7)));
    for (y = y0; y <= y1; y++)
        for (i = i0; i <= i1; i++) {
            m = (UBYTE)((i == i0 ? lm : 0xFF) & (i == i1 ? rm : 0xFF));
            pat = pat_bits(r, ink, (WORD)((i << 3) - k->phase), y);
            for (p = 0; p < k->depth; p++) {
                UBYTE *d = k->planes[p] + (LONG)y * k->bpr + i;
                v = (UBYTE)(((pb >> p) & 1 ? pat : 0) | ((pa >> p) & 1 ? (UBYTE)~pat : 0));
                *d = (UBYTE)((*d & ~m) | (v & m));
            }
        }
}

/* Bytes i0..i1 of one plane row from the composed planes to the screen's,
 * the edge bytes masked (lm, rm); the bytes between as longwords where
 * the two rows are aligned alike (the phase makes them so on a screen of
 * longword rows). */
static void copy_row(const UBYTE *s, UBYTE *d, LONG i0, LONG i1, UBYTE lm, UBYTE rm)
{
    LONG i;
    if (i0 == i1) {
        UBYTE m = (UBYTE)(lm & rm);
        d[i0] = (UBYTE)((d[i0] & ~m) | (s[i0] & m));
        return;
    }
    d[i0] = (UBYTE)((d[i0] & ~lm) | (s[i0] & lm));
    i = i0 + 1;
    if (!(((unsigned long)(s + i) ^ (unsigned long)(d + i)) & 3)) {
        for (; i < i1 && ((unsigned long)(d + i) & 3); i++)
            d[i] = s[i];
        for (; i + 4 <= i1; i += 4)
            *(unsigned int *)(d + i) = *(const unsigned int *)(s + i);
    }
    for (; i < i1; i++)
        d[i] = s[i];
    d[i1] = (UBYTE)((d[i1] & ~rm) | (s[i1] & rm));
}

/* The beam lines that show window rows y0..y1 (a conservative guess
 * from the View and the screen's ViewPort: one line a row, two rows a
 * line when interlaced); while the beam is on them (or just above), wait
 * for the next frame's top, so the copy runs ahead of the beam and every
 * row it writes is shown new in the same frame. 1 when it waited. */
#define VR_BEAM_LEAD 8
static int beam_wait(vr_render *r, WORD y0, WORD y1)
{
    struct View *v = GfxBase->ActiView;
    struct ViewPort *vp = r->win->WScreen ? &r->win->WScreen->ViewPort : 0;
    LONG top, bot, b;
    int sh;
    if (!v || !vp)
        return 0;
    sh = (vp->Modes & LACE) ? 1 : 0;
    top = v->DyOffset + ((vp->DyOffset + r->win->TopEdge + y0) >> sh);
    bot = v->DyOffset + ((vp->DyOffset + r->win->TopEdge + y1) >> sh);
    b = (LONG)VBeamPos();
    if (b < top - VR_BEAM_LEAD || b > bot)
        return 0;
    WaitTOF();
    return 1;
}

/* The composed planes' rectangles into the screen's, every plane of a
 * pixel row before the next row, top down, behind the beam (beam_wait);
 * 0 when the window cannot take it now (covered since the pass began). */
static int back_copy(vr_render *r)
{
    struct vr_back *k = r->back;
    struct Window *w = r->win;
    struct Layer *layer = w->WLayer;
    struct BitMap *bm;
    UBYTE cm = (UBYTE)(k->copy_mask | r->mask);
    WORD y0 = 0x7FFF, y1 = -1;
    int i, p, ok;
#ifdef VTCON_PROF
    struct EClockVal b0, b1;
#endif
    for (i = 0; i < k->n; i++) {
        if (k->rect[i][1] < y0)
            y0 = k->rect[i][1];
        if (k->rect[i][3] > y1)
            y1 = k->rect[i][3];
    }
#ifdef VTCON_PROF
    if (vtwin_timer)
        ReadEClock(&b0);
#endif
    i = beam_wait(r, y0, y1); /* not under the layer lock: it sleeps */
#ifdef VTCON_PROF
    if (vtwin_timer) {
        ReadEClock(&b1);
        vr_prof[7] += b1.ev_lo - b0.ev_lo;
        vr_prof[8] += i;
        b0 = b1;
    }
#endif
    LockLayer(0, layer);
    ok = screen_planes_ok(r, 0);
    if (ok) {
        LONG sbase;
        bm = r->rp->BitMap;
        sbase = (LONG)(w->LeftEdge >> 5) << 2; /* the screen byte of the composed planes' byte 0 */
        WaitBlit(); /* the window's blits so far (the cursor, a scroll) are in the planes first */
        for (i = 0; i < k->n; i++) {
            const WORD *b = k->rect[i];
            LONG bx0 = k->phase + b[0], bx1 = k->phase + b[2];
            UBYTE lm = (UBYTE)(0xFF >> (bx0 & 7)), rm = (UBYTE)(0xFF << (7 - (bx1 & 7)));
            WORD y;
            for (y = b[1]; y <= b[3]; y++) {
                const UBYTE *src = 0;
                UBYTE *dst;
                for (p = 0; p < k->depth; p++) {
                    if (!((cm >> p) & 1))
                        continue;
                    src = k->planes[p] + (LONG)y * k->bpr;
                    dst = (UBYTE *)bm->Planes[p] + (LONG)(w->TopEdge + y) * bm->BytesPerRow + sbase;
                    copy_row(src, dst, bx0 >> 3, bx1 >> 3, lm, rm);
                }
            }
        }
        r->n_copies++;
        r->blank = 0;
    }
    UnlockLayer(layer);
#ifdef VTCON_PROF
    if (vtwin_timer) {
        ReadEClock(&b1);
        vr_prof[5] += b1.ev_lo - b0.ev_lo;
        vr_prof[6]++;
    }
#endif
    return ok;
}

/* The cells drawn into the composed planes since the last copy, drawn
 * again in place (through the RastPort: the layer clips). */
static void replay(vr_render *r)
{
    struct vr_back *k = r->back;
    struct RastPort *f = r->front_rp;
    UBYTE cpu = r->cpu;
    int i;
    r->front_rp = 0;
    r->cpu = 0;
    for (i = 0; i < k->nd; i++)
        draw_rows_now(r, k->dmg[i][0], k->dmg[i][1], k->dmg[i][2], k->dmg[i][3]);
    r->front_rp = f;
    r->cpu = cpu;
}

/* A planar pass's planes so far to the window: copied, or -- covered
 * since the pass began -- the cells drawn again in place. The pass goes
 * on composing; inside draw_rows its cells so far are drawn again too
 * should it fail later (cur). */
static void planar_present(vr_render *r)
{
    struct vr_back *k = r->back;
    if (k->n && !back_copy(r))
        replay(r);
    k->n = 0;
    k->nd = 0;
    if (k->in_rows) {
        k->dmg[0][0] = k->cur[0];
        k->dmg[0][1] = k->cur[1];
        k->dmg[0][2] = k->cur[2];
        k->dmg[0][3] = k->cur[3];
        k->nd = 1;
    }
    k->copy_mask = r->mask;
}

/* A cell only graphics.library draws: the rest of the pass in place, and
 * what was composed since the last copy drawn again there. */
static void compose_abandon(vr_render *r)
{
    struct vr_back *k = r->back;
    r->cpu = 0;
    r->cpu_fail = 0;
    r->front_rp = 0;
    k->n = 0;
    k->in_rows = 0;
    replay(r);
    k->nd = 0;
}

/* Inside a composed pass, drawing that is not a damaged cell (the cursor, a
 * scroll) acts on the window itself: what is composed so far goes first.
 * to_back puts the pass back on the back bitmap (or the planar planes). */
static struct RastPort *to_front(vr_render *r)
{
    struct RastPort *f = r->front_rp;
    if (f) {
        if (r->cpu) {
            planar_present(r);
            r->cpu = 0;
        } else {
            back_present(r);
            r->rp = f;
        }
        r->front_rp = 0;
    }
    return f;
}

static void to_back(vr_render *r, struct RastPort *f)
{
    if (f) {
        r->front_rp = f;
        if (r->back->planar) {
            r->cpu = 1;
            r->back->copy_mask = r->mask;
        } else {
            r->rp = &r->back->rp;
        }
    }
}

/* A planar window's composed pass: its own planes, the window's size at
 * the window's bit phase, in any memory; 0 when the window is covered (the
 * layer must clip: in place) or there is no memory. */
static int planar_begin(vr_render *r)
{
    struct Window *w = r->win;
    struct vr_back *k = r->back;
    struct BitMap *bm = r->rp->BitMap;
    LONG bpr, plane;
    WORD phase;
    int ok, p;
    LockLayer(0, w->WLayer);
    ok = screen_planes_ok(r, 0);
    UnlockLayer(w->WLayer);
    if (!ok)
        return 0;
    phase = (WORD)(w->LeftEdge & 31);
    bpr = (LONG)((phase + w->Width + 31) >> 5) << 2;
    plane = bpr * w->Height;
    if (!k->pmem || k->psize < (ULONG)(plane * bm->Depth)) {
        if (k->pmem)
            FreeVec(k->pmem);
        k->psize = (ULONG)(plane * bm->Depth);
        k->pmem = (UBYTE *)AllocVec(k->psize, MEMF_ANY);
        if (!k->pmem) {
            k->psize = 0;
            return 0; /* no memory for it: drawn in place, as before */
        }
    }
    for (p = 0; p < bm->Depth; p++)
        k->planes[p] = k->pmem + p * plane;
    k->bpr = bpr;
    k->depth = bm->Depth;
    k->phase = phase;
    k->w = w->Width;
    k->h = w->Height;
    k->n = 0;
    k->nd = 0;
    k->in_rows = 0;
    k->planar = 1;
    k->copy_mask = r->mask;
    r->front_rp = r->rp; /* the window's, as it stays: only the CPU paths draw, into k->planes */
    r->cpu = 1;
    r->cpu_fail = 0;
    return 1;
}

int vr_compose_begin(vr_render *r)
{
    struct Window *w = r->win;
    struct vr_back *k;
    long f;
    if (!w || !r->t || r->front_rp)
        return 0;
    f = vt_sync_frames(r->t);
    if (f == r->sync_seen)
        return 0; /* no frame ended: output as it comes, drawn in place */
    r->sync_seen = f;
    if (r->hidden || r->off || r->view)
        return 0;
    if (!r->back) {
        r->back = (struct vr_back *)AllocVec(sizeof(struct vr_back), MEMF_ANY | MEMF_CLEAR);
        if (!r->back)
            return 0;
    }
    if (r->planar)
        return planar_begin(r);
    k = r->back;
    k->planar = 0;
    if (!k->bm || k->w < w->Width || k->h < w->Height) {
        if (k->bm)
            FreeBitMap(k->bm);
        /* a friend of the window's bitmap: the same pixel format, on the
         * graphics card, where a blit to the window is the card's copy */
        k->bm = AllocBitMap((ULONG)w->Width, (ULONG)w->Height, GetBitMapAttr(w->RPort->BitMap, BMA_DEPTH),
                            BMF_MINPLANES, w->RPort->BitMap);
        if (!k->bm)
            return 0; /* no memory for it: drawn in place, as before */
        k->w = w->Width;
        k->h = w->Height;
    }
    InitRastPort(&k->rp);
    k->rp.BitMap = k->bm;
    SetFont(&k->rp, r->font);
    k->n = 0;
    r->front_rp = r->rp;
    r->rp = &k->rp;
    return 1;
}

void vr_compose_end(vr_render *r)
{
    to_front(r);
    pad_flush(r);
}

/* Draw screen rows [y0, y1), columns [x0, x1): in a planar composed pass
 * the cells are kept (dmg) until copied, to be drawn again in place should
 * one of them need graphics.library (cpu_fail). */
static void draw_rows(vr_render *r, int x0, int y0, int x1, int y1)
{
    struct vr_back *k = r->back;
    if (!r->cpu) {
        draw_rows_now(r, x0, y0, x1, y1);
        pad_flush(r);
        return;
    }
    if (k->nd == VR_BACK_RECTS)
        planar_present(r);
    k->cur[0] = k->dmg[k->nd][0] = (WORD)x0;
    k->cur[1] = k->dmg[k->nd][1] = (WORD)y0;
    k->cur[2] = k->dmg[k->nd][2] = (WORD)x1;
    k->cur[3] = k->dmg[k->nd][3] = (WORD)y1;
    k->nd++;
    k->in_rows = 1;
    draw_rows_now(r, x0, y0, x1, y1);
    k->in_rows = 0;
    if (r->cpu_fail)
        compose_abandon(r);
}

/* A run of cells from x that are two flat halves each -- a blank, an upper
 * or lower half block, a full block, in a plain style -- in a planar
 * composed pass: per plane the top half's row of bits and the bottom
 * half's are made once (a byte a cell, at the window's bit phase) and
 * stored in each pixel row of their half. The same pixels draw_special's
 * fills make (draw_block: a half is (h * 4 + 4) / 8 rows, so only an even
 * cell height splits evenly); generic, those were two fills and a style
 * and glyph lookup a cell (UPDemo's plasma: 2310 cells a frame, W-T4).
 * A dithered half (W-T7) is still a byte a plane a pixel row: its pattern's
 * row (pat_bits), so a run with one makes its bits row by row.
 * Returns the cells written (0: the cell at x is not one). */
static int block_run(vr_render *r, const vt_cell *c, int x, int xe, WORD py)
{
    struct vr_back *k = r->back;
    WORD h = r->ch, s = (WORD)((h * 4 + 4) / 8), row;
    const vt_cell *last = 0;
    vr_style st;
    ULONG top, bot;
    UBYTE pens = 0, dith = 0;
    int n = 0, i, p;
    LONG b0, i0;
    UBYTE sh, lm;
    if (r->cw != 8 || s * 2 != h || r->outline || r->sel)
        return 0;
    st.fgf = st.bgf = 0;
    while (x + n < xe && n < VR_STRIP) {
        const vt_cell *cc = &c[x + n];
        vt_glyph g;
        if (cc->width != 1 || cc->ext)
            break;
        if (!last || cc->fg != last->fg || cc->bg != last->bg || cc->attr != last->attr || cc->deco != last->deco) {
            cell_style(r, cc, 0, &st);
            if (st.attr || st.deco || st.font || ((st.fgf | st.bgf) & VR_INK_RGB) ||
                (st.fgf > 255 && !IS_DITH(st.fgf)) || (st.bgf > 255 && !IS_DITH(st.bgf)))
                break;
            last = cc;
        }
        if (cc->ch == ' ') {
            top = bot = st.bgf;
        } else {
            WORD bh;
            if (cc->ch < 0x80)
                break;
            g = vt_map_glyph(cc->ch, r->enc);
            if (g.kind != VT_GLYPH_BLOCK || (g.code & 0xC0) || ((g.code >> 4) & 3) > 1)
                break;
            bh = (WORD)((h * (g.code & 15) + 4) / 8);
            if (bh == h) {
                top = bot = st.fgf;
            } else if (bh == s) {
                top = (g.code >> 4) & 3 ? st.bgf : st.fgf;
                bot = (g.code >> 4) & 3 ? st.fgf : st.bgf;
            } else {
                break;
            }
        }
        /* a plain pen is a dither of level 0: pen a alone */
        k->tp[n] = DITH_A(top);
        k->bp[n] = DITH_A(bot);
        k->tb[n] = DITH_B(top);
        k->bb[n] = DITH_B(bot);
        k->tl[n] = (UBYTE)(IS_DITH(top) ? DITH_LEV(top) : 0);
        k->bl[n] = (UBYTE)(IS_DITH(bot) ? DITH_LEV(bot) : 0);
        dith |= (UBYTE)(k->tl[n] | k->bl[n]);
        pens |= (UBYTE)(k->tp[n] | k->bp[n] | (k->tl[n] ? k->tb[n] : 0) | (k->bl[n] ? k->bb[n] : 0));
        n++;
    }
    if (!n)
        return 0;
    /* the pens' planes in use from here on, as ink_pen does */
    ink_pen(r, pens, 0);
    r->blank = 0;
    b0 = k->phase + r->ox + (LONG)x * 8;
    i0 = b0 >> 3;
    sh = (UBYTE)(b0 & 7);
    lm = (UBYTE)(0xFF >> sh);
    for (p = 0; p < k->depth && dith; p++) {
        /* a dithered half in the run: each pixel row its own bits */
        UBYTE *dst = k->planes[p] + (LONG)py * k->bpr + i0;
        for (row = 0; row < h; row++, dst += k->bpr) {
            const UBYTE *pa = row >= s ? k->bp : k->tp, *pb = row >= s ? k->bb : k->tb;
            const UBYTE *lv = row >= s ? k->bl : k->tl;
            UBYTE *o = k->strip[0], prev = 0, v, pat;
            for (i = 0; i < n; i++) {
                pat = r->dith && lv[i] ? r->dith->pat[lv[i]][row & 7] : 0;
                v = (UBYTE)(((pb[i] >> p) & 1 ? pat : 0) | ((pa[i] >> p) & 1 ? (UBYTE)~pat : 0));
                o[i] = (UBYTE)((prev << (8 - sh)) | (v >> sh));
                prev = v;
            }
            o[n] = (UBYTE)(prev << (8 - sh));
            dst[0] = (UBYTE)((dst[0] & ~lm) | (o[0] & lm));
            for (i = 1; i < n; i++)
                dst[i] = o[i];
            if (sh)
                dst[n] = (UBYTE)((dst[n] & lm) | (o[n] & ~lm));
        }
    }
    for (p = 0; p < k->depth && !dith; p++) {
        UBYTE *dst = k->planes[p] + (LONG)py * k->bpr + i0;
        int half;
        for (half = 0; half < 2; half++) {
            const UBYTE *pen = half ? k->bp : k->tp;
            UBYTE *o = k->strip[half], prev = 0, v;
            for (i = 0; i < n; i++) {
                v = (UBYTE)((pen[i] >> p) & 1 ? 0xFF : 0x00);
                o[i] = (UBYTE)((prev << (8 - sh)) | (v >> sh));
                prev = v;
            }
            o[n] = (UBYTE)(prev << (8 - sh));
        }
        for (row = 0; row < h; row++, dst += k->bpr) {
            const UBYTE *o = k->strip[row >= s];
            dst[0] = (UBYTE)((dst[0] & ~lm) | (o[0] & lm));
            for (i = 1; i < n; i++)
                dst[i] = o[i];
            if (sh)
                dst[n] = (UBYTE)((dst[n] & lm) | (o[n] & ~lm));
        }
    }
    return n;
}

/* What a default blank cell looks like now -- the dialect's default
 * background (an Amiga-dialect window's global background pen), reverse
 * video, a bell's flash -- into r->bg_ink, the window's background: what
 * "blank" means, and what the strips beside the grid and the rows a scroll
 * vacates are given. r->bs_ok: plain, so a row's tail of them can be one
 * fill. Once a render pass: it cost every damaged row a cell_style()
 * (conbench cursor-pos: seven rows a frame; S1); vr_set_defaults drops it. */
static void blank_look(vr_render *r)
{
    vt_cell b;
    vr_style bs;
    if (r->in_pass && r->bs_valid)
        return;
    b.ch = ' ';
    b.fg = VT_COLOR_DEFAULT;
    b.bg = VT_COLOR_DEFAULT;
    b.attr = 0;
    b.width = 1;
    b.deco = 0;
    b.ext = 0;
    b.pad = 0;
    cell_style(r, &b, 0, &bs);
    r->bs_ok = (UBYTE)(!bs.attr && !bs.deco && !bs.font);
    r->bs_valid = 1;
    if (bs.bg != r->bg_ink) {
        r->bg_ink = bs.bg; /* another background: nothing on screen is known blank */
        r->blank = 0;
    }
}

/* The inside of the window around the grid -- above it (a console's top
 * offset), left of it, and the strips right of and below the last whole
 * cell -- in the window's background, as xterm paints its internal border
 * in the default background: they are the window's, not Intuition's pen 0
 * (W-T12). In place: a composed pass copies only the grid's cells. */
static void pad_fill(vr_render *r)
{
    struct Window *w = r->win;
    struct RastPort *f = to_front(r);
    WORD top = (WORD)(w->BorderTop + r->inset_top), left = w->BorderLeft;
    WORD gx1 = (WORD)(r->ox + vis_cols(r) * r->cw), gy1 = (WORD)(r->oy + vis_rows(r) * r->ch);
    WORD ax1 = (WORD)(w->Width - w->BorderRight - 1), ay1 = (WORD)(w->Height - w->BorderBottom - 1);
    UBYTE keep = r->was_blank;
    fill(r, left, top, ax1, (WORD)(r->oy - 1), r->bg_ink);
    fill(r, left, r->oy, (WORD)(r->ox - 1), ay1, r->bg_ink);
    fill(r, gx1, r->oy, ax1, ay1, r->bg_ink);
    fill(r, r->ox, gy1, (WORD)(gx1 - 1), ay1, r->bg_ink);
    to_back(r, f);
    r->pad_ink = r->bg_ink;
    r->was_blank = keep;
}

/* W-T14, padding-color = extend. Ghostty's window-padding-color = extend
 * (docs: https://ghostty.org/docs/config/reference#window-padding-color,
 * quoted from memory, not fetched: "background" paints the padding in the
 * terminal background, "extend" extends the background colour of the nearest
 * cell on each edge into the padding, so an application with its own
 * background -- an editor, a status line -- has no differently coloured
 * frame; Ghostty leaves the padding alone when a cell's background is the
 * default). The colour of the displayed cell at column x, row y (clamped to
 * the grid by the caller): its area ink (a pattern of two pens on a screen of few pens). */
static ULONG edge_ink(vr_render *r, int x, int y)
{
    int n, gy = y - r->view + r->jump;
    const vt_cell *row = vt_row(r->t, gy, &n);
    vt_cell b;
    vr_style st;
    if (row && vt_row_size(r->t, gy))
        x /= 2; /* a double-size row holds a cell for two columns */
    if (row && x < n) {
        b = row[x];
    } else {
        b.ch = ' ';
        b.fg = VT_COLOR_DEFAULT;
        b.bg = VT_COLOR_DEFAULT;
        b.attr = 0;
        b.width = 1;
        b.deco = 0;
        b.ext = 0;
        b.pad = 0;
    }
    cell_style(r, &b, 0, &st);
    return st.bgf;
}

/* The strips beside the grid in the colour of the cell next to them, a run
 * of equal cells one fill. In place on the window (the caller has taken a
 * composed pass to the front), all planes written: the strips hold pens the
 * grid's narrowed write mask may leave out. */
static void pad_extend_now(vr_render *r)
{
    struct Window *w = r->win;
    WORD top = (WORD)(w->BorderTop + r->inset_top), left = w->BorderLeft;
    int cols = vis_cols(r), rows = vis_rows(r), side, i, start;
    WORD gx1, gy1, ax1, ay1;
    UBYTE keep = r->was_blank, blank = r->blank, mask_on = r->mask_on;
    ULONG ink, cur;
    if (r->hidden || cols < 1 || rows < 1)
        return;
    gx1 = (WORD)(r->ox + cols * r->cw);
    gy1 = (WORD)(r->oy + rows * r->ch);
    ax1 = (WORD)(w->Width - w->BorderRight - 1);
    ay1 = (WORD)(w->Height - w->BorderBottom - 1);
    if (mask_on) {
        r->mask_on = 0; /* ink_pen widens the mask, but must not narrow the write mask now */
        SetWriteMask(r->rp, 0xFF);
    }
    /* left and right, the corners with them: per grid row, the first or last column's cell */
    for (side = 0; side < 2; side++) {
        WORD x0 = side ? gx1 : left, x1 = side ? ax1 : (WORD)(r->ox - 1);
        int col = side ? cols - 1 : 0;
        if (x1 < x0)
            continue;
        start = 0;
        cur = edge_ink(r, col, 0);
        for (i = 1; i <= rows; i++) {
            ink = i < rows ? edge_ink(r, col, i) : cur;
            if (i == rows || ink != cur) {
                WORD y0 = start ? (WORD)(r->oy + start * r->ch) : top;
                WORD y1 = i == rows ? ay1 : (WORD)(r->oy + i * r->ch - 1);
                fill(r, x0, y0, x1, y1, cur);
                start = i;
                cur = ink;
            }
        }
    }
    /* above and below the grid, between the corners: per column, the first or last row's cell */
    for (side = 0; side < 2; side++) {
        WORD y0 = side ? gy1 : top, y1 = side ? ay1 : (WORD)(r->oy - 1);
        int row = side ? rows - 1 : 0;
        if (y1 < y0)
            continue;
        start = 0;
        cur = edge_ink(r, 0, row);
        for (i = 1; i <= cols; i++) {
            ink = i < cols ? edge_ink(r, i, row) : cur;
            if (i == cols || ink != cur) {
                fill(r, (WORD)(r->ox + start * r->cw), y0, (WORD)(r->ox + i * r->cw - 1), y1, cur);
                start = i;
                cur = ink;
            }
        }
    }
    r->mask_on = mask_on;
    if (mask_on)
        SetWriteMask(r->rp, r->mask);
    r->blank = blank; /* the grid's pixels are untouched */
    r->was_blank = keep;
}

/* Cells were drawn: the strips follow them, once the pass is on the window
 * (a composed pass is not yet: vr_compose_end). */
static void pad_flush(vr_render *r)
{
    if (!r->pad_extend || !r->pad_dirty || r->front_rp)
        return;
    r->pad_dirty = 0;
    pad_extend_now(r);
}

static void draw_rows_now(vr_render *r, int x0, int y0, int x1, int y1)
{
    UBYTE run[RUN_MAX];
    dcell dc[DCELL_MAX];
    int y, x, xe, n, nd, want_direct, tail_ok;
    ULONG tail_bg;
    if (r->hidden)
        return;
    want_direct = r->glyphs != 0;
    if (x1 > vis_cols(r))
        x1 = vis_cols(r);
    if (y1 > vis_rows(r))
        y1 = vis_rows(r);
    blank_look(r);
    tail_ok = r->bs_ok && !r->sel;
    tail_bg = r->bg_ink;
    r->was_blank = (UBYTE)(r->blank && !r->cursor_drawn); /* a drawn cursor is pixels too */
    if (r->front_rp)
        r->was_blank = 0; /* composing: the back bitmap is not the window, every cell is drawn */
    if (x0 <= 0 && y0 <= 0 && x1 >= vis_cols(r) && y1 >= vis_rows(r) && !r->view) {
        r->blank = 1; /* the whole grid again: blank unless a row draws something */
        if (r->planar && r->mask_on && !r->cursor_drawn && !(tail_bg & VR_INK_RGB)) {
            /* Every cell is drawn again: afterwards the planes in use are
             * those of the pens this pass draws (r->seen), and no more --
             * a colour that was on screen once kept its planes in every
             * scroll after (CCON narrows at a form feed the same way).
             * The pass itself draws under the mask as it is, which holds
             * every plane that is not all zeros, so the planes dropped
             * come out as zeros. Cells skipped as blank hold the
             * background; the strips beside the grid are given it once. */
            r->full_pass = 1;
            r->seen = (UBYTE)tail_bg;
        }
    }
    if (r->pad_ink != tail_bg) {
        /* the window's background changed (OSC 11, reverse video, a
         * flash, a global background): the engine redraws every cell, and
         * the strips beside the grid follow it here, on every kind of
         * screen -- a true-colour window kept them in the old one */
        UBYTE blank = r->blank;
        pad_fill(r);
        r->blank = blank;
    }
    for (y = y0; y < y1; y++) {
        int ncells;
        const vt_cell *c = vt_row(r->t, y - r->view + r->jump, &ncells);
        int gy = y - r->view + r->jump;
        WORD py = r->oy + y * r->ch, run_x = 0;
        vr_style st, run_st;
        /* the last cell's look: runs of equal cells skip the lookups */
        const vt_cell *last = 0;
        if (r->cpu_fail)
            break; /* the pass goes in place (draw_rows) */
        if (!c)
            continue;
        if (!r->view && vt_row_size(r->t, gy)) {
            draw_double_row(r, y, c, ncells, vt_row_size(r->t, gy));
            back_add(r, r->ox, py, (WORD)(r->ox + vis_cols(r) * r->cw - 1), (WORD)(py + r->ch - 1));
            continue;
        }
        n = 0;
        nd = 0;
        x = x0;
        if (x > 0 && x < ncells && c[x].width == 0 && (r->outline || r->unifont || r->emoji.store))
            x--; /* the right half of a wide glyph: a mask glyph spans both, draw it whole */
        xe = x1 < ncells ? x1 : ncells;
        /* composing: these cells (and a wide glyph's right half past x1) are the window's next */
        back_add(r, (WORD)(r->ox + x * r->cw), py,
                 (WORD)(r->ox + (xe + (xe < ncells && c[xe].width == 0)) * r->cw - 1), (WORD)(py + r->ch - 1));
        if (tail_ok) {
            /* the row's tail of default blanks: one fill, or nothing on a
             * blank screen, instead of a look at every cell (a full
             * redraw of 80 x 32 empty cells took a whole frame of a 14
             * MHz 68020; S1) */
            int used = vt_row_used(r->t, gy), tx = used > x0 ? used : x0;
            if (tx < xe) {
                if (!r->was_blank)
                    fill(r, r->ox + tx * r->cw, py, r->ox + xe * r->cw - 1, py + r->ch - 1, tail_bg);
                xe = tx;
            }
        }
#ifdef VR_ASM
        if (r->planar && r->glyphs && !r->sel) {
            /* Runs of equal cells checked against their first in one
             * assembler loop and handed to the painter one run at a time
             * (S1: the C loop below cost ~70 us a cell on a stock A1200).
             * A plain row is one run; a coloured one (ls, sgr-colour) a
             * few. Where a run cannot go this way (a wide or non-ASCII
             * cell, a decorated style, a covered window) the loop below
             * takes the row on from there. */
            while (x < xe && c[x].width == 1 && c[x].ch < 0x80) {
                long k = vr_asm_row_scan(&c[x], xe - x < RUN_MAX ? xe - x : RUN_MAX, run);
                vr_style fs;
                if (!k)
                    break;
                cell_style(r, &c[x], 0, &fs);
                if (fs.attr || fs.deco || fs.font || fs.ul != fs.fg || IS_DITH(fs.bgf) ||
                    !painter_run(r, run, (int)k, r->ox + x * r->cw, py, &fs))
                    break;
                x += (int)k;
            }
            if (x >= xe) {
                if (r->cursor_drawn && r->cursor_y == y && r->cursor_x >= x0 && r->cursor_x < x1)
                    r->cursor_drawn = 0;
                continue;
            }
        }
#endif
        for (; x < xe; x++) {
            vt_glyph g;
            if (r->cpu) {
                int b = block_run(r, c, x, xe, py);
                if (b) {
                    flush_run(r, run, n, run_x, py, &run_st);
                    n = 0;
                    last = 0;
                    x += b - 1;
                    continue;
                }
            }
            if (c[x].width == 0) {
                /* the right half of a wide glyph: its '?' took the left */
                vr_style ws;
                cell_style(r, &c[x], selected(r, x, gy), &ws);
                flush_run(r, run, n, run_x, py, &run_st);
                n = 0;
                fill(r, r->ox + x * r->cw, py, r->ox + (x + 1) * r->cw - 1, py + r->ch - 1, ws.bgf);
                last = 0;
                continue;
            }
            if (!last || c[x].fg != last->fg || c[x].bg != last->bg || c[x].attr != last->attr ||
                c[x].deco != last->deco || c[x].ext != last->ext || r->sel) {
                cell_style(r, &c[x], selected(r, x, gy), &st);
                last = &c[x];
            }
            if (c[x].ch < 0x80) {
                g.kind = VT_GLYPH_FONT; /* ASCII: the font's own character */
                g.code = (vt_u8)c[x].ch;
            } else {
                int obpr = 0;
                const UBYTE *om;
                vt_u32 cp[VT_CLUSTER_CPS];
                int ncp = 1, cells = c[x].width == 2 ? 2 : 1;
                /* composed, then the font's own, the outline font,
                 * vt_map_glyph's stand-in, Unifont (an emoji over its two
                 * cells), or the replacement (glyphmap.h) */
                om = vt_cell_glyph(&r->fb, r->t, &c[x], cp, &ncp, &obpr, &g);
                if (om) {
                    WORD px = (WORD)(r->ox + x * r->cw);
                    flush_run(r, run, n, run_x, py, &run_st);
                    n = 0;
                    draw_outline(r, px, py, om, (WORD)obpr, cells, &st);
                    draw_marks(r, px, py, cp + 1, ncp - 1, cells, &st);
                    if (cells == 2 && x + 1 < ncells && c[x + 1].width == 0)
                        x++; /* its right half is drawn */
                    continue;
                }
                if (g.kind == VT_GLYPH_COLOUR) {
                    /* a colour emoji (U4), its base character only: the
                     * marks of a sequence (skin tone, ZWJ parts) are not drawn */
                    flush_run(r, run, n, run_x, py, &run_st);
                    n = 0;
                    if (draw_colour(r, (WORD)(r->ox + x * r->cw), py, &st)) {
                        if (x + 1 < ncells && c[x + 1].width == 0)
                            x++; /* its right half is drawn */
                        continue;
                    }
                    g.kind = VT_GLYPH_MISSING; /* code 2: the box below */
                }
                if (ncp > 1 && r->outline) {
                    /* marks to draw over it: the cell now, alone */
                    WORD px = (WORD)(r->ox + x * r->cw);
                    flush_run(r, run, n, run_x, py, &run_st);
                    n = 0;
                    if (g.kind == VT_GLYPH_FONT) {
                        run[0] = g.code;
                        flush_run(r, run, 1, px, py, &st);
                    } else {
                        draw_special(r, px, py, g, g.kind == VT_GLYPH_BLOCK ? st.fgf : st.fg, st.bgf, 1);
                    }
                    draw_marks(r, px, py, cp + 1, ncp - 1, cells, &st);
                    if (g.kind == VT_GLYPH_MISSING && cells == 2 && x + 1 < ncells && c[x + 1].width == 0)
                        x++;
                    continue;
                }
            }
            if (g.kind != VT_GLYPH_FONT) {
                flush_run(r, run, n, run_x, py, &run_st);
                n = 0;
                {
                    int cells = 1;
                    if (block_joins(g) && !r->sel) {
                        /* the same block in the same colours to the right: one pair of fills
                         * for the lot (UPDemo's plasma: 22 ms a frame of per-cell RectFill) */
                        while (x + cells < xe && cells < RUN_MAX && c[x + cells].ch == c[x].ch &&
                               c[x + cells].width == 1 && c[x + cells].fg == c[x].fg &&
                               c[x + cells].bg == c[x].bg && c[x + cells].attr == c[x].attr &&
                               c[x + cells].deco == c[x].deco && !c[x + cells].ext && !c[x].ext)
                            cells++;
                    }
                    draw_special(r, r->ox + x * r->cw, py, g, g.kind == VT_GLYPH_BLOCK ? st.fgf : st.fg, st.bgf,
                                 cells);
                    x += cells - 1;
                }
                if (g.kind == VT_GLYPH_MISSING && g.code == 2 && x + 1 < ncells && c[x + 1].width == 0)
                    x++; /* the box spans its right half */
                continue;
            }
            if (want_direct && plain_style(&st) && !IS_DITH(st.bgf) && nd < DCELL_MAX) {
                flush_run(r, run, n, run_x, py, &run_st);
                n = 0;
                dc[nd].x = (WORD)x;
                dc[nd].ch = g.code;
                dc[nd].fg = (UBYTE)st.fg; /* planar: never an RGB ink */
                dc[nd].bg = (UBYTE)st.bg;
                dc[nd].attr = (UBYTE)st.attr; /* plain: the low byte holds it */
                nd++;
                continue;
            }
            if (n && (!same_style(&st, &run_st) || n == RUN_MAX)) {
                flush_run(r, run, n, run_x, py, &run_st);
                n = 0;
            }
            if (!n) {
                run_x = r->ox + x * r->cw;
                run_st = st;
            }
            run[n++] = g.code;
        }
        flush_run(r, run, n, run_x, py, &run_st);
        if (nd && !(direct_wins(dc, nd) && direct_row(r, y, dc, nd))) {
            /* covered or not planar: the same cells through the RastPort,
             * one Text() per run of equal colours */
            int i = 0;
            while (i < nd) {
                int j = i;
                vr_style ds;
                n = 0;
                while (j < nd && dc[j].x == dc[i].x + (j - i) && dc[j].fg == dc[i].fg &&
                       dc[j].bg == dc[i].bg && dc[j].attr == dc[i].attr && n < RUN_MAX)
                    run[n++] = dc[j++].ch;
                ds.fg = ds.ul = ds.fgf = dc[i].fg;
                ds.bg = ds.bgf = dc[i].bg;
                ds.attr = dc[i].attr;
                ds.deco = (vt_u8)(dc[i].attr & VT_ATTR_UNDERLINE ? VT_UL_SINGLE : 0);
                ds.font = 0;
                flush_run(r, run, n, r->ox + dc[i].x * r->cw, py, &ds);
                i = j;
            }
        }
        if (r->cursor_drawn && r->cursor_y == y && r->cursor_x >= x0 && r->cursor_x < x1)
            r->cursor_drawn = 0; /* the cursor cell was just painted over */
    }
    if (vt_images(r->t))
        draw_images(r, x0, y0, x1, y1); /* the one test a frame pays when there are none */
    r->pad_dirty = r->pad_extend; /* W-T14: the strips follow these cells (pad_flush) */
    r->was_blank = 0;
    if (r->full_pass) {
        r->full_pass = 0;
        if (!r->cpu_fail) { /* else the pens are not all seen: the pass in place sets it */
            r->mask = r->seen;
            SetWriteMask(r->rp, r->mask_on ? r->mask : 0xFF);
        }
    }
    SetSoftStyle(r->rp, 0, FSF_BOLD | FSF_UNDERLINED | FSF_ITALIC);
}

/* A blinking cursor: ?12, or a blinking DECSCUSR shape (1, 3, 5). */
int vr_cursor_blinks(vr_render *r)
{
    int style = vt_cursor_style(r->t);
    return (vt_modes(r->t) & VT_MODE_CURSOR_BLINK) || style == 1 || style == 3 || style == 5;
}

/* One phase of a blink rate over: acc holds the time it has lasted. */
static int blink_due(ULONG *acc, ULONG us, ULONG period)
{
    *acc += us;
    if (*acc < period)
        return 0;
    *acc -= period;
    if (*acc >= period)
        *acc = 0; /* a long gap (the window was hidden): one change, not a burst */
    return 1;
}

/* Blinking cells and cursor (VR_BLINK_*_US). Returns 1 while any blinking
 * cell is on screen, so the caller keeps the frames coming. */
int vr_blink_tick(vr_render *r, ULONG us)
{
    int slow, fast, y, x, n, cursor;
    if (!r->win)
        return 0; /* no window (before vr_init, after vr_free) */
    cursor = vr_cursor_blinks(r);
    if ((!r->has_blink && !cursor) || r->hidden || r->view)
        return 0;
    slow = blink_due(&r->blink_slow_us, us, VR_BLINK_SLOW_US);
    fast = blink_due(&r->blink_fast_us, us, VR_BLINK_FAST_US);
    if (cursor && slow) {
        /* the cursor's own blink, at the slow rate */
        if (r->cursor_drawn)
            vr_cursor_off(r);
        else
            vr_cursor_on(r);
    }
    if (!r->has_blink)
        return cursor;
    if (!slow && !fast)
        return 1;
    if (slow)
        r->blink_slow_off = (BYTE)!r->blink_slow_off;
    if (fast)
        r->blink_fast_off = (BYTE)!r->blink_fast_off;
    r->has_blink = 0; /* found again by the rows that still blink */
    cursor = r->cursor_drawn; /* the cursor's phase, kept across the redraw */
    vr_cursor_off(r);
    for (y = 0; y < r->rows; y++) {
        const vt_cell *c = vt_row(r->t, y, &n);
        int x0 = -1, x1 = 0;
        if (!c)
            continue;
        for (x = 0; x < n && x < r->cols; x++)
            if ((c[x].attr & VT_ATTR_BLINK) &&
                (c[x].attr & VT_ATTR_RAPID ? fast : slow)) {
                if (x0 < 0)
                    x0 = x;
                x1 = x + 1;
            } else if (c[x].attr & VT_ATTR_BLINK) {
                r->has_blink = 1; /* the other rate: still on screen */
            }
        if (x0 >= 0)
            draw_rows(r, x0, y, x1, y + 1);
    }
    if (cursor)
        vr_cursor_on(r);
    return r->has_blink || vr_cursor_blinks(r);
}

void vr_bell_flash(vr_render *r)
{
    if (r->win)
        r->bell_flash = 1; /* drawn with the next render, over for the next */
}

int vr_flash_tick(vr_render *r)
{
    if (r->bell_flash) {
        r->bell_flash = 0;
        return 1;
    }
    return 0;
}

void vr_set_alt_font(vr_render *r, int n, struct TextFont *font)
{
    if (n < 1 || n > 10)
        return;
    /* a cell is the primary font's: another width would break the grid */
    if (font && (font->tf_XSize != r->cw || font->tf_YSize > r->ch))
        font = 0;
    r->alt_font[n] = font;
}

void vr_damage(vr_render *r, int x0, int y0, int x1, int y1)
{
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    if (r->view) {
        r->view_stale = 1; /* looking at the scrollback: the live rows are not shown, and the next move must redraw */
        return;
    }
    if (r->jump) {
        /* grid row g shows on screen row g - jump (vr_scroll's jump) */
        y0 -= r->jump;
        y1 -= r->jump;
        if (y0 < 0)
            y0 = 0;
        if (y1 <= y0)
            return;
    }
    draw_rows(r, x0, y0, x1, y1);
}

static void raw_scroll(vr_render *r, int top, int bottom, int n);

void vr_set_view(vr_render *r, int lines)
{
    int max;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    max = vt_scrollback_lines(r->t);
    if (lines < 0)
        lines = 0;
    if (lines > max)
        lines = max;
    if (lines == r->view)
        return;
    vr_settle(r);
    {
        int n, y0, y1, was = r->view;
        r->view = (WORD)lines;
        vr_cursor_off(r);
        /* Scrollback to scrollback, the screen unchanged since it was drawn: move the pixels
         * (vr_scroll's blit) and draw the rows the move exposed. A selection moves with its
         * lines; images are pixels; double-size rows are live-only. */
        if (!r->hidden && !r->off &&
            vti_view_scroll(was, lines, vis_rows(r), r->view_stale, &n, &y0, &y1)) {
            raw_scroll(r, 0, vis_rows(r), n);
            draw_rows(r, 0, y0, r->cols, y1);
            return;
        }
        vr_redraw(r);
    }
}

int vr_selection(const vr_render *r, int *ax, int *ay, int *bx, int *by)
{
    LONG base = vt_lines_scrolled(r->t);
    if (!r->sel)
        return 0;
    *ax = r->sel_ax;
    *bx = r->sel_bx;
    *ay = (int)(r->sel_ay - base);
    *by = (int)(r->sel_by - base);
    return 1;
}

void vr_select(vr_render *r, int on, int ax, int ay, int bx, int by)
{
    LONG base;
    long lo, hi;
    int y0, y1, was, any;
    WORD oax, obx;
    LONG oay, oby;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    base = vt_lines_scrolled(r->t);
    was = r->sel;
    oax = r->sel_ax;
    obx = r->sel_bx;
    oay = r->sel_ay;
    oby = r->sel_by;
    r->sel = (BYTE)on;
    r->sel_ax = (WORD)ax;
    r->sel_bx = (WORD)bx;
    r->sel_ay = ay + base;
    r->sel_by = by + base;
    /* repaint only the rows whose selected span changed (W42) */
    any = vti_sel_dirty(was, oax, oay, obx, oby, on, ax, r->sel_ay, bx, r->sel_by, &lo, &hi);
    if (!any)
        return;
    y0 = (int)(lo - base) + r->view;
    y1 = (int)(hi - base) + r->view + 1;
    if (y0 < 0)
        y0 = 0;
    if (y1 > r->rows)
        y1 = r->rows;
    vr_cursor_off(r);
    if (y0 < y1)
        draw_rows(r, 0, y0, r->cols, y1);
    vr_cursor_on(r);
}

void vr_redraw(vr_render *r)
{
    struct Window *w = r->win;
    WORD top;
    if (!w)
        return; /* no window (before vr_init, after vr_free) */
    if (r->off)
        return; /* a tab another one covers: its pixels are not ours */
    r->jump = 0; /* every row drawn again where the grid has it */
    r->jump_step = 0;
    r->view_stale = 0;
    top = w->BorderTop + r->inset_top; /* the tab bar above stays the host's */
    if (w->Width - w->BorderRight - 1 >= w->BorderLeft && w->Height - w->BorderBottom - 1 >= top) {
        /* every plane written: from here the planes in use are the
         * background's and what the rows below draw */
        vr_mask_end(r);
        /* the window's background: a default blank's (an Amiga-dialect
         * window's is the program's pen, not the screen's; OSC 11's,
         * reverse video's), the same the rows below and the strips beside
         * them are given -- not the screen's pen (W-T12) */
        r->bs_valid = 0;
        blank_look(r);
        fill(r, w->BorderLeft, top, w->Width - w->BorderRight - 1,
             w->Height - w->BorderBottom - 1, r->bg_ink);
        r->mask = (UBYTE)(r->bg_ink & VR_INK_RGB ? 0xFF : r->bg_ink);
        r->pad_ink = r->bg_ink;
        r->blank = (UBYTE)!r->view; /* blank until the rows below draw something */
    }
    r->cursor_drawn = 0;
    draw_rows(r, 0, 0, r->cols, r->rows);
}


/* Jump scroll (S1, creep's CCON 1.2.8): while scrolls keep coming frame
 * after frame -- a flood of output, or a program that waits for each line
 * to be drawn (conbench sync-line) -- the screen moves further than the
 * grid did and keeps the difference as r->jump: screen row s shows grid
 * row s + jump, the rows below the text are blank, and the next scrolls
 * move no pixels until the text reaches the bottom again. Each pass that
 * scrolls again doubles the step (vr_mask_begin), up to a screenful. When
 * the output stops, vr_settle moves the text back down: the window looks
 * exactly as without the jump. Only the whole screen of the live grid;
 * anything else settles first. */
void vr_settle(vr_render *r)
{
    int e = r->jump, rows = vis_rows(r);
    if (!e || !r->win)
        return;
    r->jump = 0;
    r->jump_step = 0;
    if (r->hidden || r->view)
        return;
    vr_cursor_off(r);
    if (e < rows)
        raw_scroll(r, 0, rows, -e); /* screen rows 0..rows-e-1 show grid rows e.. : down by e */
    draw_rows(r, 0, 0, vis_cols(r), e < rows ? e : rows); /* the rows above them */
}

void vr_scroll(vr_render *r, int top, int bottom, int n)
{
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    if (r->view)
        r->view_stale = 1; /* the grid scrolled under the scrollback view */
    if (r->hidden || r->view)
        return;
    vr_cursor_off(r);
    if (bottom > vis_rows(r))
        bottom = vis_rows(r);
    if (top >= bottom || !vis_cols(r))
        return;
    if (r->blank) {
        r->jump = 0; /* nothing on screen to keep in place */
        return; /* blank rows over blank rows: no pixel changes (CCON 1.2.4, the ROM console) */
    }
    if (top == 0 && bottom == vis_rows(r) && n > 0 && r->in_pass) {
        int h = bottom, B;
        r->scrolled_pass = 1;
        if (r->jump >= n) {
            r->jump -= n; /* the text moves into the blank rows below it: no pixels move */
            return;
        }
        B = n - r->jump + r->jump_step; /* move that far, keep jump_step rows to spare */
        if (B > h)
            B = h;
        raw_scroll(r, 0, h, B);
        r->jump = r->jump + B - n;
        if (r->jump < 0)
            r->jump = 0;
        return;
    }
    vr_settle(r); /* a region, or the other way: the screen exactly as the grid first */
    raw_scroll(r, top, bottom, n);
}

/* Rows [top, bottom) of the screen up by n (down when n < 0), the vacated
 * rows in the default background. */
static void raw_scroll_now(vr_render *r, int top, int bottom, int n);

/* A scroll in a composed pass moves the window's pixels: what is composed
 * so far reaches it first, so the blit moves the frame's rows as drawn. */
static void raw_scroll(vr_render *r, int top, int bottom, int n)
{
    struct RastPort *f = to_front(r);
    raw_scroll_now(r, top, bottom, n);
    to_back(r, f);
}

static void raw_scroll_now(vr_render *r, int top, int bottom, int n)
{
    WORD dy = (WORD)(n * r->ch);
    /* The vacated rows must come out in the window's background (the
     * engine's scroll contract): a default blank's look, as the cells and
     * strips have it -- the Amiga global background pen, for one, is not
     * always pen 0, and reverse video's is the foreground (W-T12) */
    blank_look(r);
#ifdef VTCON_DIRECT
    {
        /* Unobscured planar window: blit the screen bitmap itself and
         * fill the vacated rows, as retro32-term does; ScrollRaster's
         * layer bookkeeping cost more than the copy (rig, AGA 4 planes). */
        struct Layer *layer = r->win->WLayer;
        int done = 0;
        LockLayer(0, layer);
        if (direct_ok(r)) {
            struct BitMap *bm = r->rp->BitMap;
            WORD sx = r->win->LeftEdge + r->ox, w = (WORD)(vis_cols(r) * r->cw);
            WORD sy = r->win->TopEdge + r->oy + top * r->ch;
            WORD h = (WORD)((bottom - top) * r->ch), ad = dy < 0 ? -dy : dy;
            UBYTE pen = (UBYTE)ink_solid(r->bg_ink); /* planar: never an RGB ink */
            if (ad < h) {
                if (dy > 0)
                    BltBitMap(bm, sx, sy + ad, bm, sx, sy, w, h - ad, 0xC0, 0xFF, 0);
                else
                    BltBitMap(bm, sx, sy, bm, sx, sy + ad, w, h - ad, 0xC0, 0xFF, 0);
            }
            /* the vacated rows in the background pen: minterm 0xF0 sets
             * the planes of its bits, 0x00 clears the rest */
            {
                WORD fy = dy > 0 ? sy + h - (ad < h ? ad : h) : sy, fh = ad < h ? ad : h;
                BltBitMap(bm, sx, fy, bm, sx, fy, w, fh, 0x00, (UBYTE)~pen, 0);
                if (pen)
                    BltBitMap(bm, sx, fy, bm, sx, fy, w, fh, 0xFF, pen, 0);
            }
            done = 1;
        }
        UnlockLayer(layer);
        if (done)
            return;
    }
#endif
    SetBPen(r->rp, ink_pen(r, r->bg_ink, 1));
    ScrollRaster(r->rp, 0, dy, r->ox, r->oy + top * r->ch, r->ox + vis_cols(r) * r->cw - 1,
                 r->oy + bottom * r->ch - 1);
}

/* Draw (on) or erase (off) the cursor: the cell (a block), or its two
 * bottom rows or left columns. The profile's cursor-color, when it has one,
 * fills it and a block's glyph is redrawn in the default background; else
 * ("inverse") the cell's own colours swapped, as the selection and reverse
 * video are (style_swap): the strip of an underline or bar in the text
 * colour. COMPLEMENT flips pen NUMBERS, not colours, so it drew a pen that
 * depended on the palette (a blue block on a 16-colour Workbench; rig
 * 2026-10-03, 32 pens: black in pen 17 inverted to 14, a cyan cursor);
 * only a double-size row's block is still inverted. Off repaints the cell. */
static void cursor_draw_run(vr_render *r, int on)
{
    int wide = !r->view && vt_row_size(r->t, r->cursor_y + r->jump) ? 2 : 1; /* a double-size row (the grid row, not the screen's: jump) */
    WORD px = (WORD)(r->ox + r->cursor_x * r->cw * wide), py = r->oy + r->cursor_y * r->ch;
    int span = wide == 2 && dbl_cols(r, r->cursor_x) == 1 ? 1 : wide; /* a double-size row's half cell: its half */
    WORD x1 = (WORD)(px + r->cw * span - 1), y1 = (WORD)(py + r->ch - 1);
    int style = vt_cursor_style(r->t);   /* DECSCUSR */
    int ncells;
    const vt_cell *c;
    int have;
    ULONG fillink, textink; /* the cursor's fill and the block's glyph */
    if (style == 3 || style == 4)
        py = (WORD)(y1 - 1);              /* underline: the two bottom rows */
    else if (style == 5 || style == 6)
        x1 = (WORD)(px + 1);              /* bar: the two left columns */
    if (!on) {
        if (r->cursor_colorful) {
            /* the cell was filled: repaint it normally */
            r->cursor_colorful = 0;
#ifdef VR_CPROF
            vr_gc[3]++;
#endif
            draw_rows(r, r->cursor_x, r->cursor_y, r->cursor_x + 1, r->cursor_y + 1);
        } else {
            SetDrMd(r->rp, COMPLEMENT);
            all_planes(r);
            RectFill(r->rp, px, py, x1, y1);
            SetDrMd(r->rp, JAM2);
        }
        return;
    }
    c = vt_row(r->t, r->cursor_y + r->jump, &ncells); /* grid row = screen row + jump */
    have = c && r->cursor_x < ncells;
    fillink = r->cursor_ink != VR_KEEP ? r->cursor_ink : r->pen_default_fg;
    textink = r->pen_default_bg;
    if (r->cursor_ink == VR_KEEP && have) {
        /* inverse: the cell's colours swapped. A screen of few pens dithers
         * the fill as the cell's own area is (bgf after the swap) */
        vr_style st;
        ULONG text;
        cell_style(r, &c[r->cursor_x], 0, &st);
        text = st.fg; /* the pen its glyph shows in: not the foreground's when that is the background's pen too */
        style_swap(&st);
        fillink = st.bgf;
        textink = st.fg;
        if (ink_solid(fillink) == ink_solid(textink))
            fillink = text;
    }
    if (style > 2) {
        /* underline, bar: the strip in the cursor colour */
        fill(r, px, py, x1, y1, fillink);
        r->cursor_colorful = 1;
        return;
    }
    if (wide == 1) {
        vt_glyph g = { VT_GLYPH_FONT, 0 };
        struct TextFont *font = r->font;
        if (have) {
            g = vt_map_glyph(cell_char(r, &c[r->cursor_x]), r->enc);
            if (g.kind == VT_GLYPH_MISSING)
                g.code = 1;
        }
        if (have && g.kind != VT_GLYPH_FONT) {
            /* box / block / line on the cursor colour, drawn in the text
             * colour -- draw_special fills the cell with `bg` itself */
            r->cursor_colorful = 1;
            draw_special(r, px, py, g, textink, fillink, 1);
            return;
        }
        fill(r, px, py, x1, y1, fillink);
        ink_a(r, textink);
        SetDrMd(r->rp, JAM1);
        r->cursor_colorful = 1;
        if (have && c[r->cursor_x].width == 1) {
            int obpr = 0;
            const UBYTE *om = vt_fallback_glyph(&r->fb, cell_char(r, &c[r->cursor_x]), 1, &obpr);
            if (om) {
                /* an outline or Unifont glyph under the cursor, in the text colour */
                BltTemplate((PLANEPTR)om, 0, (WORD)obpr, r->rp, px, py, r->cw, r->ch);
                SetDrMd(r->rp, JAM2);
                return;
            }
        }
        if (have && g.code) {
            int fk = vt_cell_font(r->t, &c[r->cursor_x]);
            if (fk && fk <= 10 && r->alt_font[fk])
                font = r->alt_font[fk];
            SetFont(r->rp, font);
            Move(r->rp, px, py + r->base);
            Text(r->rp, (STRPTR)&g.code, 1);
            if (font != r->font)
                SetFont(r->rp, r->font);
        }
        SetDrMd(r->rp, JAM2);
        return;
    }
    r->cursor_colorful = 0;
    SetDrMd(r->rp, COMPLEMENT);
    all_planes(r);
    RectFill(r->rp, px, py, x1, y1);
    SetDrMd(r->rp, JAM2);
}

static void cursor_draw(vr_render *r, int on)
{
#ifdef VR_CPROF
    ULONG g0[4];
    int k;
    for (k = 0; k < 4; k++)
        g0[k] = vr_gc[k];
    cursor_draw_run(r, on);
    if (vr_cp_pass)
        for (k = 0; k < 4; k++)
            vr_cp[(on ? 4 : 0) + k] += vr_gc[k] - g0[k];
    else
        vr_cp[on ? 11 : 10]++;
#else
    cursor_draw_run(r, on);
#endif
}

void vr_cursor_off(vr_render *r)
{
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    if (r->off) {
        r->cursor_drawn = 0; /* another tab owns the pixels now */
        return;
    }
    if (r->cursor_drawn) {
        /* the cell comes back as it was: a blank screen with a cursor on
         * it is blank again -- in the background it was blank in. The
         * cell's redraw may be the first to see another one (OSC 11, then
         * the pass takes the cursor off): then the screen is not blank in
         * it, and the pass must draw every blank cell (W-T12: they kept
         * the old background) */
        UBYTE was = r->blank, known = r->bs_valid;
        ULONG ink = r->bg_ink;
        struct RastPort *f = to_front(r);
        cursor_draw(r, 0);
        /* the cell's repaint (cursor_draw) looked at the default blank before the
         * pass's output (reverse video, a flash) was read: not known for the pass */
        r->bs_valid = known;
        to_back(r, f);
        r->cursor_drawn = 0;
        r->blank = (UBYTE)(was && r->bg_ink == ink);
    }
}

static void cursor_on_now(vr_render *r);

void vr_cursor_on(vr_render *r)
{
    struct RastPort *f = to_front(r);
    cursor_on_now(r);
    to_back(r, f);
}

static void cursor_on_now(vr_render *r)
{
    int x, y;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    if (r->hidden)
        return;
    vt_cursor(r->t, &x, &y);
    y -= r->jump; /* its screen row (vr_scroll's jump) */
    if (r->cursor_drawn && (x != r->cursor_x || y != r->cursor_y))
        vr_cursor_off(r);
    if (!(vt_modes(r->t) & VT_MODE_CURSOR_VISIBLE) || x >= vis_cols(r) || y >= vis_rows(r) || y < 0 || r->view)
        return;
    if (!r->cursor_drawn) {
        UBYTE was = r->blank;
        ULONG ink = r->bg_ink;
        r->cursor_x = (WORD)x;
        r->cursor_y = (WORD)y;
#ifdef VR_CPROF
        if (vr_cp_pass) {
            /* the cell as the last pass drew it: same place, style, jump and cell bytes? */
            static struct { int ok, x, y, style, jump; vt_cell c; } last;
            int ncells;
            const vt_cell *c = vt_row(r->t, y + r->jump, &ncells);
            int have = c && x < ncells;
            int same = last.ok && last.x == x && last.y == y && last.style == vt_cursor_style(r->t) &&
                       last.jump == r->jump && have && !memcmp(&last.c, &c[x], sizeof(vt_cell));
            vr_cp[8]++;
            vr_cp[9] += same;
            last.ok = have;
            last.x = x;
            last.y = y;
            last.style = vt_cursor_style(r->t);
            last.jump = r->jump;
            if (have)
                last.c = c[x];
        }
#endif
        cursor_draw(r, 1);
        r->cursor_drawn = 1;
        /* vr_scroll takes the cursor off before it looks; blank in the
         * background it was blank in (vr_cursor_off) */
        r->blank = (UBYTE)(was && r->bg_ink == ink);
    }
}
