/* The Amiga renderer; see amiga_render.h. */
#include "amiga_render.h"
#include "painter.h"
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

static void fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, ULONG pen)
{
    if (x1 < x0 || y1 < y0)
        return;
    if (pen != r->bg_ink)
        r->blank = 0;
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

static void draw_block(vr_render *r, WORD px, WORD py, vt_u8 code, ULONG fg, ULONG bg)
{
    WORD w = r->cw, h = r->ch;
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

/* May cells be written straight into the screen's bitplanes now, at
 * any bit phase (the painter, render/painter.h)? The caller holds the
 * window's layer lock. */
static int planes_ok(vr_render *r, int aligned)
{
    struct Window *w = r->win;
    struct BitMap *bm = r->rp->BitMap;
    struct ClipRect *cr = w->WLayer ? w->WLayer->ClipRect : 0;
    WORD sx0 = w->LeftEdge + r->ox, sy0 = w->TopEdge + r->oy;
    WORD sx1 = sx0 + vis_cols(r) * r->cw - 1, sy1 = sy0 + vis_rows(r) * r->ch - 1;
    if (!r->glyphs || !bm || bm->Depth > 8 || (aligned && (sx0 & 7)))
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
        WaitBlit(); /* earlier blits (scroll, fills, Text, the cursor) finish first */
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
    int y, x, xe, n, nd, want_direct, tail_ok;
    ULONG tail_bg;
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
    for (y = y0; y < y1; y++) {
        int ncells;
        const vt_cell *c = vt_row(r->t, y - r->view + r->jump, &ncells);
        int gy = y - r->view + r->jump;
        WORD py = r->oy + y * r->ch, run_x = 0;
        vr_style st, run_st;
        /* the last cell's look: runs of equal cells skip the lookups */
        const vt_cell *last = 0;
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
    vr_cursor_off(r);
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

/* Rows [top, bottom) of the screen up by n (down when n < 0), the vacated
 * rows in the default background. */
static void raw_scroll(vr_render *r, int top, int bottom, int n)
{
    WORD dy = (WORD)(n * r->ch);
    vt_cell blank;
    vt_color f, b;
    /* The vacated rows must come out in the personality's default
     * background (the engine's scroll contract): the Amiga global
     * background pen, for one, is not always pen 0. */
    blank.ch = ' ';
    blank.fg = VT_COLOR_DEFAULT;
    blank.bg = VT_COLOR_DEFAULT;
    blank.attr = 0;
    blank.width = 1;
    vt_resolve_colors(r->t, &blank, &f, &b);
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
            UBYTE pen = (UBYTE)pen_for(r, b, 1); /* planar: never an RGB ink */
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
    SetBPen(r->rp, ink_pen(r, pen_for(r, b, 1), 1));
    ScrollRaster(r->rp, 0, dy, r->ox, r->oy + top * r->ch, r->ox + vis_cols(r) * r->cw - 1,
                 r->oy + bottom * r->ch - 1);
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
    WORD x1 = (WORD)(px + r->cw * wide - 1), y1 = (WORD)(py + r->ch - 1);
    int style = vt_cursor_style(r->t);   /* DECSCUSR */
    if (style == 3 || style == 4)
        py = (WORD)(y1 - 1);              /* underline: the two bottom rows */
    else if (style == 5 || style == 6)
        x1 = (WORD)(px + 1);              /* bar: the two left columns */
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
        r->cursor_x = (WORD)x;
        r->cursor_y = (WORD)y;
        cursor_draw(r, 1);
        r->cursor_drawn = 1;
        r->blank = was; /* vr_scroll takes the cursor off before it looks */
    }
}
