/* emoji: colour emoji from Twemoji's art, for the two-cell cells of a
 * true-colour screen (ledger U4, thoughts/shared/plans/2026-10-05-emoji-colour.md).
 * Portable C89, host-tested.
 *
 * The glyphs come as one file per 256-code-point page (tools/gen_emoji.py
 * writes them; its docstring has the layout), read through the owner's
 * loader into a render/unifont page cache of their own (uf_init_pages with
 * ce_page_check). Each emoji is a palette of RGBA colours and two images of
 * indices into it: 16x16, and 16x8 for 8-pixel rows.
 *
 * The painter draws one through its target's write_lut: the box of two
 * cells as bytes of indices and a table of 0xRRGGBB -- the palette blended
 * over the cell's background, index 255 the background itself (the box
 * beside and around the image). On the Amiga that is cybergraphics'
 * WriteLUTPixelArray, so the target exists only on an RTG screen of 15 bits
 * a pixel or more with cybergraphics.library open (ce_target_ok); anywhere
 * else the colour source answers nothing and Unifont's mono glyph is drawn. */
#ifndef EMOJI_H
#define EMOJI_H
#include "../engine/vtengine.h"
#include "unifont.h"

#define CE_HEADER 76                 /* the page file's header (gen_emoji.py) */
#define CE_PAGE_MAX 0x40000L         /* the largest page file read (gen_emoji.py PAGE_MAX) */
#define CE_SLOTS 4                   /* colour pages a window keeps: the largest is ~106 KB */
#define CE_W 16                      /* the images' width, both sizes */
#define CE_BOX_MAX 2048              /* the box of two cells, in pixels (32 x 32 cells) */
#define CE_BG 255                    /* the index of the background in a box */
#define CE_DIR "UP-Term:emoji/"  /* where the kit installs the pages */

/* The length the header of a page file names (its first n bytes, at least
 * CE_HEADER), for a loader that reads the header first; -1 when it is no
 * page header or the length is past CE_PAGE_MAX. */
long ce_page_length(const vt_u8 *p, long n);
/* 1 when p (len bytes) is a good page file for `page` (a uf_check_fn). */
int ce_page_check(const vt_u8 *p, long len, int page);
/* Code point lo's glyph in a checked page, 0 when the page has none. */
const vt_u8 *ce_page_glyph(const vt_u8 *p, int lo);

/* The palette of n RGBA entries blended over bg (0xRRGGBB): ctab[0..n-1]
 * each entry shown over the background, rounded to nearest, ctab[n..255]
 * the background. ctab has 256 entries. */
void ce_blend(const vt_u8 *rgba, int n, vt_u32 bg, vt_u32 *ctab);

/* Glyph g's image for a box of bw x bh pixels into idx (bw * bh bytes, row
 * after row): the 16x16 image when bh >= 16, else the 16x8 when bh >= 8,
 * centred, CE_BG around it. 0 when it does not fit (bw < 16, bh < 8, or
 * more than CE_BOX_MAX pixels). */
int ce_image(const vt_u8 *g, int bw, int bh, vt_u8 *idx);

/* Where the colour glyphs are drawn: the screen's kind, and the call that
 * puts w x h bytes of indices (idx, w a row) at pixel px, py through ctab
 * (0xRRGGBB). write_lut 0: no such call (no cybergraphics). */
typedef struct ce_target {
    int rtg;     /* not a planar (OCS/ECS/AGA) bitmap */
    int depth;   /* bits a pixel */
    void (*write_lut)(void *user, const vt_u8 *idx, int w, int h, const vt_u32 *ctab, int px, int py);
    void *user;
} ce_target;

/* 1 when colour can be drawn there: RTG, 15 bits or more, write_lut. */
int ce_target_ok(const ce_target *t);

/* What a window that draws colour emoji keeps: the page cache and the
 * box's scratch (~3.3 KB, so a window allocates it only on a screen that
 * can show colour; the renderer holds a pointer). */
typedef struct ce_store {
    uf_cache pages;     /* uf_init_pages(..., ce_page_check, CE_SLOTS) */
    vt_u8 idx[CE_BOX_MAX];
    vt_u32 ctab[256];
} ce_store;

typedef struct ce_painter {
    ce_target tg;
    ce_store *store;    /* the colour pages and scratch, 0 for none (the owner keeps it) */
    int cw, ch;         /* the window font's cell */
    const vt_u8 *cur;   /* the glyph the last ce_has found */
    unsigned long drawn; /* emoji drawn (statistics; the tests' sentinel) */
} ce_painter;

/* A painter on no target and no store: ce_has answers 0. */
void ce_init(ce_painter *p);
/* The window font's cell: boxes are 2 * cw by ch from now on. */
void ce_set_cell(ce_painter *p, int cw, int ch);

/* 1 when cp over `cells` cells draws in colour: two cells, a target that
 * can (ce_target_ok), a box the images fit, and the page has cp -- which is
 * kept for ce_paint. The colour source of a vt_fallback (glyphmap.h). */
int ce_has(void *painter, vt_u32 cp, int cells);
/* The glyph ce_has found, its box at pixel px, py over the background bg
 * (0xRRGGBB): one write_lut call. 0 when there is none to draw. */
int ce_paint(ce_painter *p, int px, int py, vt_u32 bg);

#endif
