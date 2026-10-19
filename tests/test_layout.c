/* render/layout: the split panes of a tab as numbers -- rects tile the
 * window, ratios survive a resize, the minimum size and the pane maximum
 * hold, close gives the space back, directional focus picks the neighbour. */
#include "harness.h"
#include "../render/layout.h"

static lay L;

/* Sum of every pane's area and every divider's area. */
static long covered(const lay *l)
{
    long sum = 0;
    int p, d;
    lay_rect r;
    for (p = 0; p < LAY_MAX_PANES; p++)
        if (lay_pane_rect(l, p, &r))
            sum += (long)r.w * r.h;
    for (d = 0; lay_divider_at(l, d) >= 0; d++) {
        lay_divider_rect(l, lay_divider_at(l, d), &r);
        sum += (long)r.w * r.h;
    }
    return sum;
}

static void a_lone_pane_fills_the_window(void)
{
    lay_rect r;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 4, 20, 640, 256);
    CHECK_INT(lay_pane_rect(&L, 0, &r), 1);
    CHECK_INT(r.x, 4); CHECK_INT(r.y, 20); CHECK_INT(r.w, 640); CHECK_INT(r.h, 256);
    CHECK_INT(lay_pane_rect(&L, 1, &r), 0);
    CHECK_INT(lay_divider_at(&L, 0), -1);
}

static void a_cell_divider_split_gives_exact_numbers(void)
{
    lay_rect a, b, d;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 640, 256);
    CHECK_INT(lay_split(&L, 0, LAY_RIGHT), 1);
    lay_pane_rect(&L, 0, &a);
    lay_pane_rect(&L, 1, &b);
    lay_divider_rect(&L, lay_divider_at(&L, 0), &d);
    CHECK_INT(a.x, 0);   CHECK_INT(a.w, 320);
    CHECK_INT(d.x, 320); CHECK_INT(d.w, 8); CHECK_INT(d.h, 256);
    CHECK_INT(b.x, 328); CHECK_INT(b.w, 312);
    CHECK_INT(a.h, 256); CHECK_INT(b.h, 256);
    CHECK_INT(covered(&L), 640L * 256);
}

static void a_pixel_divider_split_tiles_the_window_exactly(void)
{
    lay_rect a, b, d;
    lay_init(&L, 8, 16, LAY_DIV_PIXEL, 10, 3, 0, 0, 640, 256);
    CHECK_INT(lay_split(&L, 0, LAY_RIGHT), 1);
    lay_pane_rect(&L, 0, &a);
    lay_pane_rect(&L, 1, &b);
    lay_divider_rect(&L, lay_divider_at(&L, 0), &d);
    CHECK_INT(a.w, 320);        /* a whole number of cells */
    CHECK_INT(d.w, 1);
    CHECK_INT(b.x, 321);
    CHECK_INT(b.w, 319);        /* takes the pixels the cells cannot use */
    CHECK_INT(covered(&L), 640L * 256);
    CHECK_INT(lay_split(&L, 1, LAY_DOWN), 2);
    CHECK_INT(covered(&L), 640L * 256);
}

static void areas_sum_to_the_window_in_a_2x2_grid_at_any_size(void)
{
    int mode, w, bad = 0;
    for (mode = LAY_DIV_CELL; mode <= LAY_DIV_PIXEL; mode++)
        for (w = 200; w <= 700; w += 37) {
            lay_init(&L, 8, 16, mode, 10, 3, 3, 5, w, 301);
            lay_split(&L, 0, LAY_RIGHT);
            lay_split(&L, 0, LAY_DOWN);
            lay_split(&L, 1, LAY_DOWN);
            if (lay_pane_count(&L) != 4 || covered(&L) != (long)w * 301)
                bad++;
        }
    CHECK_INT(bad, 0);
}

static void a_resize_keeps_the_ratio(void)
{
    lay_rect a;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 640, 256);
    lay_split(&L, 0, LAY_RIGHT);
    CHECK_INT(lay_drag(&L, lay_divider_at(&L, 0), 160), 1);
    lay_pane_rect(&L, 0, &a);
    CHECK_INT(a.w, 160);                        /* 160 of 632 */
    lay_solve(&L, 0, 0, 1264, 256);
    lay_pane_rect(&L, 0, &a);
    CHECK_INT(a.w, 320);                        /* 160 of 632 again, scaled */
    CHECK_INT(covered(&L), 1264L * 256);
    lay_solve(&L, 0, 0, 640, 256);
    lay_pane_rect(&L, 0, &a);
    CHECK_INT(a.w, 160);                        /* and back, no drift */
}

static void a_fifth_pane_is_refused(void)
{
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 1280, 512);
    CHECK_INT(lay_split(&L, 0, LAY_RIGHT), 1);
    CHECK_INT(lay_split(&L, 0, LAY_DOWN), 2);
    CHECK_INT(lay_split(&L, 1, LAY_DOWN), 3);
    CHECK_INT(lay_pane_count(&L), 4);
    CHECK_INT(lay_split(&L, 3, LAY_RIGHT), -1);
    CHECK_INT(lay_split(&L, 0, LAY_DOWN), -1);
    CHECK_INT(lay_pane_count(&L), 4);
}

static void a_split_that_breaks_the_minimum_is_refused(void)
{
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 200, 100);
    CHECK_INT(lay_split(&L, 0, LAY_RIGHT), 1);   /* 80 + 8 + 80 <= 200 */
    CHECK_INT(lay_split(&L, 1, LAY_RIGHT), -1);  /* 3 x 80 + 16 > 200 */
    CHECK_INT(lay_pane_count(&L), 2);
    CHECK_INT(lay_split(&L, 0, LAY_DOWN), -1);   /* 48 + 16 + 48 = 112 > 100 */
}

static void a_drag_snaps_to_cells_and_holds_the_minimum(void)
{
    lay_rect a, b;
    int d;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 640, 256);
    lay_split(&L, 0, LAY_RIGHT);
    d = lay_divider_at(&L, 0);
    CHECK_INT(lay_drag(&L, d, 203), 1);          /* 203 -> the cell at 200 */
    lay_pane_rect(&L, 0, &a);
    CHECK_INT(a.w, 200);
    CHECK_INT(lay_drag(&L, d, -50), 1);
    lay_pane_rect(&L, 0, &a);
    CHECK_INT(a.w, 80);                          /* 10 columns */
    CHECK_INT(lay_drag(&L, d, 5000), 1);
    lay_pane_rect(&L, 1, &b);
    CHECK_INT(b.w, 80);
    CHECK_INT(lay_drag(&L, d, 5000), 0);         /* already there */
    CHECK_INT(covered(&L), 640L * 256);
}

static void a_vertical_drag_holds_the_row_minimum(void)
{
    lay_rect a, b;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 640, 256);
    lay_split(&L, 0, LAY_DOWN);
    lay_drag(&L, lay_divider_at(&L, 0), 0);
    lay_pane_rect(&L, 0, &a);
    CHECK_INT(a.h, 48);
    lay_drag(&L, lay_divider_at(&L, 0), 9999);
    lay_pane_rect(&L, 1, &b);
    CHECK_INT(b.h, 48);
}

static void closing_a_pane_gives_its_space_to_the_sibling(void)
{
    lay_rect r;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 640, 256);
    lay_split(&L, 0, LAY_RIGHT);
    CHECK_INT(lay_close(&L, 1), 0);
    CHECK_INT(lay_pane_count(&L), 1);
    lay_pane_rect(&L, 0, &r);
    CHECK_INT(r.w, 640); CHECK_INT(r.h, 256);
    CHECK_INT(lay_divider_at(&L, 0), -1);
    CHECK_INT(lay_close(&L, 0), -1);             /* the last pane stays */
    CHECK_INT(lay_pane_rect(&L, 1, &r), 0);
}

static void closing_in_a_bigger_tree_keeps_the_rest_tiled(void)
{
    lay_rect r;
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 640, 256);
    lay_split(&L, 0, LAY_RIGHT);                 /* 0 | 1 */
    lay_split(&L, 1, LAY_DOWN);                  /* 0 | 1 over 2 */
    CHECK_INT(lay_close(&L, 1), 2);              /* 2 takes the right column */
    lay_pane_rect(&L, 2, &r);
    CHECK_INT(r.x, 328); CHECK_INT(r.h, 256);
    CHECK_INT(covered(&L), 640L * 256);
    CHECK_INT(lay_close(&L, 0), 2);
    lay_pane_rect(&L, 2, &r);
    CHECK_INT(r.x, 0); CHECK_INT(r.w, 640);
    /* a closed id is reused by the next split */
    CHECK_INT(lay_split(&L, 2, LAY_DOWN), 0);
}

static void focus_moves_to_the_right_pane_in_a_2x2_grid(void)
{
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 1280, 512);
    lay_split(&L, 0, LAY_RIGHT);                 /* 0 | 1 */
    lay_split(&L, 0, LAY_DOWN);                  /* 0 over 2 */
    lay_split(&L, 1, LAY_DOWN);                  /* 1 over 3 */
    CHECK_INT(lay_neighbour(&L, 0, LAY_RIGHT), 1);
    CHECK_INT(lay_neighbour(&L, 0, LAY_DOWN), 2);
    CHECK_INT(lay_neighbour(&L, 3, LAY_LEFT), 2);
    CHECK_INT(lay_neighbour(&L, 3, LAY_UP), 1);
    CHECK_INT(lay_neighbour(&L, 2, LAY_RIGHT), 3);
    CHECK_INT(lay_neighbour(&L, 1, LAY_LEFT), 0);
    CHECK_INT(lay_neighbour(&L, 0, LAY_LEFT), -1);
    CHECK_INT(lay_neighbour(&L, 0, LAY_UP), -1);
    CHECK_INT(lay_neighbour(&L, 3, LAY_DOWN), -1);
}

static void focus_picks_the_pane_beside_not_a_diagonal(void)
{
    lay_init(&L, 8, 16, LAY_DIV_CELL, 10, 3, 0, 0, 1280, 512);
    lay_split(&L, 0, LAY_RIGHT);                 /* 0 | 1 */
    lay_split(&L, 0, LAY_DOWN);                  /* 0 over 2, 1 full height */
    CHECK_INT(lay_neighbour(&L, 2, LAY_RIGHT), 1);
    CHECK_INT(lay_neighbour(&L, 1, LAY_LEFT), 0);   /* the nearer centre: top left */
}

static void the_pointer_finds_the_divider_it_is_on(void)
{
    int d;
    lay_init(&L, 8, 16, LAY_DIV_PIXEL, 10, 3, 0, 0, 640, 256);
    lay_split(&L, 0, LAY_RIGHT);                 /* divider at x = 320 */
    d = lay_divider_at(&L, 0);
    CHECK_INT(lay_hit_divider(&L, 320, 100, 0), d);
    CHECK_INT(lay_hit_divider(&L, 319, 100, 0), -1);
    CHECK_INT(lay_hit_divider(&L, 321, 100, 0), -1);
    CHECK_INT(lay_hit_divider(&L, 318, 100, 3), d);
    CHECK_INT(lay_hit_divider(&L, 324, 100, 3), -1);
    CHECK_INT(lay_hit_divider(&L, 320, 256, 3), -1);  /* below the window */
}

void suite_layout(void)
{
    a_lone_pane_fills_the_window();
    a_cell_divider_split_gives_exact_numbers();
    a_pixel_divider_split_tiles_the_window_exactly();
    areas_sum_to_the_window_in_a_2x2_grid_at_any_size();
    a_resize_keeps_the_ratio();
    a_fifth_pane_is_refused();
    a_split_that_breaks_the_minimum_is_refused();
    a_drag_snaps_to_cells_and_holds_the_minimum();
    a_vertical_drag_holds_the_row_minimum();
    closing_a_pane_gives_its_space_to_the_sibling();
    closing_in_a_bigger_tree_keeps_the_rest_tiled();
    focus_moves_to_the_right_pane_in_a_2x2_grid();
    focus_picks_the_pane_beside_not_a_diagonal();
    the_pointer_finds_the_divider_it_is_on();
}
