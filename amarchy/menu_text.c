/* See menu_text.h. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <string.h>
#include "menu_text.h"
#include "menu_text_core.h"
#include "menu_font.h"
#include "../render/aa_ink.h"
#include "../render/fontpair.h"
#include "../render/outline.h"
#include "../render/pens_amiga.h"
#include "../render/pen_match.h"
#include "../prefs/prefs_core.h"

#define MT_INK_RGB 0x80000000UL /* aa_ink's direct-colour ink (render/amiga_render.c VR_INK_RGB) */
#define MT_PENS 24

struct Library *CyberGfxBase;

struct menu_text {
    struct Screen *scr;
    int rx, ry;   /* the screen's pixel in ticks, 0 when unknown */
    mtc core;
    mtc_ttf tt;
    ttf_font *font;
    BPTR fh;
    ULONG fpos;
    struct TextFont *bitmap;
    int bitmap_opened;
    struct RastPort *measure_rp;
    /* TrueType drawing */
    aa_ink ink;
    struct Library *cgx;
    int truecolor;
    LONG pen[MT_PENS];
    ULONG pen_rgb_[MT_PENS];
    int npens;
    int height, base;
    long aa_drawn;
};

static void *mt_alloc(unsigned long n)
{
    return AllocVec((ULONG)n, MEMF_ANY);
}

static void mt_free_mem(void *p)
{
    FreeVec(p);
}

static long ft_read(void *ctx, unsigned long off, void *buf, long len)
{
    menu_text *m = (menu_text *)ctx;
    LONG got;
    if (off != m->fpos && Seek(m->fh, (LONG)off, OFFSET_BEGINNING) < 0) {
        m->fpos = (ULONG)-1;
        return -1;
    }
    got = Read(m->fh, buf, len);
    m->fpos = got > 0 ? off + (ULONG)got : (ULONG)-1;
    return got;
}

static void *ft_alloc(void *ctx, unsigned long size)
{
    (void)ctx;
    return AllocVec((ULONG)size, MEMF_ANY | MEMF_CLEAR);
}

static void ft_free(void *ctx, void *p)
{
    (void)ctx;
    FreeVec(p);
}

/* ---- the pens aa_ink asks for: held until mt_close ---- */

static LONG pen_of(menu_text *m, ULONG rgb)
{
    int i;
    LONG p;
    for (i = 0; i < m->npens; i++)
        if (m->pen_rgb_[i] == rgb)
            return m->pen[i];
    if (m->npens >= MT_PENS)
        return -1;
    p = vr_pen_obtain(m->scr->ViewPort.ColorMap, rgb);
    if (p < 0)
        return -1;
    m->pen[m->npens] = p;
    m->pen_rgb_[m->npens++] = rgb;
    return p;
}

static ULONG op_truecolor_ink(void *u, ULONG rgb)
{
    menu_text *m = (menu_text *)u;
    LONG p;
    if (m->cgx)
        return MT_INK_RGB | rgb;
    p = pen_of(m, rgb);
    return p < 0 ? 1UL : (ULONG)p;
}

static UBYTE op_nearest_pen(void *u, ULONG rgb)
{
    LONG p = pen_of((menu_text *)u, rgb);
    return p < 0 ? 1 : (UBYTE)p;
}

static ULONG op_pen_rgb(void *u, UBYTE pen)
{
    menu_text *m = (menu_text *)u;
    ULONG c[3];
    GetRGB32(m->scr->ViewPort.ColorMap, pen, 1, c);
    return ((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24);
}

static long op_dist(ULONG want, ULONG c)
{
    pm_target t;
    pm_aim(want, &t);
    return pm_dist(&t, c);
}

static struct RastPort *cur_rp;

static void op_set_pen(void *u, ULONG ink)
{
    (void)u;
    SetAPen(cur_rp, (UBYTE)ink);
}

static void op_fill(void *u, WORD x0, WORD y0, WORD x1, WORD y1, ULONG ink)
{
    menu_text *m = (menu_text *)u;
    if ((ink & MT_INK_RGB) && mt_fill(m, cur_rp, x0, y0, x1, y1, ink & 0xFFFFFFUL))
        return;
    SetAPen(cur_rp, (UBYTE)ink);
    RectFill(cur_rp, x0, y0, x1, y1);
}

/* ---- the bitmap font as a glyph source: Text() draws it, so only advances ---- */

static int bm_advance(void *u, unsigned long cp)
{
    menu_text *m = (menu_text *)u;
    char ch = cp < 256 ? (char)cp : '?';
    return (int)TextLength(m->measure_rp, &ch, 1);
}

/* one candidate file spec of the font (a path without a volume is in FONTS:); 0 when it is missing or no TrueType */
static int mt_open_ttf_spec(menu_text *m, const char *spec, int px)
{
    char full[160];
    UBYTE head[4];
    ttf_io io;
    mtc_src src;
    if (!strchr(spec, ':')) {
        strcpy(full, "FONTS:");
        strcat(full, spec);
    } else
        strcpy(full, spec);
    if (!(m->fh = Open((STRPTR)full, MODE_OLDFILE)))
        return 0;
    m->fpos = 0;
    io.read = ft_read;
    io.alloc = ft_alloc;
    io.free = ft_free;
    io.ctx = m;
    if (ft_read(m, 0, head, 4) == 4 &&
        ((head[0] == 0 && head[1] == 1 && head[2] == 0 && head[3] == 0) || !memcmp(head, "true", 4)) &&
        ttf_open(&io, &m->font) == TTF_OK) {
        if (mtc_ttf_init(&m->tt, m->font, px, m->rx, m->ry)) {
            mtc_ttf_source(&src, &m->tt, mt_alloc, mt_free_mem);
            mtc_init(&m->core, &src);
            m->height = m->tt.line;
            m->base = m->tt.base;
            return 1;
        }
        ttf_close(m->font);
        m->font = 0;
    }
    Close(m->fh);
    m->fh = 0;
    return 0;
}

/* the terminal's lookup (render/outline.c engine_open): the .otag's spec, and when that is missing or no TrueType
 * the FONTS:<name>.ttf */
static int mt_open_ttf(menu_text *m, const char *name, int px)
{
    char spec[128];
    int which;
    for (which = 0; which < 2; which++)
        if (vo_ttf_spec(name, which, spec, sizeof spec) && mt_open_ttf_spec(m, spec, px))
            return 1;
    return 0;
}

menu_text *mt_open(struct Screen *scr, int square, const char *spec)
{
    menu_text *m = (menu_text *)AllocVec(sizeof(*m), MEMF_ANY | MEMF_CLEAR);
    char name[64];
    long size = 0;
    mtc_src src;
    fontpair_choice ch[FONTPAIR_MENU_MAX];
    int i, n;
    if (!m)
        return 0;
    m->scr = scr;
    /* a true-colour screen draws every colour as itself (mt_fill, mt_draw): its pens are few and taken */
    if (GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) > 8 && (m->cgx = OpenLibrary((STRPTR)"cybergraphics.library", 41)) != 0)
        m->truecolor = 1;
    menu_screen_aspect(scr, &m->rx, &m->ry);
    if (!prefs_font_parse(spec, name, sizeof name, &size))
        name[0] = 0;
    /* menu_font_open's choices, best first: each as TrueType (antialiased) before its bitmap, so the default
     * JetBrains Mono draws from FONTS:JetBrainsMono.ttf as Omarchy's does and the 16 strike is the fallback */
    n = fontpair_menu_choices(name, (int)size, square, fontpair_square_fits(scr->Height, scr->BarHeight), ch);
    for (i = 0; i < n && !m->bitmap; i++) {
        if (ch[i].ttf_px > 0 && mt_open_ttf(m, ch[i].name, ch[i].ttf_px)) {
            aa_ink_ops ops;
            ops.u = m;
            ops.truecolor_ink = op_truecolor_ink;
            ops.nearest_pen = op_nearest_pen;
            ops.pen_rgb = op_pen_rgb;
            ops.dist = op_dist;
            ops.set_pen = op_set_pen;
            ops.fill = op_fill;
            aa_ink_init(&m->ink, &ops);
            return m;
        }
        if ((m->bitmap = menu_font_open_choice(&ch[i])) != 0)
            m->bitmap_opened = 1;
    }
    if (!m->bitmap)
        m->bitmap = scr->RastPort.Font;   /* the screen's own font, not ours to close */
    m->measure_rp = (struct RastPort *)AllocVec(sizeof(struct RastPort), MEMF_ANY | MEMF_CLEAR);
    if (!m->bitmap || !m->measure_rp) {
        mt_close(m);
        return 0;
    }
    InitRastPort(m->measure_rp);
    SetFont(m->measure_rp, m->bitmap);
    src.u = m;
    src.advance = bm_advance;
    src.render = 0;
    src.alloc = mt_alloc;
    src.release = mt_free_mem;
    mtc_init(&m->core, &src);
    m->height = m->bitmap->tf_YSize;
    m->base = m->bitmap->tf_Baseline;
    return m;
}

void mt_close(menu_text *m)
{
    int i;
    if (!m)
        return;
    mtc_free(&m->core);
    if (m->font) {
        aa_ink_free(&m->ink);
        ttf_close(m->font);
    }
    if (m->fh)
        Close(m->fh);
    for (i = 0; i < m->npens; i++)
        ReleasePen(m->scr->ViewPort.ColorMap, (ULONG)m->pen[i]);
    if (m->cgx)
        CloseLibrary(m->cgx);
    if (m->bitmap_opened && m->bitmap)
        CloseFont(m->bitmap);
    if (m->measure_rp)
        FreeVec(m->measure_rp);
    FreeVec(m);
}

int mt_is_ttf(const menu_text *m) { return m->font != 0; }
int mt_direct(const menu_text *m) { return m->cgx != 0; }

int mt_fill(menu_text *m, struct RastPort *rp, int x0, int y0, int x1, int y1, ULONG rgb)
{
    if (!m->cgx)
        return 0;
    if (x1 >= x0 && y1 >= y0)
        vr_cgx_fill(m->cgx, rp, (UWORD)x0, (UWORD)y0, (UWORD)(x1 - x0 + 1), (UWORD)(y1 - y0 + 1), 0xFF000000UL | rgb);
    return 1;
}
int mt_height(const menu_text *m) { return m->height; }
int mt_baseline(const menu_text *m) { return m->base; }
struct TextFont *mt_bitmap_font(const menu_text *m) { return m->font ? 0 : m->bitmap; }

int mt_width(menu_text *m, const char *text)
{
    return mtc_width(&m->core, text);
}

int mt_width_n(menu_text *m, const char *text, int len)
{
    return mtc_width_n(&m->core, text, len);
}

int mt_prefix(menu_text *m, const char *text, int len, int maxw)
{
    return mtc_prefix(&m->core, text, len, maxw);
}

long mt_aa_glyphs(const menu_text *m)
{
    return m->aa_drawn;
}

int mt_fit(menu_text *m, const char *text, int maxw, char *out, int n)
{
    return mtc_fit(&m->core, text, maxw, out, n);
}

int mt_fit_tail(menu_text *m, const char *text, int keep, int maxw, char *out, int n)
{
    return mtc_fit_tail(&m->core, text, keep, maxw, out, n);
}

void mt_draw(menu_text *m, struct RastPort *rp, int x, int y, const char *text, ULONG fg, ULONG bg)
{
    int n;
    unsigned long cp;
    if (!m->font) { /* bitmap: Text() on the pens nearest fg and bg */
        LONG pf = pen_of(m, fg), pb = pen_of(m, bg);
        SetFont(rp, m->bitmap);
        SetABPenDrMd(rp, pf < 0 ? 1 : (UBYTE)pf, pb < 0 ? 0 : (UBYTE)pb, JAM2);
        Move(rp, x, y + m->base);
        Text(rp, (STRPTR)text, strlen(text));
        return;
    }
    cur_rp = rp;
    while ((cp = mtc_next(text, &n)), n > 0) {
        const mtc_glyph *g = mtc_glyph_of(&m->core, cp);
        if (g && g->w > 0 && g->cov) {
            LONG pf = m->cgx ? 0 : pen_of(m, fg), pb = m->cgx ? 0 : pen_of(m, bg);
            ULONG fi = m->cgx ? (MT_INK_RGB | fg) : (ULONG)(pf < 0 ? 1 : pf);
            ULONG bi = m->cgx ? (MT_INK_RGB | bg) : (ULONG)(pb < 0 ? 0 : pb);
            m->aa_drawn++;
            aa_ink_draw(&m->ink, rp, m->cgx, m->truecolor, (WORD)(x + g->left), (WORD)y, g->cov, (WORD)g->w, (WORD)g->h, fi, fg, bi, bg);
        }
        x += mtc_advance(&m->core, cp);
        text += n;
    }
}
