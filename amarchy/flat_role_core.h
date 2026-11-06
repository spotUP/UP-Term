/* TE-10.6c: the role pens of a themed foreign screen (design thoughts/shared/research/s61-evidence/te10-6c-design.md section 2).
 * amarchy-flat draws with seven pen numbers of the flat_state block. On the Workbench they are the fixed Amarchy palette
 * pens (desk_flat_state); on another screen with a theme the pen that carries a role's colour has to be found. Per row:
 *   1. an exact rgb match on a held plan pen of the screen's slot;
 *   2. an exact rgb match in the screen's ColorMap (lowest pen of equal colours);
 *   3. no exact pen: the nearest ColorMap pen by gt_rgb_dist2 (the gthemes distance), ties to the lower pen; the three
 *      DrawInfo rows (text, line, back) take TEXTPEN / SHADOWPEN / BACKGROUNDPEN of the screen instead.
 * Owner decision 2026-11-06: no exact pen means the nearest pen, never "stock". Pure: no OS call, no allocation, no globals. */
#ifndef AMARCHY_FLAT_ROLE_CORE_H
#define AMARCHY_FLAT_ROLE_CORE_H

#include <stdint.h>
#include "flat_core.h"
#include "gthemes_core.h"

/* the seven rows, the order of flat_role_pens.pen[] and of the table */
enum { FLAT_ROLE_OUTLINE, FLAT_ROLE_BUTTON, FLAT_ROLE_PTEXT, FLAT_ROLE_DIMTEXT, FLAT_ROLE_TEXT, FLAT_ROLE_LINE, FLAT_ROLE_BACK,
       FLAT_ROLE_COUNT };

/* what a row takes when no pen carries its colour */
enum { FLAT_RULE_NEAREST, FLAT_RULE_DRI };

/* DrawInfo pen indices (Intuition TEXTPEN, SHADOWPEN, BACKGROUNDPEN), repeated here so the module needs no OS header */
enum { FLAT_ROLE_DRI_TEXT = 2, FLAT_ROLE_DRI_SHADOW = 4, FLAT_ROLE_DRI_BACKGROUND = 7, FLAT_ROLE_DRI_PENS = 8 };

typedef struct flat_role_row {
    int field;      /* FLAT_ROLE_*: the flat_state field this row writes */
    int elem;       /* GTE_*: the theme element whose colour the row paints */
    int fallback;   /* GR_*: the role colour gt_elem_rgb gives when the theme leaves the element unset (= the Workbench rule) */
    int rule;       /* FLAT_RULE_* */
    int dri;        /* FLAT_RULE_DRI: the DrawInfo pen index, else -1 */
} flat_role_row;

typedef struct flat_role_pens {
    short pen[FLAT_ROLE_COUNT];
    uint8_t ok;     /* 1 = the ColorMap was given, so every row was chosen from colours; 0 = no ColorMap, rows without a held match are 0 */
} flat_role_pens;

/* row i of the one mapping table, 0 when i is out of range */
const flat_role_row *flat_role_row_of(int i);

/* rgb[FLAT_ROLE_COUNT] = the theme colour of every row: gt_elem_rgb(t, row.elem, role colour row.fallback) */
void flat_role_rgb_of(const gtheme *t, uc_u32 *rgb);

/* rgb[] = theme colour per row, held[]/hrgb[] = the slot's plan pens (nheld pairs, may be 0), cmap[] = the screen's colours
 * (ncolours entries, 0xRRGGBB), dri[] = the screen's DrawInfo pens (FLAT_ROLE_DRI_PENS entries, may be 0). */
void flat_role_resolve(const uc_u32 *rgb, const short *held, const uc_u32 *hrgb, int nheld, const uc_u32 *cmap, int ncolours,
                       const short *dri, flat_role_pens *out);

/* writes the seven flat_state pen fields, nothing else; a pen outside 0..255 becomes 0 */
void flat_role_apply(flat_state *st, const flat_role_pens *p);

#endif
