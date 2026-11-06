/* TE-5 (amarchy/themeprev_core.c, plan 2026-11-05-amarchy-theme-editor.md): the preview pane's layout boxes for stock and thin
 * bar heights, radius 0 and 6, the gadget order, the colours from gt_resolve, the placement rule (D4), and the wiring in
 * amarchy/gui.c (which cannot compile on the host) checked in its source text. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "../amarchy/themeprev_core.h"
#include "../amarchy/pen_cache.h"
#include "harness.h"

#define THEME_DIR "dist/amarchy-themes"

static int load_model(const char *name, gt_model *m)
{
    static char buf[UC_MAX_FILE];
    char path[200];
    FILE *f;
    long n;
    snprintf(path, sizeof path, THEME_DIR "/%s.theme", name);
    f = fopen(path, "rb");
    if (!f)
        return 0;
    n = (long)fread(buf, 1, sizeof buf, f);
    fclose(f);
    memset(m, 0, sizeof *m);
    gt_model_parse(buf, n, m);
    return n > 0;
}

static int inside(const themeprev *p, const tpv_item *t)
{
    return t->x >= 0 && t->y >= 0 && t->x + t->w <= p->w && t->y + t->h <= p->h;
}

static void test_themeprev_stock_and_thin(void)
{
    gt_look l;
    themeprev p, q;
    int i;
    memset(&l, 0, sizeof l);
    themeprev_layout(&l, TPV_WIDTH, 220, &p);            /* 0 = the stock look: bar 12, border 2, scroll 10 */
    CHECK_INT(p.it[TPV_ACT_BAR].h, TPV_STOCK_BAR);
    CHECK_INT(p.it[TPV_ACT_FRAME].x + 2, p.it[TPV_ACT_BAR].x);
    CHECK_INT(p.it[TPV_ACT_SCROLL].w, TPV_STOCK_SCROLL);
    CHECK_INT(p.it[TPV_ACT_GADGET0].w, TPV_STOCK_BAR - 2);
    for (i = 0; i < TPV_COUNT; i++)
        CHECK(inside(&p, &p.it[i]));
    l.bar_height = 12;                                   /* an explicit 12 equals the stock layout */
    themeprev_layout(&l, TPV_WIDTH, 220, &q);
    CHECK(!memcmp(&p.it[TPV_ACT_BAR], &q.it[TPV_ACT_BAR], offsetof(tpv_item, label)));
    l.bar_height = 8;                                    /* thin bar: bars shrink, the body moves up */
    themeprev_layout(&l, TPV_WIDTH, 220, &q);
    CHECK_INT(q.it[TPV_ACT_BAR].h, 8);
    CHECK_INT(q.it[TPV_INA_BAR].h, 8);
    CHECK(q.it[TPV_ACT_BODY].y < p.it[TPV_ACT_BODY].y);
    CHECK(q.it[TPV_ACT_GADGET0].h <= 8);
    l.bar_height = 20;                                   /* tall bar: gadgets grow with it */
    themeprev_layout(&l, TPV_WIDTH, 220, &q);
    CHECK_INT(q.it[TPV_ACT_BAR].h, 20);
    CHECK_INT(q.it[TPV_ACT_GADGET0].w, 18);
}

static void test_themeprev_radius_and_order(void)
{
    gt_look l;
    themeprev p, q;
    memset(&l, 0, sizeof l);
    themeprev_layout(&l, TPV_WIDTH, 220, &p);
    CHECK_INT(p.it[TPV_ACT_FRAME].radius, 0);
    CHECK_INT(p.it[TPV_MENU].radius, 0);
    CHECK_INT(p.it[TPV_BUTTON].radius, 0);
    l.corner_radius = 6;
    themeprev_layout(&l, TPV_WIDTH, 220, &q);
    CHECK_INT(q.it[TPV_ACT_FRAME].radius, 6);
    CHECK_INT(q.it[TPV_INA_FRAME].radius, 6);
    CHECK_INT(q.it[TPV_MENU].radius, 6);
    CHECK_INT(q.it[TPV_MENU_SEL].radius, 6);
    CHECK_INT(q.it[TPV_BUTTON].radius, 6);
    CHECK_INT(q.it[TPV_ACT_TITLE].radius, 0);            /* texts are never rounded */
    CHECK(q.it[TPV_ACT_GADGET0].radius <= 6);
    CHECK_INT(q.it[TPV_FIELD].radius, 0);
    /* gadget order 0: the gadgets sit at the bar's left end, 1: at its right end */
    CHECK(p.it[TPV_ACT_GADGET0].x < p.it[TPV_ACT_TITLE].x);
    l.gadget_order = 1;
    themeprev_layout(&l, TPV_WIDTH, 220, &q);
    CHECK(q.it[TPV_ACT_GADGET0].x > q.it[TPV_ACT_TITLE].x);
    CHECK(q.it[TPV_ACT_GADGET2].x + q.it[TPV_ACT_GADGET2].w <= q.it[TPV_ACT_BAR].x + q.it[TPV_ACT_BAR].w);
}

static void test_themeprev_small_pane(void)
{
    gt_look l;
    themeprev p;
    int i, hidden = 0;
    memset(&l, 0, sizeof l);
    themeprev_layout(&l, 90, 60, &p);                    /* far too small: boxes that do not fit are not drawn, none spill */
    for (i = 0; i < TPV_COUNT; i++) {
        if (p.it[i].w == 0 || p.it[i].h == 0)
            hidden++;
        else
            CHECK(inside(&p, &p.it[i]));
    }
    CHECK(hidden > 0);
    CHECK(p.it[TPV_DESKTOP].w == 90 && p.it[TPV_DESKTOP].h == 60);
}

static void test_themeprev_colours(void)
{
    gt_model m;
    gt_look l;
    themeprev p;
    int i;
    CHECK(load_model("gruvbox", &m));
    memset(&l, 0, sizeof l);
    themeprev_layout(&l, TPV_WIDTH, 220, &p);
    for (i = 0; i < TPV_COUNT; i++)
        CHECK_INT(themeprev_rgb(&m, &p.it[i]), gt_resolve_rgb(&m, p.it[i].elem, p.it[i].state));
    CHECK_INT(p.it[TPV_ACT_BAR].elem, GTE_TITLE_BAR);
    CHECK_INT(p.it[TPV_INA_BAR].elem, GTE_TITLE_BAR_INACTIVE);
    CHECK_INT(p.it[TPV_MENU_DISABLED].state, GT_ST_DISABLED);
    /* a second theme gives other colours: the preview follows the edited model, not a fixed palette */
    {
        gt_model n;
        CHECK(load_model("nord", &n));
        CHECK(themeprev_rgb(&m, &p.it[TPV_DESKTOP]) != themeprev_rgb(&n, &p.it[TPV_DESKTOP]));
    }
}

static void test_themeprev_placement(void)
{
    CHECK_INT(themeprev_placement(1024, 300), TPV_BESIDE);
    CHECK_INT(themeprev_placement(640, 300), TPV_BESIDE);
    CHECK_INT(themeprev_placement(320, 300), TPV_TOGGLE);
    CHECK_INT(themeprev_placement(300 + TPV_WIDTH + 4 * TPV_MARGIN, 300), TPV_BESIDE);
    CHECK_INT(themeprev_placement(300 + TPV_WIDTH + 4 * TPV_MARGIN - 1, 300), TPV_TOGGLE);
}

/* Reachability (TE-5.2): gui.c cannot be built on the host, so the chain from the editor's draw to the sentinel is checked in
 * its source: draw_list -> draw_preview -> theme_preview_view -> themeprev_layout/themeprev_rgb, the sentinel bumped and
 * published, the pane shown only on a Theme Editor page with the model open, Tab toggling the full view. The rig proof is the
 * AMARCHY_PREVIEW_DRAWS global. */
static char *slurp(const char *path)
{
    static char buf[400000];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f)
        return 0;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

static const char *fn_body(const char *src, const char *head)
{
    const char *p = strstr(src, head);
    return p ? p : "";
}

static void test_themeprev_reaches_gui(void)
{
    char *src = slurp("amarchy/gui.c");
    const char *b;
    CHECK(src != 0);
    if (!src)
        return;
    b = fn_body(src, "static void theme_preview_view(gui *g, int x, int y, int w, int h)\n{");
    CHECK(*b != 0);
    CHECK(strstr(b, "themeprev_layout(&gt_ed.m.look") != 0);
    CHECK(strstr(b, "themeprev_rgb(&gt_ed.m") != 0);
    CHECK(strstr(b, "gui_preview_draws++") != 0);
    CHECK(strstr(b, "sentinel_publish(\"AMARCHY_PREVIEW_DRAWS\"") != 0);
    CHECK(strstr(b, "rich_corners(") != 0);                 /* the look's corner radius reaches the drawing */
    b = fn_body(src, "static void draw_preview(gui *g)\n{");
    CHECK(*b != 0);
    CHECK(strstr(b, "theme_preview_view(g,") != 0);
    b = fn_body(src, "static void draw_list(gui *g)");
    CHECK(*b != 0);
    CHECK(strstr(b, "draw_preview(g);") != 0);
    b = fn_body(src, "static int preview_active(const gui *g)\n{");
    CHECK(strstr(b, "lf_in_theme_editor(g->node)") != 0);
    CHECK(strstr(b, "gt_ed.open") != 0);
    CHECK(strstr(src, "themeprev_placement(g->ms.w, g->g.w)") != 0);
    CHECK(strstr(src, "ch == 9 && preview_active(g)") != 0);
}

/* The preview holds one pen per distinct box colour (fill_rgb, prev_pens); text items draw through menu_text. The boxes of
 * the enum must fit the cache whatever the model: the filled items are counted from themeprev_layout, not from a spelling. */
static void test_themeprev_boxes_fit_the_pen_cache(void)
{
    themeprev pv;
    gt_model m;
    int i, boxes = 0;
    (void)pc_of; (void)pc_theme_changed;               /* the header's statics: only PC_MAX is wanted here */
    memset(&m, 0, sizeof m);
    themeprev_layout(&m.look, 400, 300, &pv);
    for (i = 0; i < TPV_COUNT; i++)
        if (pv.it[i].w > 0 && pv.it[i].h > 0 && !pv.it[i].text)
            boxes++;
    CHECK(boxes > 0);
    CHECK(boxes <= PC_MAX);
}

void suite_themeprev(void)
{
    test_themeprev_boxes_fit_the_pen_cache();
    test_themeprev_stock_and_thin();
    test_themeprev_radius_and_order();
    test_themeprev_small_pane();
    test_themeprev_colours();
    test_themeprev_placement();
    test_themeprev_reaches_gui();
}
