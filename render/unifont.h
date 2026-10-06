/* unifont: GNU Unifont's glyphs for the code points the window's font
 * lacks and has no stand-in for -- the BMP and plane 1's emoji blocks
 * (ledger U2, W33; thoughts/shared/plans/2026-10-04-u2-unifont.md).
 * Portable C89, host-tested.
 *
 * The glyphs come as one file per 256-code-point page (tools/gen_unifont.py
 * writes them; its docstring has the layout), read on first use through the
 * owner's loader and kept in a small LRU cache. A glyph is handed out as a
 * mask in the owner's buffer, the shape render/outline's masks have: ch rows
 * of bpr bytes (bpr even, for the blitter), cells * cw pixels used, set bits
 * the ink. Unifont is 8x16 (16x16 wide, an emoji over two cells): a 16-pixel
 * font gets it 1:1, an 8-pixel one (topaz 8) each pair of rows OR'd into
 * one; other heights get nothing (the caller draws the replacement). */
#ifndef UNIFONT_H
#define UNIFONT_H
#include "../engine/vtengine.h"

#define UF_HEADER 88           /* the page file's header (gen_unifont.py) */
#define UF_PAGES 0x200         /* pages 0x00-0xFF the BMP, 0x100-0x1FF plane 1 (code point >> 8) */
#define UF_PAGE_MAX (UF_HEADER + 256 * 32) /* a page of wide glyphs: 8280 bytes */
#define UF_SLOTS 8             /* pages a cache holds: at most 8 x 8280 bytes */
#define UF_MASK_MAX 128        /* the mask buffer: two cells of 32 pixels, 16 rows */
#define UF_DIR "UP-Term:unifont/" /* where the kit installs the pages */

/* The loader: page `page`'s file into a buffer it allocates (*buf), its
 * length; -1 when there is no such file (remembered: not asked again),
 * -2 when it could not be read now (no memory: asked again later). */
typedef long (*uf_load_fn)(void *user, int page, vt_u8 **buf);
/* A buffer the loader gave back to it. */
typedef void (*uf_release_fn)(void *user, vt_u8 *buf);

typedef struct uf_slot {
    vt_u8 *buf;      /* 0: empty */
    long len;
    int page;
    unsigned long used; /* the cache's clock at the last lookup: the smallest goes first */
} uf_slot;

/* 1 when p (len bytes) is a good page file for `page` (uf_page_check for
 * Unifont's pages; render/emoji's ce_page_check for the colour ones). */
typedef int (*uf_check_fn)(const vt_u8 *p, long len, int page);

/* A cache of page files read through the owner's loader: Unifont's (U2) and
 * the colour emoji's (E1) are each one, of their own check and size. */
typedef struct uf_cache {
    uf_load_fn load;
    uf_release_fn release;
    void *user;
    uf_check_fn check;
    int nslots;      /* slots in use, at most UF_SLOTS: the cache's RAM cap */
    vt_u8 *mask;     /* the owner's buffer the glyphs are made in (chip RAM on the Amiga) */
    int mask_size;
    int cw, ch;      /* the window font's cell */
    uf_slot slot[UF_SLOTS];
    unsigned long clock;
    vt_u8 absent[UF_PAGES / 8]; /* pages with no file (or a bad one): bit 0x80 >> (page & 7) of byte page >> 3 */
    unsigned long loads; /* page files read, good or not (statistics) */
} uf_cache;

/* An empty cache on the owner's loader and mask buffer; no file is read yet. */
void uf_init(uf_cache *c, uf_load_fn load, uf_release_fn release, void *user, vt_u8 *mask, int mask_size);
/* An empty cache of other pages (no mask): `check` says a file is good, at
 * most `nslots` pages (1 to UF_SLOTS) are kept. */
void uf_init_pages(uf_cache *c, uf_load_fn load, uf_release_fn release, void *user, uf_check_fn check, int nslots);
/* Page `page`'s file (*len its bytes), read now when it is not kept, the
 * least recently used given up for it; 0 when there is no good file. */
const vt_u8 *uf_page(uf_cache *c, int page, long *len);
/* Every page given back to the loader; the cache is empty and stays usable. */
void uf_flush(uf_cache *c);
/* The window font's cell: glyphs are made for it from now on. */
void uf_set_cell(uf_cache *c, int cw, int ch);

/* 1 when p (len bytes) is a good page file for `page`. */
int uf_page_check(const vt_u8 *p, long len, int page);
/* Code point lo's glyph in a checked page: 16 bytes (8x16) or, *wide set,
 * 32 (16x16, two bytes a row); 0 when the page has none. */
const vt_u8 *uf_page_glyph(const vt_u8 *p, int lo, int *wide);
/* A glyph as a mask for `cells` cells of cw x ch: into mask, ch rows of
 * bpr bytes, cleared first; Unifont's rows 1:1 at 16, pairs OR'd at 8,
 * whatever baseline the font declares. 0 when ch is neither 16 nor 8 or
 * the mask does not fit in size bytes. */
int uf_make_mask(const vt_u8 *g, int wide, int cw, int ch, int cells, vt_u8 *mask, int bpr, int size);

/* cp's glyph over `cells` cells (1, or 2 for a wide cell), in the cache's
 * mask buffer (valid until the next call), *bpr its bytes a row; 0 when
 * there is none (past plane 1, no page, no glyph, a cell size Unifont
 * cannot be drawn at). The signature of a vt_mask_source (glyphmap.h). */
const vt_u8 *uf_glyph(void *cache, vt_u32 cp, int cells, int *bpr);

/* The page's file name into out[4]: two upper-case hex digits in the BMP
 * ("4E"), three in plane 1 ("1F6"), and a 0. */
void uf_page_name(int page, char *out);

#endif
