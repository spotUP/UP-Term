/* handler/tabslot.h: the host's tabs as slots of split panes -- a split
 * lands in the splitting pane's slot and takes the focus, the maximum and
 * the minimum refuse a split, a pane leaving gives its space and the focus
 * back, the last pane of a slot takes the slot with it. */
#include <stdio.h>
#include <stdlib.h>
#include "harness.h"
#include "../handler/tabslot.h"

static tab_slot T[4];
static int N, ACT;
static int A, B, C, D, E; /* stand-ins for the panes' cons */

static void fresh(void)
{
    memset(T, 0, sizeof(T));
    N = 0;
    ACT = 0;
}

static void a_new_tab_is_a_slot_with_one_focused_pane(void)
{
    fresh();
    CHECK_INT(tabslot_add(T, &N, 4, &A), 0);
    CHECK_INT(tabslot_add(T, &N, 4, &B), 1);
    CHECK_INT(N, 2);
    CHECK(tabslot_focused(&T[1]) == &B);
    CHECK_INT(tabslot_panes(&T[1]), 1);
    CHECK_INT(tabslot_find(T, N, &B, 0), 1);
    CHECK_INT(tabslot_find(T, N, &C, 0), -1);
}

static void a_full_bar_refuses_a_slot(void)
{
    fresh();
    tabslot_add(T, &N, 2, &A);
    tabslot_add(T, &N, 2, &B);
    CHECK_INT(tabslot_add(T, &N, 2, &C), -1);
    CHECK_INT(N, 2);
}

static void a_split_lands_in_the_splitting_panes_slot_and_takes_the_focus(void)
{
    int id;
    lay_rect a, b;
    fresh();
    tabslot_add(T, &N, 4, &A);
    tabslot_add(T, &N, 4, &B);
    /* 640 x 200 below an 11 px bar, 8 x 8 cells */
    CHECK_INT(tabslot_split(T, N, &B, &C, LAY_RIGHT, 8, 8, 0, 11, 640, 200), 1);
    CHECK_INT(tabslot_panes(&T[1]), 2);
    CHECK_INT(tabslot_panes(&T[0]), 1);
    CHECK(tabslot_focused(&T[1]) == &C);
    CHECK_INT(tabslot_find(T, N, &C, &id), 1);
    CHECK_INT(id, 1);
    /* the layout has the cell and the area: two halves and a 1 px line between */
    lay_pane_rect(&T[1].l, 0, &a);
    lay_pane_rect(&T[1].l, 1, &b);
    CHECK_INT(a.x, 0); CHECK_INT(a.y, 11); CHECK_INT(a.h, 200);
    CHECK_INT(a.w % 8, 0);
    CHECK_INT(b.x, a.x + a.w + 1);
    CHECK_INT(b.x + b.w, 640);
}

static void the_fifth_pane_and_a_pane_too_small_are_refused(void)
{
    fresh();
    tabslot_add(T, &N, 4, &A);
    CHECK_INT(tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200), 1);
    CHECK_INT(tabslot_split(T, N, &B, &C, LAY_DOWN, 8, 8, 0, 0, 640, 200), 2);
    CHECK_INT(tabslot_split(T, N, &A, &D, LAY_DOWN, 8, 8, 0, 0, 640, 200), 3);
    CHECK_INT(tabslot_split(T, N, &D, &E, LAY_RIGHT, 8, 8, 0, 0, 640, 200), -1);
    CHECK_INT(tabslot_find(T, N, &E, 0), -1);
    CHECK(tabslot_focused(&T[0]) == &D); /* a refused split moves nothing */
    /* 15 columns cannot hold two of the 10-column minimum */
    fresh();
    tabslot_add(T, &N, 4, &A);
    CHECK_INT(tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 15 * 8, 200), -1);
    CHECK_INT(tabslot_panes(&T[0]), 1);
    /* from gone (it left before the new pane registered): no split */
    CHECK_INT(tabslot_split(T, N, &C, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200), -1);
}

static void a_pane_leaving_gives_its_space_and_the_focus_back(void)
{
    int gone;
    lay_rect a;
    fresh();
    tabslot_add(T, &N, 4, &A);
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    CHECK_INT(tabslot_remove(T, &N, &ACT, &B, &gone), 0);
    CHECK_INT(gone, 0);
    CHECK_INT(N, 1);
    CHECK_INT(tabslot_panes(&T[0]), 1);
    CHECK(tabslot_focused(&T[0]) == &A);
    CHECK_INT(tabslot_find(T, N, &B, 0), -1);
    lay_pane_rect(&T[0].l, 0, &a);
    CHECK_INT(a.w, 640);
    /* a pane without the focus leaving keeps the focus where it was */
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    tabslot_split(T, N, &B, &C, LAY_DOWN, 8, 8, 0, 0, 640, 200);
    CHECK_INT(tabslot_remove(T, &N, &ACT, &A, &gone), 0);
    CHECK(tabslot_focused(&T[0]) == &C);
    CHECK_INT(tabslot_panes(&T[0]), 2);
}

static void the_last_pane_takes_its_slot_and_the_rest_move_down(void)
{
    int gone;
    fresh();
    tabslot_add(T, &N, 4, &A);
    tabslot_add(T, &N, 4, &B);
    tabslot_add(T, &N, 4, &C);
    tabslot_split(T, N, &C, &D, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    CHECK_INT(tabslot_remove(T, &N, &ACT, &B, &gone), 1);
    CHECK_INT(gone, 1);
    CHECK_INT(N, 2);
    CHECK_INT(tabslot_find(T, N, &C, 0), 1);
    CHECK_INT(tabslot_find(T, N, &D, 0), 1);
    CHECK(tabslot_focused(&T[1]) == &D);
    CHECK_INT(tabslot_panes(&T[1]), 2); /* the moved slot's layout came with it */
    CHECK_INT(tabslot_remove(T, &N, &ACT, &E, &gone), -1);
    CHECK_INT(gone, 0);
}

/* The shown slot stays the shown slot when a slot before it goes (the old
 * host list left the index where it was: the next tab became "active"
 * without being shown, and the one drawing lost the keys). */
static void the_shown_slot_stays_shown_when_another_goes(void)
{
    int gone;
    fresh();
    tabslot_add(T, &N, 4, &A);
    tabslot_add(T, &N, 4, &B);
    tabslot_add(T, &N, 4, &C);
    ACT = 2;
    tabslot_remove(T, &N, &ACT, &A, &gone);
    CHECK_INT(ACT, 1);
    CHECK(tabslot_focused(&T[ACT]) == &C);
    /* one after it going leaves it alone */
    tabslot_add(T, &N, 4, &D);
    ACT = 1;
    tabslot_remove(T, &N, &ACT, &D, &gone);
    CHECK_INT(ACT, 1);
    /* the shown one going, the last: the one before it */
    tabslot_remove(T, &N, &ACT, &C, &gone);
    CHECK_INT(ACT, 0);
    CHECK(tabslot_focused(&T[ACT]) == &B);
    /* the shown one going, not the last: the one that moves into its place */
    tabslot_add(T, &N, 4, &C);
    ACT = 0;
    tabslot_remove(T, &N, &ACT, &B, &gone);
    CHECK_INT(ACT, 0);
    CHECK(tabslot_focused(&T[ACT]) == &C);
    /* the very last going */
    tabslot_remove(T, &N, &ACT, &C, &gone);
    CHECK_INT(N, 0);
    CHECK_INT(ACT, -1);
}

/* Right Amiga [ / ]: previous / next pane in reading order, wrapping. */
static void the_focus_cycles_through_the_panes_in_reading_order(void)
{
    fresh();
    tabslot_add(T, &N, 4, &E);
    tabslot_add(T, &N, 4, &A);
    CHECK_INT(tabslot_cycle(T, N, &A, 1), -1);  /* one pane: stays */
    CHECK(tabslot_focused(&T[1]) == &A);
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200); /* A | B */
    tabslot_split(T, N, &B, &C, LAY_RIGHT, 8, 8, 0, 0, 640, 200); /* A | B | C */
    CHECK_INT(tabslot_cycle(T, N, &A, 1), 1);
    CHECK(tabslot_focused(&T[1]) == &B);
    CHECK_INT(tabslot_cycle(T, N, &B, 1), 1);
    CHECK(tabslot_focused(&T[1]) == &C);
    CHECK_INT(tabslot_cycle(T, N, &C, 1), 1);
    CHECK(tabslot_focused(&T[1]) == &A);        /* wraps */
    CHECK_INT(tabslot_cycle(T, N, &A, -1), 1);
    CHECK(tabslot_focused(&T[1]) == &C);        /* wraps back */
    CHECK_INT(tabslot_cycle(T, N, &C, -1), 1);
    CHECK(tabslot_focused(&T[1]) == &B);
    CHECK_INT(tabslot_cycle(T, N, &D, 1), -1);  /* in no slot */
    /* A | (B over C): the pane ids are not the reading order */
    fresh();
    tabslot_add(T, &N, 4, &A);
    tabslot_split(T, N, &A, &B, LAY_DOWN, 8, 8, 0, 0, 640, 200);  /* A over B */
    tabslot_split(T, N, &A, &C, LAY_RIGHT, 8, 8, 0, 0, 640, 200); /* (A | C) over B */
    CHECK_INT(tabslot_cycle(T, N, &A, 1), 0);
    CHECK(tabslot_focused(&T[0]) == &C);
    CHECK_INT(tabslot_cycle(T, N, &C, 1), 0);
    CHECK(tabslot_focused(&T[0]) == &B);        /* top row first, then the bottom */
    CHECK_INT(tabslot_cycle(T, N, &B, 1), 0);
    CHECK(tabslot_focused(&T[0]) == &A);
}

static void the_pane_under_a_point_is_found_by_its_place(void)
{
    fresh();
    tabslot_add(T, &N, 4, &A);
    CHECK_INT(tabslot_at(&T[0], 10, 10), -1); /* one pane: no lookup */
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200); /* A | B */
    CHECK(T[0].pane[tabslot_at(&T[0], 10, 10)] == &A);
    CHECK(T[0].pane[tabslot_at(&T[0], 630, 190)] == &B);
    CHECK_INT(tabslot_at(&T[0], 10, 250), -1); /* below the area */
    tabslot_zoom(T, N, &B);
    CHECK_INT(tabslot_at(&T[0], 10, 10), -1); /* zoomed: A is hidden */
}

static void a_zoom_shows_only_the_focused_pane_until_the_layout_changes(void)
{
    int gone, active = 1;
    fresh();
    tabslot_add(T, &N, 4, &E);
    CHECK_INT(tabslot_zoom(T, N, &E), -1); /* one pane: nothing to zoom */
    CHECK_INT(T[0].zoom, 0);
    tabslot_add(T, &N, 4, &A);
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200); /* A | B, B focused */
    CHECK_INT(tabslot_zoom(T, N, &B), 1);
    CHECK(tabslot_shows(&T[1], 1) && !tabslot_shows(&T[1], 0));
    CHECK_INT(tabslot_zoom(T, N, &B), 1); /* again: back */
    CHECK(tabslot_shows(&T[1], 0) && tabslot_shows(&T[1], 1));
    tabslot_zoom(T, N, &B);
    CHECK_INT(tabslot_cycle(T, N, &B, -1), 1); /* a cycle ends it */
    CHECK_INT(T[1].zoom, 0);
    tabslot_zoom(T, N, &A);
    tabslot_split(T, N, &A, &C, LAY_DOWN, 8, 8, 0, 0, 640, 200); /* so does a split */
    CHECK_INT(T[1].zoom, 0);
    tabslot_zoom(T, N, &C);
    tabslot_remove(T, &N, &active, &B, &gone); /* and a pane leaving */
    CHECK_INT(T[1].zoom, 0);
    CHECK(tabslot_shows(&T[1], 0) && !tabslot_shows(&T[1], 1));
    CHECK_INT(tabslot_zoom(T, N, &D), -1);
}

/* the host's own shell ending keeps the window while any other pane lives,
 * also the other pane of its own slot: the count is panes, not slots */
static void every_pane_of_every_slot_is_counted(void)
{
    fresh();
    tabslot_add(T, &N, 4, &A);
    CHECK_INT(tabslot_all(T, N), 1);
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    CHECK_INT(N, 1);
    CHECK_INT(tabslot_all(T, N), 2);
    tabslot_add(T, &N, 4, &C);
    tabslot_split(T, N, &C, &D, LAY_DOWN, 8, 8, 0, 0, 640, 200);
    tabslot_split(T, N, &D, &E, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    CHECK_INT(tabslot_all(T, N), 5);
    CHECK_INT(tabslot_all(T, 0), 0);
}

/* rig 2026-10-20: split and close the new pane twice, split a third
 * time, then the first pane leaves: the third pane is the one left, it
 * has the focus and is drawn */
static void first_pane_leaves_after_split_cycles(void)
{
    int gone, k, id;
    fresh();
    tabslot_add(T, &N, 4, &A);
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    tabslot_remove(T, &N, &ACT, &B, &gone);
    tabslot_split(T, N, &A, &C, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    tabslot_remove(T, &N, &ACT, &C, &gone);
    id = tabslot_split(T, N, &A, &D, LAY_RIGHT, 8, 8, 0, 0, 640, 200);
    CHECK(id >= 0);
    CHECK_INT(tabslot_all(T, N), 2);
    tabslot_remove(T, &N, &ACT, &A, &gone);
    CHECK_INT(gone, 0);
    CHECK_INT(N, 1);
    CHECK_INT(tabslot_all(T, N), 1);
    CHECK(tabslot_focused(&T[0]) == &D);
    CHECK_INT(tabslot_find(T, N, &D, &k), 0);
    CHECK(tabslot_shows(&T[0], k));
    CHECK_INT(tabslot_find(T, N, &A, 0), -1);
}

/* TAB-FILLPEN: the active tab stays distinct when the flat pen map makes FILLPEN == BACKGROUNDPEN */
static void the_active_tab_differs_under_the_stock_and_the_flat_pen_map(void)
{
    enum { TEXT = 2, BACK = 0, FILL = 5, FILLTEXT = 6 };
    unsigned short stock[8] = { 0, 1, 1, 2, 3, 3, 1, 0 }, flat[8] = { 0, 1, 1, 2, 3, 0, 1, 0 };
    unsigned short *maps[2];
    int m;
    char *src;
    FILE *f = fopen("handler/vtcon_handler.c", "rb");
    long n;
    maps[0] = stock;
    maps[1] = flat;
    stock[TEXT] = 1; stock[BACK] = 0; stock[FILL] = 3; stock[FILLTEXT] = 1;
    flat[TEXT] = 1; flat[BACK] = 0; flat[FILL] = flat[BACK]; flat[FILLTEXT] = 1;
    for (m = 0; m < 2; m++) {
        CHECK(tabbar_pen(maps[m], TEXT, BACK, 1, TABBAR_GROUND) != tabbar_pen(maps[m], TEXT, BACK, 0, TABBAR_GROUND));
        CHECK(tabbar_pen(maps[m], TEXT, BACK, 1, TABBAR_GROUND) != tabbar_pen(maps[m], TEXT, BACK, 1, TABBAR_LETTERS));
        CHECK(tabbar_pen(maps[m], TEXT, BACK, 0, TABBAR_GROUND) != tabbar_pen(maps[m], TEXT, BACK, 0, TABBAR_LETTERS));
    }
    /* wired: bar_draw takes both pens from the helper, not from FILLPEN */
    CHECK(f != 0);
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    src = (char *)malloc((size_t)n + 1);
    CHECK(src != 0);
    if (src) {
        char *b, *e;
        n = (long)fread(src, 1, (size_t)n, f);
        src[n] = 0;
        b = strstr(src, "static void bar_draw(con *c)\n{");
        e = b ? strstr(b, "\n}\n") : 0;
        CHECK(b && e);
        if (b && e) {
            *e = 0;
            CHECK(strstr(b, "tabbar_pen(pens, TEXTPEN, BACKGROUNDPEN, i == c->active, TABBAR_GROUND)") != 0);
            CHECK(strstr(b, "tabbar_pen(pens, TEXTPEN, BACKGROUNDPEN, i == c->active, TABBAR_LETTERS)") != 0);
            CHECK(strstr(b, "FILLPEN") == 0);
        }
        free(src);
    }
    fclose(f);
}

void suite_tabslot(void)
{
    first_pane_leaves_after_split_cycles();
    every_pane_of_every_slot_is_counted();
    a_new_tab_is_a_slot_with_one_focused_pane();
    a_full_bar_refuses_a_slot();
    a_split_lands_in_the_splitting_panes_slot_and_takes_the_focus();
    the_fifth_pane_and_a_pane_too_small_are_refused();
    a_pane_leaving_gives_its_space_and_the_focus_back();
    the_last_pane_takes_its_slot_and_the_rest_move_down();
    the_shown_slot_stays_shown_when_another_goes();
    the_focus_cycles_through_the_panes_in_reading_order();
    a_zoom_shows_only_the_focused_pane_until_the_layout_changes();
    the_pane_under_a_point_is_found_by_its_place();
    the_active_tab_differs_under_the_stock_and_the_flat_pen_map();
}
