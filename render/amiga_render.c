/* The Amiga renderer; see amiga_render.h. */
#include "amiga_render.h"

#include <exec/memory.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#define RUN_MAX 256

/* ---- palette ------------------------------------------------------------ */

static const ULONG ansi16[16] = {
    0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
    0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00, 0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
};

ULONG vr_palette_rgb(int i)
{
    static const UBYTE level[6] = { 0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF };
    if (i < 16)
        return ansi16[i];
    if (i < 232) {
        i -= 16;
        return ((ULONG)level[i / 36] << 16) | ((ULONG)level[(i / 6) % 6] << 8) | level[i % 6];
    }
    i = 8 + (i - 232) * 10;
    return ((ULONG)i << 16) | ((ULONG)i << 8) | (ULONG)i;
}

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

/* The screen pen for a resolved engine colour. */
static UBYTE pen_for(vr_render *r, vt_u16 c, int is_bg)
{
    if (c == VT_COLOR_DEFAULT)
        return r->pen_default_fg;
    if (c == VT_COLOR_DEFAULT_BG)
        return r->pen_default_bg;
    if (vt_personality(r->t) == VT_AMIGA)
        return (UBYTE)(c & 0xFF); /* amiga colours are screen pens */
    if (c & VT_COLOR_RGB) {
        vt_u16 k = (vt_u16)(c & 0x7FFF);
        LONG p;
        if (!r->rgb_pens)
            r->rgb_pens = (UBYTE *)AllocVec(32768, MEMF_ANY | MEMF_CLEAR);
        if (r->rgb_pens && r->rgb_pens[k])
            return (UBYTE)(r->rgb_pens[k] - 1);
        p = obtain(r, ((ULONG)((k >> 10) & 31) << 19) | ((ULONG)((k >> 5) & 31) << 11) |
                      ((ULONG)(k & 31) << 3));
        if (p < 0)
            return is_bg ? r->pen_default_bg : r->pen_default_fg;
        if (r->rgb_pens && r->n_rgb < 64) {
            /* kept until vr_free, so the colour cannot be taken away */
            r->rgb_pens[k] = (UBYTE)(p + 1);
            r->rgb_obtained[r->n_rgb++] = p;
        } else {
            ReleasePen(r->cm, (ULONG)p); /* no room: used once, not cached */
        }
        return (UBYTE)p;
    }
    c &= 0xFF;
    if (!r->have[c]) {
        LONG p = obtain(r, vr_palette_rgb(c));
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
    r->rgb_pens = 0;
    r->n_rgb = 0;
    r->cursor_drawn = 0;
    r->cursor_x = r->cursor_y = 0;
    r->view = 0;
    r->sel = 0;
    SetFont(r->rp, font);
    vr_layout(r);
}

void vr_free(vr_render *r)
{
    int i;
    if (r->cm)
        for (i = 0; i < 256; i++)
            if (r->obtained[i] >= 0)
                ReleasePen(r->cm, (ULONG)r->obtained[i]);
    if (r->cm)
        for (i = 0; i < r->n_rgb; i++)
            ReleasePen(r->cm, (ULONG)r->rgb_obtained[i]);
    r->n_rgb = 0;
    if (r->rgb_pens)
        FreeVec(r->rgb_pens);
    r->rgb_pens = 0;
}

int vr_layout(vr_render *r)
{
    struct Window *w = r->win;
    WORD iw = w->Width - w->BorderLeft - w->BorderRight;
    WORD ih = w->Height - w->BorderTop - w->BorderBottom;
    WORD cols = iw / r->cw, rows = ih / r->ch;
    int changed;
    r->ox = w->BorderLeft;
    r->oy = w->BorderTop;
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

static void fill(vr_render *r, WORD x0, WORD y0, WORD x1, WORD y1, UBYTE pen)
{
    if (x1 < x0 || y1 < y0)
        return;
    SetAPen(r->rp, pen);
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
static void draw_box(vr_render *r, WORD px, WORD py, vt_u8 code, UBYTE fg)
{
    WORD cx = px + r->cw / 2, cy = py + r->ch / 2;
    WORD x1 = px + r->cw - 1, y1 = py + r->ch - 1;
    int arm;
    SetAPen(r->rp, fg);
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

static void draw_block(vr_render *r, WORD px, WORD py, vt_u8 code, UBYTE fg, UBYTE bg)
{
    WORD w = r->cw, h = r->ch;
    if (code & 0x80) {
        SetABPenDrMd(r->rp, fg, bg, JAM2);
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

static void draw_special(vr_render *r, WORD px, WORD py, vt_glyph g, UBYTE fg, UBYTE bg)
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
        SetAPen(r->rp, fg);
        if (g.code & 1)
            line(r, px, y1, x1, py);
        if (g.code & 2)
            line(r, px, py, x1, y1);
        break;
    case VT_GLYPH_HLINE: {
        WORD y = py + (WORD)((r->ch - 1) * g.code / 7);
        SetAPen(r->rp, fg);
        line(r, px, y, x1, y);
        break;
    }
    case VT_GLYPH_DIAMOND: {
        WORD cx = px + r->cw / 2, cy = py + r->ch / 2, rx = r->cw / 2 - 1, ry = r->ch / 2 - 1, d;
        SetAPen(r->rp, fg);
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

static ULONG style_of(vt_u8 attr)
{
    ULONG s = 0;
    if (attr & VT_ATTR_BOLD)
        s |= FSF_BOLD;
    if (attr & VT_ATTR_UNDERLINE)
        s |= FSF_UNDERLINED;
    if (attr & VT_ATTR_ITALIC)
        s |= FSF_ITALIC;
    return s;
}

static void flush_run(vr_render *r, UBYTE *run, int n, WORD px, WORD py, UBYTE fg, UBYTE bg,
                      vt_u8 attr)
{
    int i;
    if (!n)
        return;
    if (!(attr & (VT_ATTR_UNDERLINE | VT_ATTR_STRIKE))) {
        /* a run of blanks (erases, clears, line ends) is a rectangle fill:
         * much cheaper than rendering spaces through the font */
        for (i = 0; i < n && run[i] == ' '; i++)
            ;
        if (i == n) {
            fill(r, px, py, px + n * r->cw - 1, py + r->ch - 1, bg);
            return;
        }
    }
    SetABPenDrMd(r->rp, fg, bg, JAM2);
    SetSoftStyle(r->rp, style_of(attr), FSF_BOLD | FSF_UNDERLINED | FSF_ITALIC);
    Move(r->rp, px, py + r->base);
    Text(r->rp, (STRPTR)run, n);
    if (attr & VT_ATTR_STRIKE) {
        SetAPen(r->rp, fg);
        line(r, px, py + r->ch / 2, px + n * r->cw - 1, py + r->ch / 2);
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
static void draw_rows(vr_render *r, int x0, int y0, int x1, int y1)
{
    UBYTE run[RUN_MAX];
    int y, x, n;
    if (r->hidden)
        return;
    if (x1 > r->cols)
        x1 = r->cols;
    if (y1 > r->rows)
        y1 = r->rows;
    for (y = y0; y < y1; y++) {
        int ncells;
        const vt_cell *c = vt_row(r->t, y - r->view, &ncells);
        int gy = y - r->view;
        WORD py = r->oy + y * r->ch, run_x = 0;
        UBYTE run_fg = 0, run_bg = 0;
        vt_u8 run_attr = 0;
        /* the last cell's colours: runs of equal cells skip the lookups */
        vt_u16 last_f = 0xFFFF, last_b = 0xFFFF;
        vt_u8 last_a = 0;
        UBYTE fg = 0, bg = 0;
        if (!c)
            continue;
        n = 0;
        for (x = x0; x < x1 && x < ncells; x++) {
            vt_u16 f, b;
            vt_glyph g;
            vt_u8 attr;
            if (c[x].width == 0) {
                /* the right half of a wide glyph: its '?' took the left */
                vt_resolve_colors(r->t, &c[x], &f, &b);
                flush_run(r, run, n, run_x, py, run_fg, run_bg, run_attr);
                n = 0;
                fill(r, r->ox + x * r->cw, py, r->ox + (x + 1) * r->cw - 1, py + r->ch - 1,
                     pen_for(r, b, 1));
                continue;
            }
            if (c[x].fg != last_f || c[x].bg != last_b || c[x].attr != last_a || r->sel) {
                last_f = c[x].fg;
                last_b = c[x].bg;
                last_a = c[x].attr;
                vt_resolve_colors(r->t, &c[x], &f, &b);
                fg = pen_for(r, f, 0);
                bg = pen_for(r, b, 1);
                if (selected(r, x, gy)) {
                    UBYTE tmp = fg;
                    fg = bg;
                    bg = tmp;
                }
            }
            attr = (vt_u8)(c[x].attr & (VT_ATTR_BOLD | VT_ATTR_UNDERLINE | VT_ATTR_ITALIC |
                                        VT_ATTR_STRIKE));
            if (c[x].ch < 0x80) {
                g.kind = VT_GLYPH_FONT; /* ASCII: the font's own character */
                g.code = (vt_u8)c[x].ch;
            } else {
                g = vt_map_glyph(c[x].ch, r->enc);
            }
            if (g.kind != VT_GLYPH_FONT) {
                flush_run(r, run, n, run_x, py, run_fg, run_bg, run_attr);
                n = 0;
                draw_special(r, r->ox + x * r->cw, py, g, fg, bg);
                continue;
            }
            if (n && (fg != run_fg || bg != run_bg || attr != run_attr || n == RUN_MAX)) {
                flush_run(r, run, n, run_x, py, run_fg, run_bg, run_attr);
                n = 0;
            }
            if (!n) {
                run_x = r->ox + x * r->cw;
                run_fg = fg;
                run_bg = bg;
                run_attr = attr;
            }
            run[n++] = g.code;
        }
        flush_run(r, run, n, run_x, py, run_fg, run_bg, run_attr);
        if (r->cursor_drawn && r->cursor_y == y && r->cursor_x >= x0 && r->cursor_x < x1)
            r->cursor_drawn = 0; /* the cursor cell was just painted over */
    }
    SetSoftStyle(r->rp, 0, FSF_BOLD | FSF_UNDERLINED | FSF_ITALIC);
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
    vt_u16 f, b;
    if (r->hidden || r->view)
        return;
    vr_cursor_off(r);
    /* The vacated rows must come out in the personality's default
     * background (the engine's scroll contract): the Amiga global
     * background pen, for one, is not always pen 0. */
    blank.ch = ' ';
    blank.fg = VT_COLOR_DEFAULT;
    blank.bg = VT_COLOR_DEFAULT;
    blank.attr = 0;
    blank.width = 1;
    vt_resolve_colors(r->t, &blank, &f, &b);
    SetBPen(r->rp, pen_for(r, b, 1));
    ScrollRaster(r->rp, 0, dy, r->ox, r->oy + top * r->ch, r->ox + r->cols * r->cw - 1,
                 r->oy + bottom * r->ch - 1);
}

static void cursor_flip(vr_render *r)
{
    WORD px = r->ox + r->cursor_x * r->cw, py = r->oy + r->cursor_y * r->ch;
    SetDrMd(r->rp, COMPLEMENT);
    SetWriteMask(r->rp, 0xFF);
    RectFill(r->rp, px, py, px + r->cw - 1, py + r->ch - 1);
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
    if (!(vt_modes(r->t) & VT_MODE_CURSOR_VISIBLE) || x >= r->cols || y >= r->rows || r->view)
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
