/* aa_ink -- coverage to ink: how an antialiased glyph (one coverage byte per pixel) is drawn in a text colour
 * over a background colour. Shared by the terminal renderer (render/amiga_render.c) and every program that
 * draws antialiased text in a window of its own (amarchy-gui's TrueType menu text); no copy of this path
 * anywhere else.
 *
 * What it does: a table of fg over bg at each of 256 coverages (ce_mix); on a true-colour screen with
 * cybergraphics, one WriteLUTPixelArray through that table; otherwise (pens) up to three shade inks (1/4, 1/2,
 * 3/4 of the way to fg) and one masked blit per level, lightest first.
 *
 * What it does not own: pens. The host answers for them through aa_ink_ops (the renderer's exact-pen hash and
 * scratch pens; a window of its own wraps vr_pen_obtain / ReleasePen) and releases them itself. aa_ink keeps
 * only the table, the shades and the chip-RAM template, freed by aa_ink_free. */
#ifndef AA_INK_H
#define AA_INK_H

#include "aa_ink_rule.h" /* aa_ink_rgb */

#include <exec/types.h>

struct Library;
struct RastPort;

/* cybergraphics.library V41 WriteLUTPixelArray (LVO -198; AROS
 * cybergraphics.conf, the CGX/P96 SDK's fd): 8-bit indices through a
 * table of 0x00RRGGBB, on true-colour screens. No SDK header here, so
 * the call is declared as vbcc's inline. */
#define VR_CTABFMT_XRGB8 0
#ifdef VR_HOST_TEST /* the host test (tests/test_render.c) links a fake of it */
LONG vr_cgx_write_lut(struct Library *, APTR src, UWORD sx, UWORD sy, UWORD smod, struct RastPort *rp,
                      APTR ctab, UWORD dx, UWORD dy, UWORD w, UWORD h, UBYTE fmt);
#else
LONG vr_cgx_write_lut(__reg("a6") struct Library *, __reg("a0") APTR src, __reg("d0") UWORD sx,
                      __reg("d1") UWORD sy, __reg("d2") UWORD smod, __reg("a1") struct RastPort *rp,
                      __reg("a2") APTR ctab, __reg("d3") UWORD dx, __reg("d4") UWORD dy,
                      __reg("d5") UWORD w, __reg("d6") UWORD h, __reg("d7") UBYTE fmt) = "\tjsr\t-198(a6)";
#endif

/* The host's pen services. Every ink is what SetAPen's caller would hand ink_pen: a pen number, or
 * VR_INK_RGB | 0xRRGGBB for a colour no pen holds. */
typedef struct aa_ink_ops {
    void *u;
    ULONG (*truecolor_ink)(void *u, ULONG rgb); /* the ink of a direct colour on a true-colour screen */
    UBYTE (*nearest_pen)(void *u, ULONG rgb);   /* the pen the screen finds nearest rgb */
    ULONG (*pen_rgb)(void *u, UBYTE pen);       /* that pen's colour */
    long (*dist)(ULONG want, ULONG c);    /* perceptual distance (pen_match pm_dist) from want to c */
    void (*set_pen)(void *u, ULONG ink);        /* SetAPen of the pen that draws ink */
    void (*fill)(void *u, WORD x0, WORD y0, WORD x1, WORD y1, ULONG ink); /* the box in the background colour */
} aa_ink_ops;

typedef struct aa_ink {
    aa_ink_ops ops;
    UBYTE ok, shades_ok;
    ULONG fg, bg;       /* 0xRRGGBB the table and shades are for */
    aa_ink_rgb *tab;       /* fg over bg at each coverage, made at the first draw (1 KB) */
    ULONG shade[3];     /* the inks of 1/4, 1/2, 3/4 of the way to fg */
    UBYTE use[3];       /* 1 when that ink shows its shade closer than fg and bg */
    UBYTE *mask;        /* a pass's template, chip RAM */
    ULONG mask_size;
} aa_ink;

void aa_ink_init(aa_ink *a, const aa_ink_ops *ops);
void aa_ink_free(aa_ink *a);
/* The pens the shades hold show other colours now (the palette changed): remake them at the next draw */
void aa_ink_forget(aa_ink *a);
/* *buf at least n bytes (fast RAM, or chip for a template): 0 when there is no memory */
UBYTE *aa_ink_room(UBYTE **buf, ULONG *size, ULONG n, ULONG flags);
/* The glyph of coverage cov (w x h bytes) in fg_ink (rgb fg_rgb) over bg_ink (bg_rgb) at px, py of rp, over the
 * whole box. cgx: the cybergraphics library of a true-colour screen (then every colour is direct), else 0 and
 * truecolor says whether the screen takes direct colours without it. 0 when it could not be (no memory, no shade
 * the screen shows): the caller draws a mask. */
int aa_ink_draw(aa_ink *a, struct RastPort *rp, struct Library *cgx, int truecolor, WORD px, WORD py,
                const UBYTE *cov, WORD w, WORD h, ULONG fg_ink, ULONG fg_rgb, ULONG bg_ink, ULONG bg_rgb);
#endif
