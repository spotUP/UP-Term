/* TE-10.6c: the role pens of a themed foreign screen. See flat_role_core.h. */
#include "flat_role_core.h"

/* The Workbench rule is desk_flat_state / desk_flat_glyph_pens (desk_core.c): outline = accent, button = selection,
 * ptext = surface, dimtext = muted, text = text, line = ACCENT (not the muted border group), back = surface. The fallback
 * column repeats it, so a theme with no per-element overrides resolves to the same colours as the Workbench pens. */
static const flat_role_row g_rows[FLAT_ROLE_COUNT] = {
    { FLAT_ROLE_OUTLINE, GTE_BORDER, GR_ACCENT, FLAT_RULE_NEAREST, -1 },
    { FLAT_ROLE_BUTTON, GTE_BUTTON_FACE, GR_SELECTION, FLAT_RULE_NEAREST, -1 },
    { FLAT_ROLE_PTEXT, GTE_BUTTON_TEXT, GR_SURFACE, FLAT_RULE_NEAREST, -1 },
    { FLAT_ROLE_DIMTEXT, GTE_GADGET_GLYPH_INACTIVE, GR_MUTED, FLAT_RULE_NEAREST, -1 },
    { FLAT_ROLE_TEXT, GTE_GADGET_GLYPH, GR_TEXT, FLAT_RULE_DRI, FLAT_ROLE_DRI_TEXT },
    { FLAT_ROLE_LINE, GTE_BORDER, GR_ACCENT, FLAT_RULE_DRI, FLAT_ROLE_DRI_SHADOW },
    { FLAT_ROLE_BACK, GTE_REQUESTER_BACKGROUND, GR_SURFACE, FLAT_RULE_DRI, FLAT_ROLE_DRI_BACKGROUND },
};

const flat_role_row *flat_role_row_of(int i)
{
    return i >= 0 && i < FLAT_ROLE_COUNT ? &g_rows[i] : 0;
}

void flat_role_rgb_of(const gtheme *t, uc_u32 *rgb)
{
    gt_roles r;
    int i;
    gt_roles_of(t, &r);
    for (i = 0; i < FLAT_ROLE_COUNT; i++)
        rgb[i] = gt_elem_rgb(t, g_rows[i].elem, r.c[g_rows[i].fallback]) & 0xFFFFFFUL;
}

void flat_role_resolve(const uc_u32 *rgb, const short *held, const uc_u32 *hrgb, int nheld, const uc_u32 *cmap, int ncolours,
                       const short *dri, flat_role_pens *out)
{
    int i, j;
    out->ok = ncolours > 0 ? 1 : 0;
    for (i = 0; i < FLAT_ROLE_COUNT; i++) {
        uc_u32 c = rgb[i] & 0xFFFFFFUL;
        int hit = -1, best = 0;
        for (j = 0; j < nheld && hit < 0; j++)
            if ((hrgb[j] & 0xFFFFFFUL) == c)
                hit = held[j];
        for (j = 0; j < ncolours && hit < 0; j++)
            if ((cmap[j] & 0xFFFFFFUL) == c)
                hit = j;
        if (hit < 0 && g_rows[i].rule == FLAT_RULE_DRI)
            hit = dri ? dri[g_rows[i].dri] : 0;
        else if (hit < 0) {
            for (j = 1; j < ncolours; j++)      /* strict <: a tie keeps the lower pen, as gt_nearest_role_pen */
                if (gt_rgb_dist2(c, cmap[j]) < gt_rgb_dist2(c, cmap[best]))
                    best = j;
            hit = best;
        }
        out->pen[i] = (short)hit;
    }
}

void flat_role_apply(flat_state *st, const flat_role_pens *p)
{
#define PEN8(i) ((uint8_t)(p->pen[i] >= 0 && p->pen[i] <= 255 ? p->pen[i] : 0))
    st->pen_outline = PEN8(FLAT_ROLE_OUTLINE);
    st->pen_button = PEN8(FLAT_ROLE_BUTTON);
    st->pen_ptext = PEN8(FLAT_ROLE_PTEXT);
    st->pen_dimtext = PEN8(FLAT_ROLE_DIMTEXT);
    st->pen_text = PEN8(FLAT_ROLE_TEXT);
    st->pen_line = PEN8(FLAT_ROLE_LINE);
    st->pen_back = PEN8(FLAT_ROLE_BACK);
#undef PEN8
}
