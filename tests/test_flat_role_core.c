/* TE-10.6c brief 1: the role pens of a themed foreign screen (amarchy/flat_role_core.c). R1-R6 of
 * thoughts/shared/research/s61-evidence/te10-6c-design.md section 7. */
#include <stdio.h>
#include <string.h>
#include "../amarchy/flat_role_core.h"
#include "../amarchy/desk_core.h"
#include "harness.h"

#define RGB(r, g, b) (((uc_u32)(r) << 16) | ((uc_u32)(g) << 8) | (uc_u32)(b))

/* DrawInfo slots as flat_role_resolve reads them: index by the Intuition pen constants */
static void dri_fill(short *dri)
{
    int i;
    for (i = 0; i < FLAT_RP_DRI_PENS; i++)
        dri[i] = (short)(100 + i);
}

static void rgb_all(uc_u32 *rgb, uc_u32 c)
{
    int i;
    for (i = 0; i < FLAT_RP_COUNT; i++)
        rgb[i] = c;
}

/* R1: a held plan pen with the row's colour wins, over a ColorMap pen with the same colour too */
static void r1_held_exact(void)
{
    uc_u32 rgb[FLAT_RP_COUNT], cmap[8] = { 0x000000, 0x111111, 0x222222, 0x333333, 0x444444, 0x555555, 0x666666, 0xABCDEF };
    short held[2] = { 20, 21 }, dri[FLAT_RP_DRI_PENS];
    uc_u32 hrgb[2] = { 0xABCDEF, 0x123456 };
    flat_role_pens out;
    rgb_all(rgb, 0x999999);
    rgb[FLAT_RP_BUTTON] = 0xABCDEF;
    rgb[FLAT_RP_OUTLINE] = 0x123456;
    dri_fill(dri);
    flat_role_resolve(rgb, held, hrgb, 2, cmap, 8, dri, &out);
    CHECK_INT(out.pen[FLAT_RP_BUTTON], 20);     /* held pen 20, not ColorMap pen 7 */
    CHECK_INT(out.pen[FLAT_RP_OUTLINE], 21);
    CHECK_INT(out.ok, 1);
}

/* R2: no held match: the exact ColorMap pen, even when another pen is closer to nothing else */
static void r2_cmap_exact(void)
{
    uc_u32 rgb[FLAT_RP_COUNT], cmap[8] = { 0x000000, 0x101010, 0x202020, 0x303030, 0x404040, 0x505050, 0x606060, 0x707070 };
    short dri[FLAT_RP_DRI_PENS];
    flat_role_pens out;
    rgb_all(rgb, 0x000000);
    rgb[FLAT_RP_PTEXT] = 0x505050;
    rgb[FLAT_RP_DIMTEXT] = 0x707070;
    rgb[FLAT_RP_TEXT] = 0x303030;               /* a DrawInfo row with an exact match takes the match, not TEXTPEN */
    dri_fill(dri);
    flat_role_resolve(rgb, 0, 0, 0, cmap, 8, dri, &out);
    CHECK_INT(out.pen[FLAT_RP_PTEXT], 5);
    CHECK_INT(out.pen[FLAT_RP_DIMTEXT], 7);
    CHECK_INT(out.pen[FLAT_RP_TEXT], 3);
    /* a held pen is also found when the ColorMap lacks the colour, and exact beats the first of two equal cmap pens */
    cmap[2] = cmap[5] = 0x505050;
    flat_role_resolve(rgb, 0, 0, 0, cmap, 8, dri, &out);
    CHECK_INT(out.pen[FLAT_RP_PTEXT], 2);       /* lowest pen of equal colours */
}

/* R3: nearest by the gthemes distance (squared RGB, ties to the lower pen). Target 0: pen 1 (30,30,30) is nearer by squared
 * distance (2700 against 3600) but farther by a Manhattan sum (90 against 60), so a second formula would pick pen 2. */
static void r3_nearest_gthemes_distance(void)
{
    uc_u32 rgb[FLAT_RP_COUNT], cmap[3] = { 0xFFFFFF, RGB(30, 30, 30), RGB(0, 0, 60) };
    short dri[FLAT_RP_DRI_PENS];
    flat_role_pens out;
    int i, best = 0;
    rgb_all(rgb, RGB(0, 0, 0));
    dri_fill(dri);
    flat_role_resolve(rgb, 0, 0, 0, cmap, 3, dri, &out);
    for (i = 1; i < 3; i++)
        if (gt_rgb_dist2(rgb[0], cmap[i]) < gt_rgb_dist2(rgb[0], cmap[best]))
            best = i;
    CHECK_INT(best, 1);
    CHECK_INT(out.pen[FLAT_RP_OUTLINE], best);
    CHECK_INT(out.pen[FLAT_RP_BUTTON], best);
    CHECK_INT(out.pen[FLAT_RP_PTEXT], best);
    CHECK_INT(out.pen[FLAT_RP_DIMTEXT], best);
    /* tie keeps the lower pen, as gt_nearest_role_pen */
    cmap[1] = RGB(10, 0, 0);
    cmap[2] = RGB(0, 10, 0);
    flat_role_resolve(rgb, 0, 0, 0, cmap, 3, dri, &out);
    CHECK_INT(out.pen[FLAT_RP_OUTLINE], 1);
    CHECK_INT(gt_rgb_dist2(RGB(0, 0, 0), RGB(3, 4, 0)), 25);
}

/* R4: the three DrawInfo rows take TEXTPEN / SHADOWPEN / BACKGROUNDPEN when nothing matches exactly; the nearest rows never do */
static void r4_dri_rows(void)
{
    uc_u32 rgb[FLAT_RP_COUNT], cmap[2] = { 0x000000, 0xFFFFFF };
    short dri[FLAT_RP_DRI_PENS];
    flat_role_pens out;
    rgb_all(rgb, 0x808080);
    dri_fill(dri);
    flat_role_resolve(rgb, 0, 0, 0, cmap, 2, dri, &out);
    CHECK_INT(out.pen[FLAT_RP_TEXT], dri[FLAT_RP_DRI_TEXT]);
    CHECK_INT(out.pen[FLAT_RP_LINE], dri[FLAT_RP_DRI_SHADOW]);
    CHECK_INT(out.pen[FLAT_RP_BACK], dri[FLAT_RP_DRI_BACKGROUND]);
    CHECK(out.pen[FLAT_RP_BUTTON] == 0 || out.pen[FLAT_RP_BUTTON] == 1);   /* nearest of the ColorMap, never "stock" */
    CHECK_INT(FLAT_RP_DRI_TEXT, 2);             /* the Intuition constants, pinned: no OS header here */
    CHECK_INT(FLAT_RP_DRI_SHADOW, 4);
    CHECK_INT(FLAT_RP_DRI_BACKGROUND, 7);
    /* no DrawInfo: the rows stay 0 */
    flat_role_resolve(rgb, 0, 0, 0, cmap, 2, 0, &out);
    CHECK_INT(out.pen[FLAT_RP_TEXT], 0);
}

/* R5: apply writes the seven fields and nothing else */
static void r5_apply(void)
{
    flat_state st, before;
    flat_role_pens p;
    int i;
    memset(&st, 0xA5, sizeof st);
    before = st;
    for (i = 0; i < FLAT_RP_COUNT; i++)
        p.pen[i] = (short)(10 + i);
    p.ok = 1;
    flat_role_apply(&st, &p);
    CHECK_INT(st.pen_outline, 10);
    CHECK_INT(st.pen_button, 11);
    CHECK_INT(st.pen_ptext, 12);
    CHECK_INT(st.pen_dimtext, 13);
    CHECK_INT(st.pen_text, 14);
    CHECK_INT(st.pen_line, 15);
    CHECK_INT(st.pen_back, 16);
    /* every other byte unchanged: restore the seven and compare the whole block */
    st.pen_outline = before.pen_outline; st.pen_button = before.pen_button; st.pen_ptext = before.pen_ptext;
    st.pen_dimtext = before.pen_dimtext; st.pen_text = before.pen_text; st.pen_line = before.pen_line;
    st.pen_back = before.pen_back;
    CHECK(memcmp(&st, &before, sizeof st) == 0);
    /* a pen above 255 is not a pen: refused to 0 rather than wrapped */
    p.pen[0] = 300;
    flat_role_apply(&st, &p);
    CHECK_INT(st.pen_outline, 0);
}

/* R6: Workbench parity. A theme without a model gives rgb[] equal to the role colours; on a ColorMap that is the Workbench
 * palette (pen i = desk_role i) the resolved pens are exactly what desk_flat_state writes. */
static void r6_workbench_parity(void)
{
    gtheme t;
    uc_u32 rgb[FLAT_RP_COUNT], cmap[DK_ROLES];
    short dri[FLAT_RP_DRI_PENS];
    flat_role_pens out;
    flat_state want, got;
    int i, distinct = 1, j;
    gt_builtin(&t);
    for (i = 0; i < DK_ROLES; i++)
        cmap[i] = desk_role(&t, i) & 0xFFFFFFUL;
    for (i = 0; i < 6; i++)                       /* DK_BG..DK_DARKER are the roles the table reads */
        for (j = 0; j < i; j++)
            if (cmap[i] == cmap[j])
                distinct = 0;
    CHECK(distinct);
    dri_fill(dri);
    flat_role_rgb_of(&t, rgb);
    flat_role_resolve(rgb, 0, 0, 0, cmap, DK_ROLES, dri, &out);
    memset(&want, 0, sizeof want);
    memset(&got, 0, sizeof got);
    desk_flat_state(1, 1, &want);
    flat_role_apply(&got, &out);
    CHECK_INT(got.pen_outline, want.pen_outline);
    CHECK_INT(got.pen_button, want.pen_button);
    CHECK_INT(got.pen_ptext, want.pen_ptext);
    CHECK_INT(got.pen_dimtext, want.pen_dimtext);
    CHECK_INT(got.pen_text, want.pen_text);
    CHECK_INT(got.pen_line, want.pen_line);       /* the Workbench rule: line = GR_ACCENT (desk_flat_glyph_pens), not the border element's own group */
    CHECK_INT(got.pen_back, want.pen_back);
    /* the table is the one source: seven rows, in the field order of the enum */
    for (i = 0; i < FLAT_RP_COUNT; i++)
        CHECK_INT(flat_role_row_of(i)->field, i);
    CHECK(flat_role_row_of(FLAT_RP_COUNT) == 0);
    CHECK_INT(flat_role_row_of(FLAT_RP_LINE)->fallback, GR_ACCENT);
}

/* TE-10.6c brief 3: the draw table slot carries the themed flag (enabled), the role pens and the side border of its screen.
 * Lives here, not in test_flat_core.c: flat_glyph.h (included there) kept FLAT_ROLE_BACK before the role pens became FLAT_RP_*. */
static flat_draw_tab g_dt;

static void roles_fill(flat_role_pens *r, int base, uint8_t ok)
{
    int i;
    for (i = 0; i < FLAT_RP_COUNT; i++) r->pen[i] = (short)(base + i);
    r->ok = ok;
}

static int roles_equal(const flat_state *a, const flat_state *b)
{
    return a->pen_outline == b->pen_outline && a->pen_button == b->pen_button && a->pen_ptext == b->pen_ptext &&
           a->pen_dimtext == b->pen_dimtext && a->pen_text == b->pen_text && a->pen_line == b->pen_line && a->pen_back == b->pen_back;
}

static void test_draw_slot_themed(void)
{
    flat_state look, live, livecopy, want, base;
    flat_pens_tab t;
    flat_pens *hp;
    flat_draw_scr l[1];
    flat_role_pens ra, rb, rbad;
    int wb, sa, un;
    const flat_state *p, *p1;
    desk_flat_state(1, 1, &look);
    look.enabled = 1; look.side = 4;
    flat_pens_tab_init(&t);
    hp = flat_pens_tab_for(&t, &sa, 1);
    hp->screen = &sa; hp->epoch = 1; hp->dim = 77; hp->knob = 78; hp->inbar = 79; hp->sel = 80;
    live = look; live.pen_dim = 5;
    livecopy = live;
    roles_fill(&ra, 20, 1); roles_fill(&rb, 90, 1); roles_fill(&rbad, 50, 0);
    flat_draw_init(&g_dt);
    l[0].screen = &sa; l[0].epoch = 1; l[0].roles = 0; l[0].side = 7;
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    p = flat_draw_state(&g_dt, &live, &sa);                              /* D1a: listed, not themed */
    flat_state_for(&look, &t, &sa, 1, &want);
    want.enabled = 0; want.side = 7;
    CHECK_INT(memcmp(p, &want, sizeof want), 0);
    CHECK_INT(p->enabled, 0);
    CHECK_INT(g_dt.plain.enabled, 0);                                    /* D1b */
    CHECK(flat_draw_state(&g_dt, &live, &un) == &g_dt.plain);
    CHECK_INT(flat_draw_state(&g_dt, &live, &un)->enabled, 0);
    l[0].roles = &ra;                                                    /* D1c */
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    p1 = p = flat_draw_state(&g_dt, &live, &sa);
    CHECK_INT(p->enabled, look.enabled);
    flat_state_for(&look, &t, &sa, 1, &base);
    want = base; flat_role_apply(&want, &ra);
    CHECK(roles_equal(p, &want));
    CHECK_INT(p->pen_dim, 77); CHECK_INT(p->pen_knob, 78); CHECK_INT(p->pen_inbar, 79); CHECK_INT(p->pen_sel, 80);
    CHECK_INT(p->pen_outline, 20);
    look.enabled = 0;                                                    /* D1d */
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    CHECK_INT(flat_draw_state(&g_dt, &live, &sa)->enabled, 0);
    look.enabled = 1;
    CHECK(flat_draw_state(&g_dt, &live, 0) == &live);                    /* D1e */
    CHECK(flat_draw_state(&g_dt, &live, &wb) == &live);
    CHECK_INT(memcmp(&live, &livecopy, sizeof live), 0);
    l[0].roles = &rb;                                                    /* D2 */
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    p = flat_draw_state(&g_dt, &live, &sa);
    CHECK(p == p1);
    CHECK_INT(p->pen_outline, 90); CHECK_INT(p->enabled, 1);
    l[0].roles = 0;
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    CHECK(flat_draw_state(&g_dt, &live, &sa) == p1);
    CHECK_INT(p1->enabled, 0);
    l[0].roles = &rbad;                                                  /* ok = 0 behaves as not themed */
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    CHECK_INT(p1->enabled, 0);
    l[0].roles = &ra; l[0].side = 3;                                     /* D3 */
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    CHECK_INT(p1->side, 3);
    l[0].side = 9;
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    CHECK_INT(p1->side, 9);
    CHECK_INT(g_dt.plain.side, look.side);
}

void suite_flat_role_core(void)
{
    test_draw_slot_themed();
    r1_held_exact();
    r2_cmap_exact();
    r3_nearest_gthemes_distance();
    r4_dri_rows();
    r5_apply();
    r6_workbench_parity();
}
