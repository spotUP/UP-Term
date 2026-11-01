/* aa_ink -- see aa_ink.h */
#include "aa_ink.h"
#include "aa_ink_rule.h"
#include "ttf.h"

#include <exec/memory.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/graphics.h>

void aa_ink_init(aa_ink *a, const aa_ink_ops *ops)
{
    a->ops = *ops;
    a->ok = a->shades_ok = 0;
    a->fg = a->bg = 0;
    a->tab = 0;
    a->use[0] = a->use[1] = a->use[2] = 0;
    a->shade[0] = a->shade[1] = a->shade[2] = 0;
    a->mask = 0;
    a->mask_size = 0;
}

void aa_ink_free(aa_ink *a)
{
    if (a->mask) {
        WaitBlit(); /* a template blit may still read it */
        FreeVec(a->mask);
    }
    FreeVec(a->tab);
    a->tab = 0;
    a->mask = 0;
    a->mask_size = 0;
    a->ok = 0;
}

void aa_ink_forget(aa_ink *a)
{
    a->ok = 0;
}

UBYTE *aa_ink_room(UBYTE **buf, ULONG *size, ULONG n, ULONG flags)
{
    if (*size >= n)
        return *buf;
    if (*buf) {
        if (flags & MEMF_CHIP)
            WaitBlit();
        FreeVec(*buf);
    }
    *size = 0;
    *buf = (UBYTE *)AllocVec(n, flags);
    if (*buf)
        *size = n;
    return *buf;
}

/* The antialiased glyph's colours for text fg over background bg (0xRRGGBB), made once per pair: the table of
 * every coverage, and the shade inks for a screen drawn with pens -- a direct colour of its own on a
 * true-colour screen, else the pen the screen finds nearest, used only when it shows the shade closer than the
 * text and background pens do. 0 when there is no memory for the table. */
static int colours(aa_ink *a, ULONG fg, ULONG bg, int lut, int truecolor)
{
    int i;
    if (!a->tab) {
        a->tab = (aa_ink_rgb *)AllocVec(256 * sizeof(aa_ink_rgb), MEMF_ANY);
        if (!a->tab)
            return 0;
        a->ok = 0;
    }
    if (!a->ok || a->fg != fg || a->bg != bg) {
        aa_ink_build_tab(a->tab, fg, bg);
        a->fg = fg;
        a->bg = bg;
        a->ok = 1;
        a->shades_ok = 0;
    }
    if (lut || a->shades_ok)
        return 1;
    for (i = 0; i < 3; i++) {
        ULONG want = aa_ink_shade_target(a->tab, i);
        a->use[i] = 0;
        if (truecolor) {
            a->shade[i] = a->ops.truecolor_ink(a->ops.u, want);
            a->use[i] = 1;
        } else {
            UBYTE p = a->ops.nearest_pen(a->ops.u, want);
            ULONG prgb = a->ops.pen_rgb(a->ops.u, p);
            a->shade[i] = p;
            a->use[i] = (UBYTE)aa_ink_shade_accept(a->ops.dist(want, prgb), a->ops.dist(want, fg), a->ops.dist(want, bg));
        }
    }
    a->shades_ok = 1;
    return 1;
}

int aa_ink_draw(aa_ink *a, struct RastPort *rp, struct Library *cgx, int truecolor, WORD px, WORD py,
                const UBYTE *cov, WORD w, WORD h, ULONG fg_ink, ULONG fg_rgb, ULONG bg_ink, ULONG bg_rgb)
{
    int lut = truecolor && cgx, i, lo, bpr;
        if (!colours(a, fg_rgb, bg_rgb, lut, truecolor))
        return 0;
    if (lut) {
        vr_cgx_write_lut(cgx, (APTR)cov, 0, 0, (UWORD)w, rp, (APTR)a->tab, (UWORD)px, (UWORD)py, (UWORD)w,
                         (UWORD)h, VR_CTABFMT_XRGB8);
        return 1;
    }
    if (!a->use[0] && !a->use[1] && !a->use[2])
        return 0;
    bpr = ((w + 15) >> 4) << 1;
    if (!aa_ink_room(&a->mask, &a->mask_size, (ULONG)bpr * h, MEMF_CHIP))
        return 0;
    /* each pixel the level nearest its coverage among the background, the
     * shades the screen shows and the text: a pass per level from the
     * lightest, at the coverage halfway between it and the level below */
    a->ops.fill(a->ops.u, px, py, (WORD)(px + w - 1), (WORD)(py + h - 1), bg_ink);
    SetDrMd(rp, JAM1);
    for (i = 0, lo = 0; i < 4; i++) {
        int lv = aa_ink_level(i);
        if (i < 3 && !a->use[i])
            continue;
        WaitBlit(); /* the previous pass may still read the template */
        ttf_threshold_at(cov, w, h, a->mask, bpr, (lo + lv + 1) / 2);
        a->ops.set_pen(a->ops.u, i < 3 ? a->shade[i] : fg_ink);
        BltTemplate((PLANEPTR)a->mask, 0, (WORD)bpr, rp, px, py, w, h);
        lo = lv;
    }
    SetDrMd(rp, JAM2);
    return 1;
}
