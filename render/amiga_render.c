/* The Amiga renderer; see amiga_render.h. */
#include "amiga_render.h"
#include "painter.h"
#include "chips.h"
#ifdef VTCON_PROF
#define TimerBase vtwin_timer
extern struct Device *vtwin_timer;
#include <proto/timer.h>
ULONG vr_prof[4]; /* PROF=1: EClock ticks in painter_run, Text() runs; painter / Text calls */
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
#include <graphics/videocontrol.h>
#include <graphics/displayinfo.h>
#include <intuition/intuitionbase.h>

extern struct GfxBase *GfxBase;

/* cybergraphics.library V41 WriteLUTPixelArray (LVO -198; AROS
 * cybergraphics.conf, the CGX/P96 SDK's fd): 8-bit indices through a
 * table of 0x00RRGGBB, on true-colour screens. No SDK header here, so
 * the call is declared as vbcc's inline. */
#define VR_CTABFMT_XRGB8 0
LONG vr_cgx_write_lut(__reg("a6") struct Library *, __reg("a0") APTR src, __reg("d0") UWORD sx,
                      __reg("d1") UWORD sy, __reg("d2") UWORD smod, __reg("a1") struct RastPort *rp,
                      __reg("a2") APTR ctab, __reg("d3") UWORD dx, __reg("d4") UWORD dy,
                      __reg("d5") UWORD w, __reg("d6") UWORD h, __reg("d7") UBYTE fmt) = "\tjsr\t-198(a6)";

#define RUN_MAX 256

/* ---- palette ------------------------------------------------------------ */


static LONG obtain(vr_render *r, ULONG rgb)
{
    static const struct TagItem tags[] = {
        { OBP_Precision, PRECISION_IMAGE },
        { TAG_DONE, 0 }
    };
    ULONG cr = (rgb >> 16) & 0xFF, cg = (rgb >> 8) & 0xFF, cb = rgb & 0xFF;
    if (!r->cm)
        return -1;
    return ObtainBestPenA(r->cm, cr * 0x01010101UL, cg * 0x01010101UL, cb * 0x01010101UL,
                          (struct TagItem *)tags);
}

/* An ink is what a cell is drawn with: a screen pen, or on a true-colour
 * screen VR_INK_RGB | 0xRRGGBB, loaded into a scratch pen just before the
 * drawing call that uses it (ink_pen). */
#define VR_INK_RGB 0x80000000UL

/* The pen that draws ink as pen A (slot 0) or pen B (slot 1). On a screen
 * of more than 8 bits a pen is only the colour of the next drawing, not a
 * palette entry: pixels drawn earlier keep their colour when it changes
 * (the method Phantasm described), so one exclusive pen per slot shows any
 * number of colours. */
static UBYTE ink_pen(vr_render *r, ULONG ink, int slot)
{
    ULONG rgb;
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

/* The ink for a resolved engine colour. */
static ULONG pen_for(vr_render *r, vt_color c, int is_bg)
{
    if (c == VT_COLOR_DEFAULT)
        return r->pen_default_fg;
    if (c == VT_COLOR_DEFAULT_BG)
        return r->pen_default_bg;
    if (vt_personality(r->t) == VT_AMIGA)
        return (UBYTE)(c & 0xFF); /* amiga colours are screen pens */
    if (c & VT_COLOR_RGB) {
        if (r->truecolor)
            return truecolor_ink(r, VT_RGB_OF(c));
        /* a palette screen (AGA, 8-bit RTG): the nearest of the xterm 256
         * colours, drawn with that index's pen. At most 240 pens, each
         * obtained once and shared with SGR 38;5 -- a pen per new colour
         * ran out after 64 and the rest fell to one colour (AGA 256: 63
         * colours on screen for 320 asked, mean error 161) */
        c = (vt_color)vt_rgb_to_256(VT_RGB_OF(c));
    }
    c &= 0xFF;
    /* the 240 extended colours on a true-colour screen: exact, as a direct
     * colour -- a pen each from the screen's colour map ran out after
     * about 150, and "best pen" then gave Workbench colours (a 256-colour
     * cube on the rig: 52 of 240 cells wrong, (238,153,0) for (215,135,0)) */
    if (r->truecolor && c >= 16)
        return truecolor_ink(r, vt_palette_rgb(r->t, c));
    if (!r->have[c]) {
        LONG p = obtain(r, vt_palette_rgb(r->t, c));
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

static void extract_glyphs(vr_render *r);
static void spr_free(vr_render *r);
static int spr_screen_ns(struct Screen *scr, int *lace);

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
    r->cursor_flip = 0;
    r->full_pass = 0;
    r->in_pass = r->bs_valid = r->bs_ok = 0;
    r->seen = 0;
    r->bg_ink = r->pad_ink = r->pen_default_bg;
    r->planar = (UBYTE)((GetBitMapAttr(win->RPort->BitMap, BMA_FLAGS) & BMF_STANDARD) &&
                        GetBitMapAttr(win->RPort->BitMap, BMA_DEPTH) <= 8);
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
    r->scratch[0] = r->scratch[1] = -1;
    r->scratch_ink[0] = r->scratch_ink[1] = 0;
    r->n_exact = 0;
    for (i = 0; i < VR_EXACT_SLOTS; i++)
        r->exact_key[i] = 0;
    r->dflt_obtained[0] = r->dflt_obtained[1] = -1;
    r->has_blink = 0;
    r->blink_frames = 0;
    r->blink_slow_off = r->blink_fast_off = 0;
    for (i = 0; i <= 10; i++)
        r->alt_font[i] = 0;
    if (r->cm && win->WScreen &&
        GetBitMapAttr(win->WScreen->RastPort.BitMap, BMA_DEPTH) > 8) {
        r->scratch[0] = ObtainPen(r->cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR);
        r->scratch[1] = ObtainPen(r->cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR);
        r->truecolor = r->scratch[0] >= 0 && r->scratch[1] >= 0;
    }
    r->cursor_drawn = 0;
    r->cursor_colorful = 0;
    r->cursor_ink = VR_KEEP;
    r->sel_ink[0] = r->sel_ink[1] = VR_KEEP;
    r->cursor_pen = r->sel_pen[0] = r->sel_pen[1] = -1;
    r->bell_flash = 0;
    r->cursor_x = r->cursor_y = 0;
    r->view = 0;
    r->sel = 0;
    r->lay_rows = r->lay_cols = r->lay_x = r->lay_y = -1;
    SetFont(r->rp, font);
    r->glyphs = 0;
    if (r->planar)
        extract_glyphs(r); /* the planar path's glyphs (DIRECT=1: also the text's 8-pixel alignment) */
    r->n_direct = r->n_text = 0;
    r->bp = VC_BLIT_ANY;
    r->owe0 = r->owe1 = 0;
    r->n_blit_scroll = r->n_owed_fill = 0;
    r->spr_num = -1;
    r->spr_es[0] = r->spr_es[1] = 0;
    r->spr_key[0] = r->spr_key[1] = 0;
    r->spr_cur = r->spr_vis = r->spr_none = 0;
    r->spr_x = r->spr_y = 0;
    r->spr_env = VC_CUR_NONE;
    {
        int lace = 0;
        r->spr_sns = (WORD)(win->WScreen ? spr_screen_ns(win->WScreen, &lace) : 0); /* a screen's mode stays */
        r->spr_lace = (BYTE)lace;
        r->spr_ns = 0;
    }
    r->n_spr_moves = r->n_spr_images = r->n_plane_cursor = 0;
    for (i = 0; i < VR_IMG_SLOTS; i++)
        r->img_key[i] = 0;
    r->n_img_pens = 0;
    r->img_buf = 0;
    r->img_buf_size = 0;
    r->img_serial = 0;
    r->n_img_runs = 0;
    /* true colour: images straight from their indices (P96 and CGX both
     * offer cybergraphics.library); else the pens path below */
    r->cgx = r->truecolor ? OpenLibrary((STRPTR)"cybergraphics.library", 41) : 0;
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
        r->have[i] = 0;
        r->obtained[i] = -1;
    }
}

void vr_free(vr_render *r)
{
    int i;
    spr_free(r); /* the window is still open: its screen gets the sprite back */
    if (r->cm)
        for (i = 0; i < 256; i++)
            if (r->obtained[i] >= 0)
                ReleasePen(r->cm, (ULONG)r->obtained[i]);
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
    for (i = 0; i < VR_IMG_SLOTS; i++)
        if (r->cm && r->img_key[i])
            ReleasePen(r->cm, (ULONG)r->img_pen[i]);
    if (r->img_buf)
        FreeVec(r->img_buf);
    if (r->cgx)
        CloseLibrary(r->cgx);
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
}

void vr_set_off(vr_render *r, int off)
{
    if (off && !r->off && r->win) {
        vr_cursor_hide(r); /* still ours to take away, a sprite too */
        spr_free(r);       /* the tab shown now may need it (sprites are few) */
    }
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

/* A graphics call that may leave a blit running, rectangle unknown (CC1):
 * every drawing call here other than the painter's goes through one. */
#define GFX(r) ((r)->bp = VC_BLIT_ANY)
static void pay_owed(vr_render *r);

/* The CPU is about to write pixel rows [y0, y1) of the window into the
 * bitplanes: a WaitBlit first, unless the only blit that may still run is
 * the scroll copy and these rows are outside it (CC1: the painter fills
 * and draws beside the copy). */
static void cpu_sync(vr_render *r, WORD y0, WORD y1)
{
    if (vc_cpu_may_write(r->bp, r->bp_y0, r->bp_y1, y0, y1))
        return;
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
    WaitBlit();
#endif
    r->bp = VC_BLIT_IDLE;
}

static void fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG pen)
{
    if (x1 < x0 || y1 < y0)
        return;
    if (pen != r->bg_ink)
        r->blank = 0;
    ink_a(r, pen);
    SetDrMd(r->rp, JAM1);
    GFX(r);
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
    r->bp = VC_BLIT_ANY; /* blits since the last pass (gadgets, the title): not known */
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
    pay_owed(r);
    if (r->bp != VC_BLIT_IDLE && r->in_pass) {
        /* the one WaitBlit of the pass (CC1): every pixel is in the planes
         * before the pass is over -- a WaitForChar answers after this */
        WaitBlit();
    }
    r->bp = VC_BLIT_ANY; /* until the next pass: others draw too (gadgets, the title) */
    r->in_pass = 0;
    if (!r->mask_on)
        return;
    r->mask_on = 0;
    SetWriteMask(r->rp, 0xFF);
}

static void line(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1)
{
    GFX(r);
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

static void draw_block(vr_render *r, WORD px, WORD py, vt_u8 code, ULONG fg, ULONG bg)
{
    WORD w = r->cw, h = r->ch;
    if (code & 0x80) {
        ink_ab(r, fg, bg);
        SetAfPt(r->rp, (UWORD *)shade_pat[(code & 3) - 1], 1);
        GFX(r);
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

static void draw_special(vr_render *r, WORD px, WORD py, vt_glyph g, ULONG fg, ULONG bg)
{
    WORD x1 = px + r->cw * (g.kind == VT_GLYPH_MISSING && g.code == 2 ? 2 : 1) - 1, y1 = py + r->ch - 1;
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
        draw_block(r, px, py, g.code, fg, bg);
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
    return a->fg == b->fg && a->bg == b->bg && a->ul == b->ul && a->attr == b->attr &&
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
    GFX(r);
    Move(r->rp, x0, y);
    Draw(r->rp, x1, y);
}

/* A broken line: on pixels, off pixels (dotted 1/1, dashed 3/2). Drawn as
 * short fills: the line pattern (SetDrPt) drew nothing on the rig's RTG
 * screen. */
static void broken_hline(vr_render *r, WORD x0, WORD x1, WORD y, WORD on, WORD off)
{
    WORD x;
    GFX(r);
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
    GFX(r);
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
    GFX(r);
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
            if (!(r->was_blank && st->bg == r->bg_ink))
                fill(r, px, py, px + n * r->cw - 1, py + r->ch - 1, st->bg);
            return;
        }
    }
    if (painter_run(r, run, n, px, py, st))
        return;
    r->n_text++;
    r->blank = 0;
    if (st->font && st->font <= 10 && r->alt_font[st->font])
        font = r->alt_font[st->font];
    if (!(st->attr & (VT_ATTR_SUPER | VT_ATTR_SUB)) || !draw_script(r, run, n, px, py, st, font)) {
        if (font != r->font)
            SetFont(r->rp, font);
        ink_ab(r, st->fg, st->bg);
        GFX(r);
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

/* Is the window's bitmap planes in chip RAM -- a native display, not a
 * graphics card's? Once a bitmap (it is asked for every run the painter
 * draws: S1, sgr-colour paid ~1 ms a run): RTG is not planar, and
 * Picasso96 calls its bitmaps standard too (it hung the rig, 2026-09-29) --
 * a native display bitmap is planes in chip RAM, graphics card memory
 * never is. */
static int chip_planar(vr_render *r)
{
    struct BitMap *bm = r->rp->BitMap;
    if (!bm || bm->Depth > 8)
        return 0;
    if (bm != r->chip_bm || bm->Planes[0] != r->chip_plane0) {
        int p, ok = (GetBitMapAttr(bm, BMA_FLAGS) & BMF_STANDARD) != 0;
        for (p = 0; ok && p < bm->Depth; p++)
            if (!bm->Planes[p] || !(TypeOfMem(bm->Planes[p]) & MEMF_CHIP))
                ok = 0;
        r->chip_bm = bm;
        r->chip_plane0 = bm->Planes[0];
        r->chip_ok = (UBYTE)ok;
    }
    return r->chip_ok;
}

/* Does one unobscured clip rectangle hold the whole text area (nothing
 * covers it, in part or whole)? The caller holds the layer lock. */
static int layer_whole(vr_render *r)
{
    struct Window *w = r->win;
    struct ClipRect *cr = w->WLayer ? w->WLayer->ClipRect : 0;
    WORD sx0 = w->LeftEdge + r->ox, sy0 = w->TopEdge + r->oy;
    WORD sx1 = sx0 + vis_cols(r) * r->cw - 1, sy1 = sy0 + vis_rows(r) * r->ch - 1;
    if (!cr || cr->Next || cr->obscured)
        return 0; /* covered in part: the layer draws for us */
    return cr->bounds.MinX <= sx0 && cr->bounds.MinY <= sy0 && cr->bounds.MaxX >= sx1 &&
           cr->bounds.MaxY >= sy1;
}

/* May cells be written straight into the screen's bitplanes now, at
 * any bit phase (the painter, render/painter.h)? The caller holds the
 * window's layer lock. */
static int planes_ok(vr_render *r, int aligned)
{
    if (!r->glyphs || (aligned && ((r->win->LeftEdge + r->ox) & 7)))
        return 0;
    return chip_planar(r) && layer_whole(r);
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
    struct BitMap *bm;
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
    LockLayer(0, layer);
    ok = planes_ok(r, 0);
    if (ok) {
        if (pens & ~r->mask) {
            /* a pen's plane not in use yet holds zeros: from here on it is
             * drawn too (as ink_pen does for the RastPort) */
            r->mask |= pens;
            if (r->mask_on)
                SetWriteMask(r->rp, r->mask);
        }
        r->seen |= pens;
        bm = r->rp->BitMap;
        cpu_sync(r, py, (WORD)(py + r->ch)); /* the blits before it are in the planes first */
        vp_span_fast((vp_u8 **)bm->Planes, bm->Depth, bm->BytesPerRow, (long)(r->win->LeftEdge + px),
                     (long)(r->win->TopEdge + py), r->glyphs, r->font->tf_YSize, run, n, (int)st->fg,
                     (int)st->bg, r->mask);
    }
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

static void direct_cell(vr_render *r, int x, int y, UBYTE ch, UBYTE fg, UBYTE bg, vt_u8 attr)
{
    struct BitMap *bm = r->rp->BitMap;
    WORD bpr = bm->BytesPerRow;
    LONG off = (LONG)(r->win->TopEdge + r->oy + y * r->ch) * bpr + ((r->win->LeftEdge + r->ox) >> 3) + x;
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
    vr_asm_cell((UBYTE **)bm->Planes, bm->Depth, off, bpr, rows, h, fg, bg, r->mask);
#else
    for (p = 0; p < bm->Depth; p++) {
        UBYTE *d = (UBYTE *)bm->Planes[p] + off;
        int f = (fg >> p) & 1, b = (bg >> p) & 1;
        if (!((r->mask >> p) & 1))
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
    int i, ok;
    UBYTE pens = 0;
    for (i = 0; i < n; i++)
        pens |= (UBYTE)(d[i].fg | d[i].bg);
    LockLayer(0, layer);
    ok = direct_ok(r);
    if (ok) {
        if (pens & ~r->mask) {
            /* a pen's plane not in use yet: it holds zeros, the cells
             * below start writing it (as ink_pen does for the RastPort) */
            r->mask |= pens;
            if (r->mask_on)
                SetWriteMask(r->rp, r->mask);
        }
        r->seen |= pens;
        cpu_sync(r, (WORD)(r->oy + y * r->ch), (WORD)(r->oy + (y + 1) * r->ch)); /* earlier blits first */
        for (i = 0; i < n; i++)
            direct_cell(r, d[i].x, y, d[i].ch, d[i].fg, d[i].bg, d[i].attr);
    }
    UnlockLayer(layer);
    r->n_direct += ok ? n : 0;
    if (ok)
        r->blank = 0;
    return ok;
}

/* The style cell c draws with (selection and blink phase applied). */
static void cell_style(vr_render *r, const vt_cell *c, int selected_cell, vr_style *st)
{
    vt_color f, b, u;
    vt_resolve_colors(r->t, c, &f, &b);
    st->fg = pen_for(r, f, 0);
    st->bg = pen_for(r, b, 1);
    if (st->fg == st->bg && f != b)
        /* a screen with few pens (a 4-colour Workbench) gave the text the
         * background's pen: the text went invisible (rig 3.2, the line
         * editor's green command word). The plain text pen then, or the
         * background's opposite. Conceal and the blink's off phase hide
         * text on purpose below. */
        st->fg = st->bg == r->pen_default_fg ? r->pen_default_bg : r->pen_default_fg;
    if (selected_cell) {
        /* the profile's selection colours, else the swapped cell */
        if (r->sel_ink[0] != VR_KEEP)
            st->fg = r->sel_ink[0];
        if (r->sel_ink[1] != VR_KEEP)
            st->bg = r->sel_ink[1];
        if (r->sel_ink[0] == VR_KEEP && r->sel_ink[1] == VR_KEEP) {
            ULONG tmp = st->fg;
            st->fg = st->bg;
            st->bg = tmp;
        }
    }
    if (r->bell_flash) {
        /* the visual bell's frame: the whole window reversed, the terminal's
         * own reverse-video mode untouched */
        ULONG tmp = st->fg;
        st->fg = st->bg;
        st->bg = tmp;
    }
    u = vt_cell_underline_color(r->t, c);
    st->ul = u == VT_COLOR_DEFAULT ? st->fg : pen_for(r, u, 0);
    st->attr = (vt_attr)(c->attr & DRAWN_ATTRS);
    st->deco = c->deco;
    st->font = (vt_u8)vt_cell_font(r->t, c);
    if ((c->attr & VT_ATTR_BLINK) && vt_personality(r->t) != VT_PCANSI) {
        /* pcansi's blink is the iCE bright background, never a blink */
        r->has_blink = 1;
        if (c->attr & VT_ATTR_RAPID ? r->blink_fast_off : r->blink_slow_off)
            st->fg = st->ul = st->bg; /* the off phase: text and lines hidden */
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
    fill(r, r->ox, py, (WORD)(r->ox + vis_cols(r) * cw - 1), (WORD)(py + ch - 1), r->pen_default_bg);
    GFX(r);
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
        WORD px = (WORD)(r->ox + x * 2 * cw);
        vt_glyph g;
        struct TextFont *font = r->font;
        UBYTE code;
        cell_style(r, &c[x], selected(r, x, y - r->view + r->jump), &st);
        fill(r, px, py, (WORD)(px + 2 * cw - 1), (WORD)(py + ch - 1), st.bg);
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
                        0, (WORD)big->BytesPerRow, r->rp, px, py, (WORD)(2 * cw), ch);
        }
        if ((st.attr & LINE_ATTRS) || (st.deco & VT_DECO_IDEO_MASK))
            decorate(r, 2, px, py, &st);
    }
    WaitBlit();
    FreeBitMap(big);
    FreeBitMap(full);
    if (r->cursor_drawn && r->cursor_y == y)
        r->cursor_drawn = 0; /* the cursor cell was just painted over */
}

/* The outline font's glyph for a cell the bitmap font cannot show itself
 * (render/outline; 0: draw as without one). */
static const UBYTE *outline_glyph(vr_render *r, vt_u32 cp, int cells, WORD *bpr)
{
    if (r->outline && cp >= 0x80 && !vt_glyph_native(cp, r->enc))
        return vo_glyph(r->outline, cp, cells, bpr);
    return 0;
}

/* The marks left over a drawn cell (vt_compose_cell could not fold them
 * into its character): the outline font's mark glyphs, in the text colour
 * on top. Without an outline font they are not drawn (copy keeps them). */
static void draw_marks(vr_render *r, WORD px, WORD py, const vt_u32 *mk, int n, int cells, const vr_style *st)
{
    int i;
    for (i = 0; i < n && r->outline; i++) {
        WORD bpr;
        const UBYTE *m = vo_mark(r->outline, mk[i], cells, &bpr);
        if (!m)
            continue;
        ink_a(r, st->fg);
        SetDrMd(r->rp, JAM1);
        GFX(r);
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
    fill(r, px, py, (WORD)(px + w - 1), (WORD)(py + r->ch - 1), st->bg);
    ink_a(r, st->fg);
    SetDrMd(r->rp, JAM1);
    GFX(r);
    BltTemplate((PLANEPTR)m, 0, bpr, r->rp, px, py, w, r->ch);
    if (st->attr & VT_ATTR_BOLD)
        BltTemplate((PLANEPTR)m, 0, bpr, r->rp, (WORD)(px + 1), py, (WORD)(w - 1), r->ch);
    SetDrMd(r->rp, JAM2);
    if ((st->attr & LINE_ATTRS) || (st->deco & VT_DECO_IDEO_MASK))
        decorate(r, cells, px, py, st);
    r->n_outline++;
    r->blank = 0;
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
        if (r->img_key[h] == key)
            return r->img_pen[h];
        if (!r->img_key[h])
            break;
    }
    if (i < VR_IMG_SLOTS && r->n_img_pens < VR_IMG_MAX && (p = obtain(r, rgb)) >= 0) {
        r->img_key[h] = key;
        r->img_pen[h] = (UBYTE)p;
        r->n_img_pens++;
        return (UBYTE)p;
    }
    ink = pen_for(r, VT_COLOR_RGB | rgb, 0);
    return (UBYTE)(ink & VR_INK_RGB ? r->pen_default_fg : ink);
}

/* The image's colours for this screen, once per image (and background). */
static void img_colours(vr_render *r, const vt_image_view *v)
{
    ULONG bgrgb = r->bg_ink & VR_INK_RGB ? r->bg_ink & 0xFFFFFFUL : vr_pen_rgb(r, (UBYTE)r->bg_ink);
    int i;
    if (r->img_serial == v->serial && r->img_bgrgb == bgrgb)
        return;
    r->img_serial = v->serial;
    r->img_bgrgb = bgrgb;
    if (r->cgx) {
        r->img_ctab[0] = bgrgb; /* the pixels the image left unset */
        for (i = 1; i < v->npal; i++)
            r->img_ctab[i] = v->pal[i];
        return;
    }
    r->img_map[0] = r->bg_ink & VR_INK_RGB ? img_pen_for(r, bgrgb) : (UBYTE)r->bg_ink;
    r->img_planes = r->img_map[0];
    for (i = 1; i < v->npal; i++) {
        r->img_map[i] = img_pen_for(r, v->pal[i]);
        r->img_planes |= r->img_map[i];
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
    img_colours(r, v);
    r->blank = 0;
    r->n_img_runs++;
    GFX(r);
    if (r->cgx) {
        vr_cgx_write_lut(r->cgx, (APTR)v->pix, (UWORD)sx, (UWORD)sy, (UWORD)v->w, r->rp, r->img_ctab, (UWORD)dx,
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
            d[j] = r->img_map[src[j]];
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
                img_run(r, &v, a, x, y);
            }
        }
    }
}

static void draw_rows(vr_render *r, int x0, int y0, int x1, int y1)
{
    UBYTE run[RUN_MAX];
    dcell dc[DCELL_MAX];
    int y, x, xe, n, nd, want_direct, tail_ok, paid0 = 0, paid1 = 0;
    ULONG tail_bg;
    UBYTE wb;
    if (r->hidden)
        return;
    want_direct = r->glyphs != 0;
    if (x1 > vis_cols(r))
        x1 = vis_cols(r);
    if (y1 > vis_rows(r))
        y1 = vis_rows(r);
    if (r->in_pass && r->bs_valid) {
        tail_ok = r->bs_ok && !r->sel;
        tail_bg = r->bg_ink;
    } else {
        /* what a default blank looks like now (the dialect's default
         * background, reverse video, a bell's flash): plain means its row
         * tails can be filled at once; its ink is what "blank" means.
         * Once a render pass: it cost every damaged row a cell_style()
         * (conbench cursor-pos: seven rows a frame; S1). */
        vt_cell b;
        vr_style bs;
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
        tail_ok = r->bs_ok && !r->sel;
        tail_bg = bs.bg;
        if (tail_bg != r->bg_ink) {
            r->bg_ink = tail_bg; /* another background: nothing on screen is known blank */
            r->blank = 0;
        }
    }
    if (r->owe1 > r->owe0 && y0 < r->owe1 && y1 > r->owe0) {
        /* rows a blitter scroll vacated (CC1): filled before anything is
         * drawn on them -- and then their default blanks need nothing more */
        if ((ULONG)r->owe_pen == tail_bg) {
            paid0 = r->owe0;
            paid1 = r->owe1;
        }
        pay_owed(r);
    }
    r->was_blank = (UBYTE)(r->blank && !r->cursor_drawn); /* a drawn cursor is pixels too */
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
            if (r->pad_ink != tail_bg) {
                struct Window *w = r->win;
                WORD gx1 = (WORD)(r->ox + vis_cols(r) * r->cw), gy1 = (WORD)(r->oy + vis_rows(r) * r->ch);
                WORD ax1 = (WORD)(w->Width - w->BorderRight - 1), ay1 = (WORD)(w->Height - w->BorderBottom - 1);
                UBYTE keep = r->was_blank;
                fill(r, gx1, r->oy, ax1, ay1, tail_bg);
                fill(r, r->ox, gy1, (WORD)(gx1 - 1), ay1, tail_bg);
                if (w->BorderLeft < r->ox)
                    fill(r, w->BorderLeft, r->oy, (WORD)(r->ox - 1), ay1, tail_bg);
                r->pad_ink = tail_bg;
                r->was_blank = keep;
                r->blank = 1;
            }
        }
    }
    wb = r->was_blank;
    for (y = y0; y < y1; y++) {
        int ncells;
        const vt_cell *c = vt_row(r->t, y - r->view + r->jump, &ncells);
        int gy = y - r->view + r->jump;
        WORD py = r->oy + y * r->ch, run_x = 0;
        vr_style st, run_st;
        /* the last cell's look: runs of equal cells skip the lookups */
        const vt_cell *last = 0;
        r->was_blank = (UBYTE)(wb || (y >= paid0 && y < paid1)); /* a row just filled is blank */
        if (!c)
            continue;
        if (!r->view && vt_row_size(r->t, gy)) {
            draw_double_row(r, y, c, ncells, vt_row_size(r->t, gy));
            continue;
        }
        n = 0;
        nd = 0;
        x = x0;
        if (x > 0 && x < ncells && c[x].width == 0 && r->outline)
            x--; /* the right half of a wide glyph: the outline glyph spans both, draw it whole */
        xe = x1 < ncells ? x1 : ncells;
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
                if (fs.attr || fs.deco || fs.font || fs.ul != fs.fg ||
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
            if (c[x].width == 0) {
                /* the right half of a wide glyph: its '?' took the left */
                vr_style ws;
                cell_style(r, &c[x], selected(r, x, gy), &ws);
                flush_run(r, run, n, run_x, py, &run_st);
                n = 0;
                fill(r, r->ox + x * r->cw, py, r->ox + (x + 1) * r->cw - 1, py + r->ch - 1, ws.bg);
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
                WORD obpr;
                const UBYTE *om;
                vt_u32 cp[VT_CLUSTER_CPS];
                int ncp = 1, cells = c[x].width == 2 ? 2 : 1;
                cp[0] = c[x].ch;
                if (VT_CELL_IS_CLUSTER(&c[x])) /* beyond the BMP, or with marks */
                    ncp = vt_compose_cell(cp, vt_cell_text(r->t, &c[x], cp));
                om = outline_glyph(r, cp[0], cells, &obpr);
                if (om) {
                    WORD px = (WORD)(r->ox + x * r->cw);
                    flush_run(r, run, n, run_x, py, &run_st);
                    n = 0;
                    draw_outline(r, px, py, om, obpr, cells, &st);
                    draw_marks(r, px, py, cp + 1, ncp - 1, cells, &st);
                    if (cells == 2 && x + 1 < ncells && c[x + 1].width == 0)
                        x++; /* its right half is drawn */
                    continue;
                }
                g = vt_map_glyph(cp[0], r->enc);
                if (g.kind == VT_GLYPH_MISSING)
                    g.code = (vt_u8)cells;
                if (ncp > 1 && r->outline) {
                    /* marks to draw over it: the cell now, alone */
                    WORD px = (WORD)(r->ox + x * r->cw);
                    flush_run(r, run, n, run_x, py, &run_st);
                    n = 0;
                    if (g.kind == VT_GLYPH_FONT) {
                        run[0] = g.code;
                        flush_run(r, run, 1, px, py, &st);
                    } else {
                        draw_special(r, px, py, g, st.fg, st.bg);
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
                draw_special(r, r->ox + x * r->cw, py, g, st.fg, st.bg);
                if (g.kind == VT_GLYPH_MISSING && g.code == 2 && x + 1 < ncells && c[x + 1].width == 0)
                    x++; /* the box spans its right half */
                continue;
            }
            if (want_direct && plain_style(&st) && nd < DCELL_MAX) {
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
                ds.fg = ds.ul = dc[i].fg;
                ds.bg = dc[i].bg;
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
    r->was_blank = 0;
    if (r->full_pass) {
        r->full_pass = 0;
        r->mask = r->seen;
        SetWriteMask(r->rp, r->mask_on ? r->mask : 0xFF);
    }
    SetSoftStyle(r->rp, 0, FSF_BOLD | FSF_UNDERLINED | FSF_ITALIC);
}

/* Blinking (SGR 5 slow, 6 rapid): called every frame (50 ms). Slow cells
 * change every 10 frames (1 Hz), rapid ones every 3. Returns 1 while any
 * blinking cell is on screen, so the caller keeps the frames coming. */
/* A blinking cursor: ?12, or a blinking DECSCUSR shape (1, 3, 5). */
int vr_cursor_blinks(vr_render *r)
{
    int style = vt_cursor_style(r->t);
    return (vt_modes(r->t) & VT_MODE_CURSOR_BLINK) || style == 1 || style == 3 || style == 5;
}

int vr_blink_tick(vr_render *r)
{
    int slow, fast, y, x, n, cursor;
    if (!r->win)
        return 0; /* no window (before vr_init, after vr_free) */
    cursor = vr_cursor_blinks(r);
    if ((!r->has_blink && !cursor) || r->hidden || r->view)
        return 0;
    r->blink_frames++;
    slow = r->blink_frames % 10 == 0;
    fast = r->blink_frames % 3 == 0;
    if (cursor && slow) {
        /* the cursor's own blink, at the slow rate */
        if (r->cursor_drawn || r->spr_vis)
            vr_cursor_hide(r);
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
    cursor = r->cursor_drawn || r->spr_vis; /* the cursor's phase, kept across the redraw */
    vr_cursor_off(r); /* a sprite cursor stays where it is */
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
    if (r->view)
        return; /* looking at the scrollback: the live rows are not shown */
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
    r->view = (WORD)lines;
    vr_cursor_hide(r); /* back in the scrollback: no cursor (vr_cursor_on brings it back) */
    vr_redraw(r);
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
    LONG base, lo = 0, hi = -1;
    int y0, y1, any = 0;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    base = vt_lines_scrolled(r->t);
    /* redraw the rows the old and the new selection touch */
    if (r->sel) {
        lo = (r->sel_ay < r->sel_by ? r->sel_ay : r->sel_by) - base;
        hi = (r->sel_ay < r->sel_by ? r->sel_by : r->sel_ay) - base;
        any = 1;
    }
    r->sel = (BYTE)on;
    r->sel_ax = (WORD)ax;
    r->sel_bx = (WORD)bx;
    r->sel_ay = ay + base;
    r->sel_by = by + base;
    if (on) {
        int nlo = ay < by ? ay : by, nhi = ay < by ? by : ay;
        if (!any || nlo < lo)
            lo = nlo;
        if (!any || nhi > hi)
            hi = nhi;
        any = 1;
    }
    if (!any)
        return;
    y0 = (int)lo + r->view;
    y1 = (int)hi + r->view + 1;
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
    top = w->BorderTop + r->inset_top; /* the tab bar above stays the host's */
    if (w->Width - w->BorderRight - 1 >= w->BorderLeft && w->Height - w->BorderBottom - 1 >= top) {
        /* every plane written: from here the planes in use are the
         * background's and what the rows below draw */
        r->owe0 = r->owe1 = 0; /* the fill below covers the rows a scroll left owed */
        vr_mask_end(r);
        fill(r, w->BorderLeft, top, w->Width - w->BorderRight - 1,
             w->Height - w->BorderBottom - 1, r->pen_default_bg);
        r->mask = r->pen_default_bg;
        r->pad_ink = r->pen_default_bg;
        /* blank until the rows below draw something -- when this is the
         * blank cells' own background (an Amiga-dialect window's is the
         * program's pen, not the screen's) */
        r->blank = (UBYTE)(!r->view && r->bg_ink == r->pen_default_bg);
    }
    r->cursor_drawn = 0;
    draw_rows(r, 0, 0, r->cols, r->rows);
}

static void raw_scroll(vr_render *r, int top, int bottom, int n);

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

/* The rows a blitter scroll vacated (CC1), in the pen the scroll left them
 * owed in: straight into the planes by the CPU -- after the copy only where
 * they lie in its rectangle -- or through the RastPort when the window got
 * covered since. Before anything draws on those rows (draw_rows, the
 * cursor), before another scroll moves them, and at the end of the pass. */
static void pay_owed(vr_render *r)
{
    struct Layer *layer;
    WORD y0 = (WORD)(r->oy + r->owe0 * r->ch), y1 = (WORD)(r->oy + r->owe1 * r->ch);
    UBYTE pen = r->owe_pen;
    int ok = 0;
    if (r->owe1 <= r->owe0)
        return;
    r->owe0 = r->owe1 = 0;
    if (!r->win || r->hidden)
        return; /* nothing shows: the next redraw paints it all */
    layer = r->win->WLayer;
    LockLayer(0, layer);
    if (planes_ok(r, 0)) {
        struct BitMap *bm = r->rp->BitMap;
        ink_pen(r, pen, 0); /* its planes in use from here (the mask widens) */
        cpu_sync(r, y0, y1);
        vp_fill_fast((vp_u8 **)bm->Planes, bm->Depth, bm->BytesPerRow, (long)(r->win->LeftEdge + r->ox),
                     (long)(r->win->TopEdge + y0), y1 - y0, vis_cols(r), pen, r->mask);
        ok = 1;
    }
    UnlockLayer(layer);
    if (!ok)
        fill(r, r->ox, y0, (WORD)(r->ox + vis_cols(r) * r->cw - 1), (WORD)(y1 - 1), pen);
    r->n_owed_fill++;
    if (r->cursor_drawn && r->cursor_y * r->ch + r->oy >= y0 && r->cursor_y * r->ch + r->oy < y1)
        r->cursor_drawn = 0; /* the plane cursor was in those rows: gone with them */
}

/* Rows [top, bottom) of the screen up by n (down when n < 0), the vacated
 * rows in the default background.
 *
 * Inside a render pass on an unobscured planar window (CC1) it is one
 * BltBitMap copy of the planes in use: no ScrollRaster (its layer work and
 * its clearing blit, which waits for the copy), and the vacated rows owed
 * to the CPU painter (pay_owed), which writes beside the copy where it
 * can. Elsewhere -- covered, RTG, outside a pass -- ScrollRaster. */
static void raw_scroll(vr_render *r, int top, int bottom, int n)
{
    WORD dy = (WORD)(n * r->ch);
    vt_cell blank;
    vt_color f, b;
    ULONG pen;
    /* The vacated rows must come out in the personality's default
     * background (the engine's scroll contract): the Amiga global
     * background pen, for one, is not always pen 0. */
    blank.ch = ' ';
    blank.fg = VT_COLOR_DEFAULT;
    blank.bg = VT_COLOR_DEFAULT;
    blank.attr = 0;
    blank.width = 1;
    vt_resolve_colors(r->t, &blank, &f, &b);
    pen = pen_for(r, b, 1);
    if (r->in_pass && r->mask_on) {
        struct Layer *layer = r->win->WLayer;
        int done = 0;
        pay_owed(r); /* rows still owed would be moved as they are */
        LockLayer(0, layer);
        if (vc_scroll_by_blit(1, planes_ok(r, 0), (pen & VR_INK_RGB) != 0)) {
            struct BitMap *bm = r->rp->BitMap;
            WORD sx = (WORD)(r->win->LeftEdge + r->ox), sy = (WORD)(r->win->TopEdge + r->oy);
            vc_scroll s;
            vc_scroll_plan(top, bottom, n, &s);
            if (s.copy) {
                BltBitMap(bm, sx, (WORD)(sy + s.src * r->ch), bm, sx, (WORD)(sy + s.dst * r->ch),
                          (WORD)(vis_cols(r) * r->cw), (WORD)(s.copy * r->ch), 0xC0, r->mask, 0);
                /* graphics waited for every earlier blit: this copy is the only one now */
                r->bp = VC_BLIT_SCROLL;
                r->bp_y0 = (WORD)(r->oy + s.busy0 * r->ch);
                r->bp_y1 = (WORD)(r->oy + s.busy1 * r->ch);
            }
            r->owe0 = (WORD)s.vac0;
            r->owe1 = (WORD)s.vac1;
            r->owe_pen = (UBYTE)pen; /* planar: never an RGB ink */
            r->n_blit_scroll++;
            done = 1;
        }
        UnlockLayer(layer);
        if (done)
            return;
    }
    SetBPen(r->rp, ink_pen(r, pen, 1));
    GFX(r);
    ScrollRaster(r->rp, 0, dy, r->ox, r->oy + top * r->ch, r->ox + vis_cols(r) * r->cw - 1,
                 r->oy + bottom * r->ch - 1);
}

/* ---- CC2: the cursor as a hardware sprite ---------------------------------- */

/* What a released sprite shows, one per sprite engine width (16, 32, 64):
 * the display keeps fetching whatever a sprite was last given, so that data
 * must outlive it. Made on the first release, never freed. */
static struct ExtSprite *spr_blank[3];
static const BYTE spr_order[6] = { 2, 4, 6, 3, 5, 7 }; /* never 0 and 1: the pointer's colours */

/* Width in nanoseconds of the screen's pixels (0: not a mode the sprite can
 * follow -- RTG, scan doubled, no sprites), and laced or not. */
static int spr_screen_ns(struct Screen *scr, int *lace)
{
    struct DisplayInfo di;
    ULONG mode = GetVPModeID(&scr->ViewPort);
    if (mode == (ULONG)INVALID_ID || !GetDisplayInfoData(0, (UBYTE *)&di, sizeof(di), DTAG_DISP, mode))
        return 0;
    if ((di.PropertyFlags & (DIPF_IS_FOREIGN | DIPF_IS_SCANDBL)) || !(di.PropertyFlags & DIPF_IS_SPRITES))
        return 0;
    *lace = (di.PropertyFlags & DIPF_IS_LACE) != 0;
    return di.PixelSpeed;
}

/* Width in nanoseconds of a sprite pixel on this screen now (AGA: what
 * Intuition set for its pointer; before AGA: lores). */
static int spr_sprite_ns(struct ColorMap *cm)
{
    struct TagItem t[2];
    LONG res = SPRITERESN_ECS;
    t[0].ti_Tag = VTAG_SPRITERESN_GET;
    t[0].ti_Data = (ULONG)SPRITERESN_ECS;
    t[1].ti_Tag = TAG_DONE;
    if (cm && !VideoControl(cm, t))
        res = (LONG)t[0].ti_Data;
    if (res == SPRITERESN_DEFAULT && cm && cm->Type >= COLORMAP_TYPE_V39)
        res = (LONG)cm->SpriteResDefault;
    return res == SPRITERESN_35NS ? 35 : res == SPRITERESN_70NS ? 70 : 140;
}

/* The colour register of sprite num's colour 1 on this screen, -1 when it
 * would recolour text (render/chips.h). */
static int spr_colour_reg(vr_render *r, int num)
{
    struct TagItem t[3];
    ULONG even = 16, odd = 16;
    struct Screen *scr = r->win->WScreen;
    if (!r->cm || !scr)
        return -1;
    t[0].ti_Tag = VTAG_SPEVEN_BASE_GET;
    t[0].ti_Data = 16;
    t[1].ti_Tag = VTAG_SPODD_BASE_GET;
    t[1].ti_Data = 16;
    t[2].ti_Tag = TAG_DONE;
    if (!VideoControl(r->cm, t)) {
        even = t[0].ti_Data;
        odd = t[1].ti_Data;
    }
    return vc_sprite_colour_reg(num, (int)even, (int)odd, (int)GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH),
                                (int)r->cm->Count);
}

/* An image of sh lines from planes a and b, for a sprite engine `width`
 * pixels wide (AllocSpriteDataA converts and pads; the planes need not be
 * in chip RAM). */
static struct ExtSprite *spr_alloc(const UWORD *a, const UWORD *b, int sh, int width)
{
    struct BitMap bm;
    struct TagItem t[3];
    InitBitMap(&bm, 2, VC_SPRITE_W, sh);
    bm.Planes[0] = (PLANEPTR)a;
    bm.Planes[1] = (PLANEPTR)b;
    t[0].ti_Tag = SPRITEA_Width;
    t[0].ti_Data = (ULONG)width;
    t[1].ti_Tag = SPRITEA_OutputHeight;
    t[1].ti_Data = (ULONG)sh;
    t[2].ti_Tag = TAG_DONE;
    return AllocSpriteDataA(&bm, t);
}

/* A sprite of our own on the window's screen: the first free one whose
 * colours are not text colours. Asked once a screen (spr_none). */
static int spr_get(vr_render *r, int sh)
{
    static const WORD widths[3] = { 16, 32, 64 };
    UWORD zero[VC_SPRITE_H];
    int wi, k;
    if (r->spr_num >= 0)
        return 1;
    if (r->spr_none)
        return 0;
    r->spr_none = 1;
    for (k = 0; k < VC_SPRITE_H; k++)
        zero[k] = 0;
    for (wi = 0; wi < 3; wi++) {
        /* the engine's width is the display's (Intuition's pointer sets
         * it): a sprite of another width is refused, so each is tried */
        struct ExtSprite *es = spr_alloc(zero, zero, sh, widths[wi]);
        if (!es)
            continue;
        for (k = 0; k < 6; k++) {
            struct TagItem t[2];
            LONG num;
            int reg = spr_colour_reg(r, spr_order[k]);
            if (reg < 0)
                continue;
            t[0].ti_Tag = GSTAG_SPRITE_NUM;
            t[0].ti_Data = (ULONG)spr_order[k];
            t[1].ti_Tag = TAG_DONE;
            num = GetExtSpriteA(es, t);
            if (num < 0)
                continue;
            r->spr_num = (BYTE)num;
            r->spr_reg = (WORD)reg;
            r->spr_w = widths[wi];
            r->spr_es[0] = es;
            r->spr_es[1] = 0;
            r->spr_cur = 0;
            r->spr_key[0] = 1; /* the empty image */
            r->spr_key[1] = 0;
            r->spr_rgb[0] = r->spr_rgb[1] = 0xFFFFFFFFUL;
            GetRGB32(r->cm, (ULONG)reg, 1, r->spr_old[0]);
            GetRGB32(r->cm, (ULONG)reg + 1, 1, r->spr_old[1]);
            r->spr_none = 0; /* the first spr_image puts an image on it (ChangeExtSpriteA) */
            return 1;
        }
        FreeSpriteData(es);
    }
    return 0;
}

/* Some sprite's colours on this screen are not text colours. */
static int spr_colours_ok(vr_render *r)
{
    int k;
    for (k = 0; k < 6; k++)
        if (spr_colour_reg(r, spr_order[k]) >= 0)
            return 1;
    return 0;
}

/* The sprite back to the system: it shows the never-freed blank, its
 * colour registers get back what they held, its images go. */
static void spr_free(vr_render *r)
{
    struct ViewPort *vp;
    int wi, k, keep = -1;
    if (r->spr_num < 0 || !r->win || !r->win->WScreen)
        return;
    vp = &r->win->WScreen->ViewPort;
    wi = r->spr_w == 64 ? 2 : r->spr_w == 32 ? 1 : 0;
    if (!spr_blank[wi]) {
        UWORD zero[2];
        zero[0] = zero[1] = 0;
        spr_blank[wi] = spr_alloc(zero, zero, 1, r->spr_w);
    }
    if (spr_blank[wi]) {
        spr_blank[wi]->es_SimpleSprite.num = (UWORD)r->spr_num;
        ChangeExtSpriteA(vp, r->spr_es[(int)r->spr_cur], spr_blank[wi], 0);
    } else {
        keep = r->spr_cur; /* no blank to show (no chip RAM left): the image shown stays allocated */
    }
    FreeSprite((WORD)r->spr_num);
    for (k = 0; k < 2; k++)
        SetRGB32(vp, (ULONG)r->spr_reg + k, r->spr_old[k][0], r->spr_old[k][1], r->spr_old[k][2]);
    WaitTOF(); /* the display fetches the blank from here: the images may go */
    for (k = 0; k < 2; k++)
        if (r->spr_es[k] && k != keep)
            FreeSpriteData(r->spr_es[k]);
    r->spr_es[0] = r->spr_es[1] = 0;
    r->spr_num = -1;
    r->spr_vis = 0;
}

/* Show image (a, b) under key: made into the image not shown, then swapped
 * in (the one shown before stays until the next swap: the display may be
 * fetching it this frame). 0 when it cannot be made. */
static int spr_image(vr_render *r, ULONG key, const UWORD *a, const UWORD *b, int sh)
{
    struct ViewPort *vp = &r->win->WScreen->ViewPort;
    int nx = !r->spr_cur;
    struct ExtSprite *es;
    if (r->spr_key[(int)r->spr_cur] == key)
        return 1;
    if (r->spr_es[nx])
        FreeSpriteData(r->spr_es[nx]);
    r->spr_es[nx] = es = spr_alloc(a, b, sh, r->spr_w);
    r->spr_key[nx] = 0;
    if (!es)
        return 0;
    es->es_SimpleSprite.num = (UWORD)r->spr_num;
    ChangeExtSpriteA(vp, r->spr_es[(int)r->spr_cur], es, 0);
    MoveSprite(vp, &es->es_SimpleSprite, r->spr_x, r->spr_y);
    r->spr_cur = (BYTE)nx;
    r->spr_key[nx] = key;
    r->n_spr_images++;
    return 1;
}

/* The sprite off the screen (the empty image): the blink's off phase, a
 * cursor that is not to be seen, the planes drawing it instead. */
static void spr_hide(vr_render *r)
{
    UWORD zero[VC_SPRITE_H];
    int k;
    if (r->spr_num < 0 || !r->spr_vis)
        return;
    for (k = 0; k < VC_SPRITE_H; k++)
        zero[k] = 0;
    spr_image(r, 1, zero, zero, 1);
    r->spr_vis = 0;
}

/* The colour register pair: colour 1 the cursor's, colour 2 the glyph's. */
static void spr_colours(vr_render *r, ULONG c1, ULONG c2)
{
    struct ViewPort *vp = &r->win->WScreen->ViewPort;
    ULONG c[2];
    int k;
    c[0] = c1;
    c[1] = c2;
    for (k = 0; k < 2; k++)
        if (r->spr_rgb[k] != c[k]) {
            r->spr_rgb[k] = c[k];
            SetRGB32(vp, (ULONG)r->spr_reg + k, ((c[k] >> 16) & 0xFF) * 0x01010101UL,
                     ((c[k] >> 8) & 0xFF) * 0x01010101UL, (c[k] & 0xFF) * 0x01010101UL);
        }
}

/* May a sprite stand for this window's cursor: nothing covers the text,
 * it is the active window, its screen the frontmost? (A sprite is above
 * every window and screen.) */
static int spr_visible(vr_render *r)
{
    struct Window *w = r->win;
    int ok;
    if (!(w->Flags & WFLG_WINDOWACTIVE) || IntuitionBase->FirstScreen != w->WScreen)
        return 0;
    LockLayer(0, w->WLayer);
    ok = layer_whole(r);
    UnlockLayer(w->WLayer);
    return ok;
}

/* The cursor at screen cell (x, y) as the sprite (CC2): 1 when it is shown
 * there; 0 and the reason in spr_env (render/chips.h) when the planes are
 * to draw it. */
static int spr_show(vr_render *r, int x, int y)
{
    vc_cursor_env e;
    vc_sprite_geom g;
    UWORD a[VC_SPRITE_H], b[VC_SPRITE_H];
    UBYTE rows[VC_SPRITE_H * 2];
    const UBYTE *glyph = 0;
    struct Screen *scr = r->win->WScreen;
    int style = vt_cursor_style(r->t), dx, dy, w, h, code = 0, k, choice;
    long sx = 0, sy = 0;
    ULONG key = 0;
    e.native = e.visible = e.geom = e.pos = e.plain_cell = e.image = e.colours = e.have_sprite = 0;
    g.sw = g.sh = g.lace = 0;
    g.sprite_ns = g.screen_ns = 140;
    e.native = r->spr_sns && scr && r->planar && !r->truecolor && chip_planar(r);
    if (e.native)
        e.visible = spr_visible(r);
    if (e.visible)
        e.geom = vc_sprite_geom_of(r->spr_sns, spr_sprite_ns(r->cm), r->spr_lace, r->cw, r->ch, &g);
    if (e.geom) {
        struct ViewPort *vp = &scr->ViewPort;
        long px = (long)r->win->LeftEdge + r->ox + (long)x * r->cw;
        long py = (long)r->win->TopEdge + r->oy + (long)y * r->ch;
        if (vp->RasInfo) {
            px -= vp->RasInfo->RxOffset; /* a screen bigger than the display, scrolled */
            py -= vp->RasInfo->RyOffset;
        }
        e.pos = vc_sprite_pos(&g, px, py, &sx, &sy);
    }
    if (e.pos) {
        /* the cell under the cursor: a glyph of the bitmap font, one cell
         * wide, or nothing -- anything else the planes draw */
        int ncells, gy = y + r->jump;
        const vt_cell *c = vt_row(r->t, gy, &ncells);
        e.plain_cell = !vt_row_size(r->t, gy) && r->ch <= VC_SPRITE_H * 2;
        if (e.plain_cell && c && x < ncells) {
            vt_u32 cp = cell_char(r, &c[x]);
            vt_glyph gl = vt_map_glyph(cp, r->enc);
            WORD obpr;
            if (c[x].width != 1 || vt_cell_font(r->t, &c[x]) || gl.kind != VT_GLYPH_FONT ||
                outline_glyph(r, cp, 1, &obpr))
                e.plain_cell = 0;
            else
                code = gl.code;
        }
    }
    if (e.plain_cell) {
        int with_glyph = style <= 2 && code && code != ' '; /* a block over a glyph shows it */
        /* the image's key: what it shows -- glyph, shape, size and scale (1 is the empty image) */
        key = 0x80000000UL | (ULONG)(with_glyph ? code : 0) | ((ULONG)style << 8) | ((ULONG)g.sw << 12) |
              ((ULONG)g.sh << 17) | ((ULONG)g.lace << 24) | ((ULONG)(g.sprite_ns / 35) << 25);
        if (r->spr_num >= 0 && r->spr_key[(int)r->spr_cur] == key) {
            e.image = 1; /* shown already: nothing to make (the common case: a cursor on blanks) */
        } else if (with_glyph && !(r->glyphs && r->cw == 8)) {
            e.image = 0; /* no 8-pixel glyph rows to put in it */
        } else {
            if (with_glyph) {
                for (k = 0; k < r->ch; k++)
                    rows[k] = r->glyphs[code * r->ch + k];
                glyph = rows;
            }
            vc_cursor_rect(style, r->cw, r->ch, &dx, &dy, &w, &h);
            e.image = vc_sprite_image(&g, dx, dy, w, h, glyph, a, b);
        }
    }
    if (e.image)
        e.colours = r->spr_num >= 0 || r->spr_none || spr_colours_ok(r);
    if (e.colours)
        e.have_sprite = spr_get(r, g.sh);
    choice = vc_cursor_choice(&e);
    r->spr_env = (BYTE)choice;
    if (choice != VC_CUR_SPRITE)
        return 0;
    spr_colours(r, vr_pen_rgb(r, (UBYTE)(r->cursor_ink != VR_KEEP ? r->cursor_ink : r->pen_default_fg)),
                vr_pen_rgb(r, r->pen_default_bg));
    if (sx != r->spr_x || sy != r->spr_y) {
        r->spr_x = (WORD)sx;
        r->spr_y = (WORD)sy;
        if (r->spr_key[(int)r->spr_cur] == key) {
            MoveSprite(&scr->ViewPort, &r->spr_es[(int)r->spr_cur]->es_SimpleSprite, r->spr_x, r->spr_y);
            r->n_spr_moves++;
        }
    }
    if (!spr_image(r, key, a, b, g.sh)) {
        spr_hide(r);
        return 0;
    }
    r->spr_ns = (WORD)g.sprite_ns;
    r->spr_vis = 1;
    return 1;
}

/* Draw (on) or erase (off) the cursor: the cell (a block), or its two
 * bottom rows or left columns, filled with the cursor colour -- the
 * profile's, else the default foreground -- and a block's glyph redrawn in
 * the default background; off repaints the cell. Only a double-size row's
 * block is inverted (COMPLEMENT): that flips pen NUMBERS, not colours, so
 * on a screen whose pens are not paired by colour it drew anything (rig
 * 2026-10-03, 32 pens on UP-Term's screen: black in pen 17 inverted to 14,
 * a cyan cursor). */
/* The rectangle inverted in the planes of m (COMPLEMENT under a write mask). */
static void cursor_flip(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, UBYTE m)
{
    r->mask |= m; /* planes not in use held zeros; they do no longer */
    GFX(r);
    SetDrMd(r->rp, COMPLEMENT);
    SetWriteMask(r->rp, m);
    RectFill(r->rp, x0, y0, x1, y1);
    SetDrMd(r->rp, JAM2);
    SetWriteMask(r->rp, r->mask_on ? r->mask : 0xFF);
}

static void cursor_draw(vr_render *r, int on)
{
    ULONG ink = r->cursor_ink != VR_KEEP ? r->cursor_ink : r->pen_default_fg;
    int wide = !r->view && vt_row_size(r->t, r->cursor_y) ? 2 : 1; /* a double-size row */
    WORD px = (WORD)(r->ox + r->cursor_x * r->cw * wide), py = r->oy + r->cursor_y * r->ch;
    WORD x1, y1;
    int style = vt_cursor_style(r->t);   /* DECSCUSR */
    int dx, dy, w, h;
    vc_cursor_rect(style, r->cw * wide, r->ch, &dx, &dy, &w, &h); /* a block, an underline, a bar */
    px = (WORD)(px + dx);
    py = (WORD)(py + dy);
    x1 = (WORD)(px + w - 1);
    y1 = (WORD)(py + h - 1);
    GFX(r);
    if (on)
        r->n_plane_cursor++;
    if (!on) {
        if (r->cursor_colorful) {
            /* the cell was filled: repaint it normally */
            r->cursor_colorful = 0;
            draw_rows(r, r->cursor_x, r->cursor_y, r->cursor_x + 1, r->cursor_y + 1);
        } else if (r->cursor_flip) {
            cursor_flip(r, px, py, x1, y1, r->cursor_flip);
            r->cursor_flip = 0;
        } else {
            SetDrMd(r->rp, COMPLEMENT);
            all_planes(r);
            RectFill(r->rp, px, py, x1, y1);
            SetDrMd(r->rp, JAM2);
        }
        return;
    }
    if (r->planar && r->cursor_ink == VR_KEEP) {
        /* A planar screen, no cursor colour of its own: the cell (or the
         * strip of an underline or bar) flipped in the planes in use, as
         * the ROM console draws its cursor -- one fill on, one off, and no
         * pen that would bring another plane into use (the cursor's pen
         * made every scroll move its planes too; S1). A blank screen has
         * none yet: the default pens' planes then. */
        UBYTE m = r->mask;
        if (!m)
            m = (UBYTE)(r->pen_default_fg ^ r->pen_default_bg);
        if (!m)
            m = 1;
        r->cursor_flip = m;
        r->cursor_colorful = 0;
        cursor_flip(r, px, py, x1, y1, m);
        return;
    }
    if (style > 2) {
        /* underline, bar: the strip in the cursor colour */
        ink_a(r, ink);
        RectFill(r->rp, px, py, x1, y1);
        r->cursor_colorful = 1;
        return;
    }
    if (wide == 1) {
        int ncells;
        const vt_cell *c = vt_row(r->t, r->cursor_y, &ncells);
        int have = c && r->cursor_x < ncells;
        vt_glyph g = { VT_GLYPH_FONT, 0 };
        struct TextFont *font = r->font;
        if (have) {
            g = vt_map_glyph(cell_char(r, &c[r->cursor_x]), r->enc);
            if (g.kind == VT_GLYPH_MISSING)
                g.code = 1;
        }
        if (have && g.kind != VT_GLYPH_FONT) {
            /* box / block / line on the cursor colour, drawn in the default
             * background -- draw_special fills the cell with `bg` itself */
            r->cursor_colorful = 1;
            draw_special(r, px, py, g, r->pen_default_bg, ink);
            return;
        }
        ink_ab(r, r->pen_default_bg, ink);
        RectFill(r->rp, px, py, x1, y1);
        if (have && c[r->cursor_x].width == 1) {
            WORD obpr;
            const UBYTE *om = outline_glyph(r, cell_char(r, &c[r->cursor_x]), 1, &obpr);
            if (om) {
                /* an outline glyph under the cursor, in the background colour */
                ink_a(r, r->pen_default_bg);
                SetDrMd(r->rp, JAM1);
                BltTemplate((PLANEPTR)om, 0, obpr, r->rp, px, py, r->cw, r->ch);
                SetDrMd(r->rp, JAM2);
                r->cursor_colorful = 1;
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
        r->cursor_colorful = 1;
        return;
    }
    r->cursor_colorful = 0;
    SetDrMd(r->rp, COMPLEMENT);
    all_planes(r);
    RectFill(r->rp, px, py, x1, y1);
    SetDrMd(r->rp, JAM2);
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
         * it is blank again */
        UBYTE was = r->blank;
        cursor_draw(r, 0);
        r->cursor_drawn = 0;
        r->blank = was;
    }
}

void vr_cursor_on(vr_render *r)
{
    int x, y;
    if (!r->win)
        return; /* no window (before vr_init, after vr_free) */
    pay_owed(r); /* the cursor's row may be one a scroll left owed (CC1) */
    if (r->hidden) {
        spr_hide(r);
        return;
    }
    vt_cursor(r->t, &x, &y);
    y -= r->jump; /* its screen row (vr_scroll's jump) */
    if (r->cursor_drawn && (x != r->cursor_x || y != r->cursor_y))
        vr_cursor_off(r);
    if (!(vt_modes(r->t) & VT_MODE_CURSOR_VISIBLE) || x >= vis_cols(r) || y >= vis_rows(r) || y < 0 || r->view) {
        spr_hide(r);
        return;
    }
    if (spr_show(r, x, y)) {
        /* CC2: a hardware sprite is the cursor -- nothing in the planes */
        vr_cursor_off(r);
        r->cursor_x = (WORD)x;
        r->cursor_y = (WORD)y;
        return;
    }
    spr_hide(r); /* the planes draw it (covered, RTG, a glyph the sprite cannot show ...) */
    if (!r->cursor_drawn) {
        UBYTE was = r->blank;
        r->cursor_x = (WORD)x;
        r->cursor_y = (WORD)y;
        cursor_draw(r, 1);
        r->cursor_drawn = 1;
        r->blank = was; /* vr_scroll takes the cursor off before it looks */
    }
}

void vr_cursor_hide(vr_render *r)
{
    vr_cursor_off(r);
    spr_hide(r);
}

int vr_cursor_watch(vr_render *r)
{
    if (!r->win || !r->spr_vis)
        return 0;
    if (!spr_visible(r) || spr_sprite_ns(r->cm) != r->spr_ns) {
        /* covered, another window active, another screen in front: the
         * planes draw the cursor from here (they clip to the layer); or
         * the pointer changed the sprites' pixel size: a new image */
        vr_cursor_off(r);
        vr_cursor_on(r);
    }
    return r->spr_vis;
}
