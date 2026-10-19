/* handler/tabslot.h: the host's tabs as slots of split panes -- a split
 * lands in the splitting pane's slot and takes the focus, the maximum and
 * the minimum refuse a split, a pane leaving gives its space and the focus
 * back, the last pane of a slot takes the slot with it. */
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

/* Right Amiga arrows: the focus goes to the pane on that side, and stays
 * put at an edge or for a pane in no slot. */
static void the_focus_moves_to_the_pane_on_that_side(void)
{
    fresh();
    tabslot_add(T, &N, 4, &E);
    tabslot_add(T, &N, 4, &A);
    tabslot_split(T, N, &A, &B, LAY_RIGHT, 8, 8, 0, 0, 640, 200); /* A | B */
    tabslot_split(T, N, &B, &C, LAY_DOWN, 8, 8, 0, 0, 640, 200);  /* A | B over C */
    CHECK(tabslot_focused(&T[1]) == &C);
    CHECK_INT(tabslot_move(T, N, &C, LAY_UP), 1);
    CHECK(tabslot_focused(&T[1]) == &B);
    CHECK_INT(tabslot_move(T, N, &B, LAY_LEFT), 1);
    CHECK(tabslot_focused(&T[1]) == &A);
    CHECK_INT(tabslot_move(T, N, &A, LAY_LEFT), -1);
    CHECK(tabslot_focused(&T[1]) == &A);
    CHECK_INT(tabslot_move(T, N, &A, LAY_RIGHT), 1);
    CHECK(tabslot_focused(&T[1]) != &A);
    CHECK_INT(tabslot_move(T, N, &D, LAY_RIGHT), -1);
    CHECK_INT(tabslot_move(T, N, &E, LAY_RIGHT), -1); /* one pane: nowhere to go */
    CHECK(tabslot_focused(&T[0]) == &E);
}

void suite_tabslot(void)
{
    a_new_tab_is_a_slot_with_one_focused_pane();
    a_full_bar_refuses_a_slot();
    a_split_lands_in_the_splitting_panes_slot_and_takes_the_focus();
    the_fifth_pane_and_a_pane_too_small_are_refused();
    a_pane_leaving_gives_its_space_and_the_focus_back();
    the_last_pane_takes_its_slot_and_the_rest_move_down();
    the_shown_slot_stays_shown_when_another_goes();
    the_focus_moves_to_the_pane_on_that_side();
}
