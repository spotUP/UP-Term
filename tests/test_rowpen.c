/* render/rowpen: which planes a run must write, against a model of what
 * every cell's pixels may hold. */
#include "harness.h"
#include "../render/rowpen.h"

#define ROWS 6
#define COLS 12

/* Per cell: the planes some pixel may have set (ones) and may have clear
 * (zeros). A cell of one pen p: ones p, zeros ~p. Unknown: both 0xFF. */
static unsigned char ones[ROWS][COLS], zeros[ROWS][COLS];
static vrp_row rows[ROWS];

static void model_fill(int y, int x0, int x1, int p)
{
    int x;
    for (x = x0; x < x1; x++) {
        ones[y][x] = (unsigned char)p;
        zeros[y][x] = (unsigned char)~p;
    }
}

/* A run in fg / bg writing only the planes of m: right when every plane
 * left out already holds, in every pixel, the one bit both pens have. */
static int mask_is_safe(int y, int x0, int x1, int fg, int bg, int m)
{
    int x, q;
    for (q = 0; q < 8; q++) {
        int b = 1 << q;
        if (m & b)
            continue;
        if ((fg & b) != (bg & b))
            return 0;
        for (x = x0; x < x1; x++)
            if ((fg & b) ? (zeros[y][x] & b) : (ones[y][x] & b))
                return 0;
    }
    return 1;
}

static void model_scroll(int top, int bottom, int k, int p)
{
    int y, x;
    if (k > 0) {
        for (y = top; y < bottom; y++)
            for (x = 0; x < COLS; x++) {
                ones[y][x] = y + k < bottom ? ones[y + k][x] : (unsigned char)p;
                zeros[y][x] = y + k < bottom ? zeros[y + k][x] : (unsigned char)~p;
            }
    } else {
        for (y = bottom - 1; y >= top; y--)
            for (x = 0; x < COLS; x++) {
                ones[y][x] = y + k >= top ? ones[y + k][x] : (unsigned char)p;
                zeros[y][x] = y + k >= top ? zeros[y + k][x] : (unsigned char)~p;
            }
    }
}

static unsigned long seed = 12345;
static int rnd(int n)
{
    seed = seed * 1103515245UL + 12345UL;
    return (int)((seed >> 16) % (unsigned long)n);
}

/* Random scrolls, runs, fills and drawings the records do not follow: no
 * run ever leaves out a plane it needed, and a fill is only skipped where
 * the cells hold its pen already. */
static void runs_never_leave_out_a_plane_they_need(void)
{
    int i, y, x, bad = 0, narrowed = 0, skipped = 0;
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < COLS; x++)
            ones[y][x] = zeros[y][x] = 0xFF;
    vrp_forget(rows, ROWS);
    for (i = 0; i < 20000; i++) {
        int op = rnd(10), x0 = rnd(COLS), x1 = x0 + 1 + rnd(COLS - x0), p = rnd(8), fg = rnd(16), bg = rnd(3);
        y = rnd(ROWS);
        if (op == 0) {
            int top = rnd(ROWS - 1), bottom = top + 2 + rnd(ROWS - top - 1), k = 1 + rnd(bottom - top);
            if (bottom > ROWS)
                bottom = ROWS;
            if (rnd(2))
                k = -k;
            vrp_scroll(rows, top, bottom, k, p);
            model_scroll(top, bottom, k, p);
        } else if (op <= 5) {
            int m = VRP_MASK(&rows[y], x0, fg, bg, 0xFF);
            if (!mask_is_safe(y, x0, x1, fg, bg, m))
                bad++;
            if (m != 0xFF)
                narrowed++;
            VRP_DRAWN(&rows[y], x1);
            for (x = x0; x < x1; x++) {
                ones[y][x] = (unsigned char)(fg | bg);
                zeros[y][x] = (unsigned char)~(fg & bg);
            }
        } else if (op <= 8) {
            if (rnd(2))
                x1 = COLS;
            if (!VRP_FILL_NEEDED(&rows[y], x0, p)) {
                skipped++;
                for (x = x0; x < x1; x++)
                    if (ones[y][x] != p || zeros[y][x] != (unsigned char)~p)
                        bad++;
            } else {
                model_fill(y, x0, x1, p);
            }
            vrp_filled(&rows[y], x0, x1, COLS, p);
        } else {
            /* a drawing the records do not follow (a decoration, an
             * image): the hook forgets the row */
            for (x = x0; x < x1; x++)
                ones[y][x] = zeros[y][x] = 0xFF;
            vrp_forget(&rows[y], 1);
        }
    }
    CHECK_INT(bad, 0);
    CHECK(narrowed > 1000);
    CHECK(skipped > 100);
}

/* conbench plain-lines / sgr-colour on a 16-colour screen: a line that a
 * scroll has just cleared takes only its pens' planes. */
static void a_line_a_scroll_cleared_writes_only_its_pens_planes(void)
{
    vrp_forget(rows, ROWS);
    vrp_scroll(rows, 0, ROWS, 1, 0);
    CHECK_INT(VRP_MASK(&rows[ROWS - 1], 0, 1, 0, 0x0F), 0x01); /* pen 1 on 0: one plane of four */
    CHECK_INT(VRP_MASK(&rows[ROWS - 1], 0, 6, 0, 0x0F), 0x06); /* ANSI colour 6: two */
    CHECK_INT(VRP_MASK(&rows[ROWS - 1], 0, 0, 0, 0x0F), 0x00); /* pen 0 on 0 over pen 0: none */
    VRP_DRAWN(&rows[ROWS - 1], 8);
    CHECK_INT(VRP_MASK(&rows[ROWS - 1], 8, 3, 0, 0x0F), 0x03); /* the line goes on in the next frame */
    CHECK_INT(VRP_MASK(&rows[ROWS - 1], 4, 3, 0, 0x0F), 0x0F); /* over what this line drew: all */
    CHECK_INT(VRP_MASK(&rows[0], 0, 1, 0, 0x0F), 0x0F);        /* a row scrolled up: not known */
}

/* The tail of a row a scroll cleared is not filled again in its own pen. */
static void a_cleared_rows_tail_is_not_filled_again(void)
{
    vrp_clear(rows, ROWS, 0);
    VRP_DRAWN(&rows[2], 5);
    CHECK_INT(VRP_FILL_NEEDED(&rows[2], 5, 0), 0);
    CHECK_INT(VRP_FILL_NEEDED(&rows[2], 4, 0), 1);
    CHECK_INT(VRP_FILL_NEEDED(&rows[2], 5, 2), 1);
    vrp_filled(&rows[2], 3, COLS, COLS, 2);
    CHECK_INT(rows[2].from, 3);
    CHECK_INT(rows[2].pen, 2);
}

void suite_rowpen(void)
{
    runs_never_leave_out_a_plane_they_need();
    a_line_a_scroll_cleared_writes_only_its_pens_planes();
    a_cleared_rows_tail_is_not_filled_again();
}
