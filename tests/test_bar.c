/* The Amarchy bar core (amarchy/bar_core.c; roadmap phase 2 R2.4) and the geometry of the agents panel that hangs
 * from it (amarchy/menu_geom.c anchored layout). The bar's drawing and IDCMP loop (amarchy/bar.c) are the rig
 * script's (tools/rig/amarchy_bar_rig.py). */
#include <string.h>
#include "../amarchy/bar_core.h"
#include "../amarchy/menu_geom.h"
#include "harness.h"

static void test_layout_groups(void)
{
    bar b;
    int a, c, r1, r2;
    bar_init(&b, 640, 4, 2);
    a = bar_add(&b, 100, BAR_LEFT, "ws", 16);
    c = bar_add(&b, 101, BAR_LEFT, "clock", 40);
    r1 = bar_add(&b, 102, BAR_RIGHT, "ram", 24);
    r2 = bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(b.seg[a].x, 0);
    CHECK_INT(b.seg[a].w, 24);
    CHECK_INT(b.seg[c].x, 26);                          /* after the first plus the gap */
    CHECK_INT(b.seg[r2].x + b.seg[r2].w, 640);           /* the last right segment ends at the edge */
    CHECK_INT(b.seg[r2].w, 56);
    CHECK_INT(b.seg[r1].x + b.seg[r1].w, b.seg[r2].x - 2);
    /* a screen of another width moves only the right group */
    bar_layout(&b, 320);
    CHECK_INT(b.seg[a].x, 0);
    CHECK_INT(b.seg[r2].x + b.seg[r2].w, 320);
}

static void test_hit(void)
{
    bar b;
    int r;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, 100, BAR_LEFT, "ws", 16);
    r = bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(bar_hit(&b, 0), 0);
    CHECK_INT(bar_hit(&b, 23), 0);
    CHECK_INT(bar_hit(&b, 24), -1);                      /* past the segment: the gap */
    CHECK_INT(bar_hit(&b, 300), -1);
    CHECK_INT(bar_hit(&b, b.seg[r].x - 1), -1);
    CHECK_INT(bar_hit(&b, b.seg[r].x), r);
    CHECK_INT(bar_hit(&b, 639), r);
    CHECK_INT(bar_hit(&b, 640), -1);
    CHECK_INT(bar_hit(&b, -1), -1);
}

static void test_click_dispatch(void)
{
    bar b;
    int id = -1;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, 100, BAR_LEFT, "ws", 16);
    bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(bar_click(&b, 630, BAR_BTN_LEFT, &id), BAR_ACT_PANEL);
    CHECK_INT(id, BAR_SEG_AGENTS);
    CHECK_INT(bar_click(&b, 630, BAR_BTN_RIGHT, &id), BAR_ACT_LAUNCH_AGENT);
    CHECK_INT(bar_click(&b, 630, 9, &id), BAR_ACT_NONE);  /* another button */
    CHECK_INT(bar_click(&b, 5, BAR_BTN_LEFT, &id), BAR_ACT_NONE);   /* a segment with no action yet */
    CHECK_INT(id, 100);
    CHECK_INT(bar_click(&b, 300, BAR_BTN_LEFT, &id), BAR_ACT_NONE);
    CHECK_INT(id, 0);
}

static void test_dirty_only_changed(void)
{
    bar b;
    int i, drawn = 0;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, 100, BAR_LEFT, "ws", 16);
    bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    for (i = bar_take_dirty(&b, 0); i >= 0; i = bar_take_dirty(&b, i + 1))
        drawn++;
    CHECK_INT(drawn, 2);                                 /* first paint: everything */
    CHECK_INT(bar_take_dirty(&b, 0), -1);
    CHECK_INT(bar_set_text(&b, 100, "ws", 16), 0);       /* same value: no redraw */
    bar_layout(&b, 640);                                 /* same width: nothing moved */
    CHECK_INT(bar_take_dirty(&b, 0), -1);
    CHECK_INT(bar_set_text(&b, 100, "ws2", 24), 1);
    CHECK_INT(bar_take_dirty(&b, 0), 0);
    CHECK_INT(bar_take_dirty(&b, 0), -1);
    CHECK_INT(bar_set_text(&b, 999, "x", 8), 0);         /* unknown id */
}

/* The panel opens upward from the bar's right end: right edge on the screen's, bottom edge on the bar's top. */
static void test_panel_anchor(void)
{
    mg_font f = { 8, 8 };
    mg_screen s = { 640, 256, 22, 44, 0, 0, 0, 0 };
    mg_geom g;
    int ax, ay;
    bar_panel_anchor(640, 246, &ax, &ay);
    CHECK_INT(ax, 640);
    CHECK_INT(ay, 246);
    s.anchored = 1;
    s.ax = ax;
    s.ay = ay;
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.x + g.w, 640);
    CHECK_INT(g.y + g.h, 246);
    CHECK(g.y >= 0 && g.y < 246);
    /* unanchored stays centred */
    s.anchored = 0;
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.x, (640 - 120) / 2);
}

static void test_panel_anchor_clamped(void)
{
    mg_font f = { 8, 8 };
    mg_screen s = { 640, 256, 22, 44, 0, 1, 640, 246 };
    mg_geom g;
    mg_layout(&g, &f, 120, 40, &s);                      /* more rows than fit */
    CHECK(g.y >= 0);
    CHECK(g.y + g.h <= 246);
    s.ay = 20;                                           /* the bar's top is above the menu's height */
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.y, 0);
    s.ax = 50;                                           /* the right end is nearer the left edge than the width */
    s.ay = 246;
    mg_layout(&g, &f, 200, 6, &s);
    CHECK_INT(g.x, 0);
    s.ax = 900;                                          /* beyond the screen: the edge */
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.x + g.w, 640);
}

void suite_bar(void)
{
    test_layout_groups();
    test_hit();
    test_click_dispatch();
    test_dirty_only_changed();
    test_panel_anchor();
    test_panel_anchor_clamped();
}
