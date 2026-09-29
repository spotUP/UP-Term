/* The Amiga renderer; see amiga_render.h. */
#include "amiga_render.h"

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
    if (!(ink & VR_INK_RGB))
        return (UBYTE)ink;
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
    r->cursor_x = r->cursor_y = 0;
    r->view = 0;
    r->sel = 0;
    r->lay_rows = r->lay_cols = r->lay_x = r->lay_y = -1;
    SetFont(r->rp, font);
    r->glyphs = 0;
#ifdef VTCON_DIRECT
    extract_glyphs(r); /* and with it the 8-pixel alignment of the text */
#endif
    r->n_direct = r->n_text = 0;
    vr_layout(r);
}

void vr_set_defaults(vr_render *r, ULONG fg_rgb, ULONG bg_rgb)
{
    ULONG want[2];
    int i;
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
    r->dflt_obtained[0] = r->dflt_obtained[1] = -1;
    for (i = 0; i < VR_EXACT_SLOTS; i++)
        r->exact_key[i] = 0;
    r->n_exact = 0;
    r->scratch[0] = r->scratch[1] = -1;
    r->truecolor = 0;
    if (r->glyphs)
        FreeVec(r->glyphs);
    r->glyphs = 0;
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
    WORD lx = r->lay_x > 0 ? r->lay_x : 0, ly = r->lay_y > 0 ? r->lay_y : 0;
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
    if (r->glyphs) {
        /* the planar path needs cells on a byte boundary of the screen:
         * start the text at the next 8-pixel column (at most 7 px more) */
        WORD pad = (WORD)((8 - ((w->LeftEdge + r->ox) & 7)) & 7);
        if (cols * r->cw + pad > iw)
            cols = (iw - pad) / r->cw;
        r->ox += pad;
    }
    r->hidden = cols < 1 || rows < 1;
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
    ink_a(r, pen);
    SetDrMd(r->rp, JAM1);
    RectFill(r->rp, x0, y0, x1, y1);
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
    WORD x1 = px + r->cw - 1, y1 = py + r->ch - 1;
    fill(r, px, py, x1, y1, bg);
    switch (g.kind) {
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
            fill(r, px, py, px + n * r->cw - 1, py + r->ch - 1, st->bg);
            return;
        }
    }
    r->n_text++;
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

/* May cells be written straight into the screen's bitplanes now? The
 * caller holds the window's layer lock. */
static int direct_ok(vr_render *r)
{
    struct Window *w = r->win;
    struct BitMap *bm = r->rp->BitMap;
    struct ClipRect *cr = w->WLayer ? w->WLayer->ClipRect : 0;
    WORD sx0 = w->LeftEdge + r->ox, sy0 = w->TopEdge + r->oy;
    WORD sx1 = sx0 + vis_cols(r) * r->cw - 1, sy1 = sy0 + vis_rows(r) * r->ch - 1;
    if (!r->glyphs || !bm || bm->Depth > 8 || (sx0 & 7))
        return 0;
    if (!(GetBitMapAttr(bm, BMA_FLAGS) & BMF_STANDARD))
        return 0; /* RTG: not planar */
    {
        /* Picasso96 calls its bitmaps standard too (it hung the rig,
         * 2026-09-29): a native display bitmap is planes in chip RAM,
         * graphics card memory never is. */
        int p;
        for (p = 0; p < bm->Depth; p++)
            if (!bm->Planes[p] || !(TypeOfMem(bm->Planes[p]) & MEMF_CHIP))
                return 0;
    }
    if (!cr || cr->Next || cr->obscured)
        return 0; /* covered in part: the layer draws for us */
    return cr->bounds.MinX <= sx0 && cr->bounds.MinY <= sy0 && cr->bounds.MaxX >= sx1 &&
           cr->bounds.MaxY >= sy1;
}

/* One cell, straight into the planes: each plane byte row is the glyph,
 * its inverse, or a constant, as the two pens' bits say. */
static void direct_cell(vr_render *r, int x, int y, UBYTE ch, UBYTE fg, UBYTE bg, vt_u8 attr)
{
    struct BitMap *bm = r->rp->BitMap;
    WORD bpr = bm->BytesPerRow;
    LONG off = (LONG)(r->win->TopEdge + r->oy + y * r->ch) * bpr + ((r->win->LeftEdge + r->ox) >> 3) + x;
    const UBYTE *g0 = r->glyphs + ch * r->ch;
    UBYTE rows[32];
    int p, k, h = r->ch;
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
    for (p = 0; p < bm->Depth; p++) {
        UBYTE *d = (UBYTE *)bm->Planes[p] + off;
        int f = (fg >> p) & 1, b = (bg >> p) & 1;
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

/* Write the collected cells of row y straight into the planes when the
 * layer allows it now; returns 0 when it does not (then the caller draws
 * them through the RastPort). */
static int direct_row(vr_render *r, int y, const dcell *d, int n)
{
#ifndef VTCON_DIRECT
    /* off by default: measured slower (see extract_glyphs) */
    return r && d && y < 0 && n < 0; /* always 0 */
#else
    struct Layer *layer = r->win->WLayer;
    int i, ok;
    LockLayer(0, layer);
    ok = direct_ok(r);
    if (ok) {
        WaitBlit(); /* earlier blits (scroll, fills, Text, the cursor) finish first */
        for (i = 0; i < n; i++)
            direct_cell(r, d[i].x, y, d[i].ch, d[i].fg, d[i].bg, d[i].attr);
    }
    UnlockLayer(layer);
    r->n_direct += ok ? n : 0;
    return ok;
#endif
}

/* The style cell c draws with (selection and blink phase applied). */
static void cell_style(vr_render *r, const vt_cell *c, int selected_cell, vr_style *st)
{
    vt_color f, b, u;
    vt_resolve_colors(r->t, c, &f, &b);
    st->fg = pen_for(r, f, 0);
    st->bg = pen_for(r, b, 1);
    if (selected_cell) {
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
        cell_style(r, &c[x], selected(r, x, y - r->view), &st);
        fill(r, px, py, (WORD)(px + 2 * cw - 1), (WORD)(py + ch - 1), st.bg);
        g = vt_map_glyph(c[x].ch, r->enc);
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

static void draw_rows(vr_render *r, int x0, int y0, int x1, int y1)
{
    UBYTE run[RUN_MAX];
    dcell dc[DCELL_MAX];
    int y, x, n, nd, want_direct;
    if (r->hidden)
        return;
    want_direct = r->glyphs != 0;
    if (x1 > vis_cols(r))
        x1 = vis_cols(r);
    if (y1 > vis_rows(r))
        y1 = vis_rows(r);
    for (y = y0; y < y1; y++) {
        int ncells;
        const vt_cell *c = vt_row(r->t, y - r->view, &ncells);
        int gy = y - r->view;
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
        for (x = x0; x < x1 && x < ncells; x++) {
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
                g = vt_map_glyph(c[x].ch, r->enc);
            }
            if (g.kind != VT_GLYPH_FONT) {
                flush_run(r, run, n, run_x, py, &run_st);
                n = 0;
                draw_special(r, r->ox + x * r->cw, py, g, st.fg, st.bg);
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
        if (nd && !direct_row(r, y, dc, nd)) {
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
    int slow, fast, y, x, n, cursor = vr_cursor_blinks(r);
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
    if (r->view)
        return; /* looking at the scrollback: the live rows are not shown */
    draw_rows(r, x0, y0, x1, y1);
}

void vr_set_view(vr_render *r, int lines)
{
    int max = vt_scrollback_lines(r->t);
    if (lines < 0)
        lines = 0;
    if (lines > max)
        lines = max;
    if (lines == r->view)
        return;
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
    LONG base = vt_lines_scrolled(r->t), lo = 0, hi = -1;
    int y0, y1, any = 0;
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
    if (w->Width - w->BorderRight - 1 >= w->BorderLeft && w->Height - w->BorderBottom - 1 >= w->BorderTop)
        fill(r, w->BorderLeft, w->BorderTop, w->Width - w->BorderRight - 1,
             w->Height - w->BorderBottom - 1, r->pen_default_bg);
    r->cursor_drawn = 0;
    draw_rows(r, 0, 0, r->cols, r->rows);
}

void vr_scroll(vr_render *r, int top, int bottom, int n)
{
    WORD dy = (WORD)(n * r->ch);
    vt_cell blank;
    vt_color f, b;
    if (r->hidden || r->view)
        return;
    vr_cursor_off(r);
    if (bottom > vis_rows(r))
        bottom = vis_rows(r);
    if (top >= bottom || !vis_cols(r))
        return;
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

static void cursor_flip(vr_render *r)
{
    int wide = !r->view && vt_row_size(r->t, r->cursor_y) ? 2 : 1; /* a double-size row */
    WORD px = (WORD)(r->ox + r->cursor_x * r->cw * wide), py = r->oy + r->cursor_y * r->ch;
    WORD x1 = (WORD)(px + r->cw * wide - 1), y1 = (WORD)(py + r->ch - 1);
    int style = vt_cursor_style(r->t);   /* DECSCUSR */
    if (style == 3 || style == 4)
        py = (WORD)(y1 - 1);              /* underline: the two bottom rows */
    else if (style == 5 || style == 6)
        x1 = (WORD)(px + 1);              /* bar: the two left columns */
    SetDrMd(r->rp, COMPLEMENT);
    SetWriteMask(r->rp, 0xFF);
    RectFill(r->rp, px, py, x1, y1);
    SetDrMd(r->rp, JAM2);
}

void vr_cursor_off(vr_render *r)
{
    if (r->cursor_drawn) {
        cursor_flip(r);
        r->cursor_drawn = 0;
    }
}

void vr_cursor_on(vr_render *r)
{
    int x, y;
    if (r->hidden)
        return;
    vt_cursor(r->t, &x, &y);
    if (r->cursor_drawn && (x != r->cursor_x || y != r->cursor_y))
        vr_cursor_off(r);
    if (!(vt_modes(r->t) & VT_MODE_CURSOR_VISIBLE) || x >= vis_cols(r) || y >= vis_rows(r) || r->view)
        return;
    if (!r->cursor_drawn) {
        r->cursor_x = (WORD)x;
        r->cursor_y = (WORD)y;
        cursor_flip(r);
        r->cursor_drawn = 1;
    }
}

int vr_cell_at(const vr_render *r, WORD mx, WORD my, int *x, int *y)
{
    WORD cx = (WORD)((mx - r->ox) / r->cw), cy = (WORD)((my - r->oy) / r->ch);
    if (mx < r->ox || my < r->oy || cx >= r->cols || cy >= r->rows)
        return 0;
    *x = cx;
    *y = cy;
    return 1;
}
