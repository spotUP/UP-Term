#include <string.h>
#include "harness.h"
#include "../amarchy/tile_core.h"

static const win_screen scr = { 800, 600, 12, 0 };

static tile_win mk(long id, const char *title)
{
    tile_win w;
    memset(&w, 0, sizeof w);
    w.id = id;
    w.flags = TW_SIZEGADGET;
    w.title = title;
    w.box.minw = 50; w.box.minh = 30;
    return w;
}

static void check_box(const win_box *b, int l, int t, int w, int h)
{
    CHECK_INT(b->left, l);
    CHECK_INT(b->top, t);
    CHECK_INT(b->width, w);
    CHECK_INT(b->height, h);
}

static void master_stack_boxes(void)
{
    tile_win in[4];
    win_box out[4];
    int i;
    for (i = 0; i < 4; i++)
        in[i] = mk(i + 1, "w");

    tile_layout(TL_MASTER, &scr, 4, 55, 1, 1, in, out);            /* work area 4,16 792x580 */
    check_box(&out[0], 4, 16, 792, 580);

    tile_layout(TL_MASTER, &scr, 4, 55, 1, 2, in, out);            /* master (792-4)*55/100 = 433 */
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 355, 580);

    tile_layout(TL_MASTER, &scr, 4, 55, 1, 3, in, out);
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 355, 288);
    check_box(&out[2], 441, 308, 355, 288);

    tile_layout(TL_MASTER, &scr, 4, 55, 1, 4, in, out);            /* (580-8)/3 = 190, the last cell takes 192 */
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 355, 190);
    check_box(&out[2], 441, 210, 355, 190);
    check_box(&out[3], 441, 404, 355, 192);
}

static void min_size_beats_the_cell(void)
{
    tile_win in[2];
    win_box out[2];
    in[0] = mk(1, "a");
    in[1] = mk(2, "b");
    in[1].box.minw = 400;                                  /* wider than the 355 px stack cell */
    in[1].box.maxw = 700;
    tile_layout(TL_MASTER, &scr, 4, 55, 1, 2, in, out);
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 400, 580);
    in[1].box.minw = 50;
    in[1].box.maxh = 200;                                  /* a Max below the cell keeps the box small */
    tile_layout(TL_MASTER, &scr, 4, 55, 1, 2, in, out);
    check_box(&out[1], 441, 16, 355, 200);
}

static void classification(void)
{
    tile_win w = mk(1, "Shell");
    CHECK_INT(tile_classify(&w), TILE_TILE);
    w.flags = 0;                                           /* no size gadget: requesters, fixed windows */
    CHECK_INT(tile_classify(&w), TILE_FLOAT);
    w = mk(1, "Shell");
    w.flags |= TW_BACKDROP;
    CHECK_INT(tile_classify(&w), TILE_FLOAT);
    w = mk(1, "Shell");
    w.flags |= TW_BORDERLESS;
    CHECK_INT(tile_classify(&w), TILE_FLOAT);
    w = mk(1, "Amarchy");
    CHECK_INT(tile_classify(&w), TILE_FLOAT);
    w = mk(1, NULL);
    CHECK_INT(tile_classify(&w), TILE_TILE);
    w = mk(1, "Shell");
    w.box.maxw = w.box.minw;                               /* a size gadget on a window that cannot change size */
    CHECK_INT(tile_classify(&w), TILE_FLOAT);
}

static void order_update(void)
{
    long order[8] = { 1, 2, 3 };
    long set[8] = { 3, 1, 2, 4 };
    int n = tile_order_update(order, 3, set, 4);           /* 4 is new: front */
    CHECK_INT(n, 4);
    CHECK_INT(order[0], 4);
    CHECK_INT(order[1], 1);
    CHECK_INT(order[2], 2);
    CHECK_INT(order[3], 3);
    set[0] = 4; set[1] = 3;                                /* 1 and 2 closed */
    n = tile_order_update(order, 4, set, 2);
    CHECK_INT(n, 2);
    CHECK_INT(order[0], 4);
    CHECK_INT(order[1], 3);
    set[0] = 3; set[1] = 4; set[2] = 5; set[3] = 6;        /* two new at once, the old keep their order */
    n = tile_order_update(order, 2, set, 4);
    CHECK_INT(n, 4);
    CHECK_INT(order[2], 4);
    CHECK_INT(order[3], 3);
    CHECK(order[0] != order[1] && (order[0] == 5 || order[0] == 6));
    n = tile_order_update(order, 4, set, 0);               /* all closed */
    CHECK_INT(n, 0);
}

static void screen_switch(void)
{
    long cur = 0;
    CHECK_INT(tile_screen_switch(&cur, 100, 0), 0);        /* first screen */
    CHECK_INT(cur, 100);
    CHECK_INT(tile_screen_switch(&cur, 100, 3), 3);        /* same screen keeps the order */
    CHECK_INT(tile_screen_switch(&cur, 200, 3), 0);        /* front screen changed: order dropped */
    CHECK_INT(cur, 200);
    CHECK_INT(tile_screen_switch(&cur, 200, 2), 2);
}

static void state_file(void)
{
    CHECK(tile_state_on("on\n", 3));
    CHECK(tile_state_on("on", 2));
    CHECK(tile_state_on("ON\r\n", 4));
    CHECK(!tile_state_on("off\n", 4));
    CHECK(!tile_state_on("", 0));
    CHECK(!tile_state_on("o", 1));
    CHECK(!tile_state_on("once", 4));
}

/* Worked numbers of the plan: 800x600, title 14, gap 4, ratio 55 -> work area 4,18 792x578. */
static const win_screen scr14 = { 800, 600, 14, 0 };

static void lay(int kind, int ratio, int nm, int n, win_box *out)
{
    tile_win in[TILE_NMASTER_MAX + 4];
    int i;
    for (i = 0; i < n; i++)
        in[i] = mk(i + 1, "w");
    tile_layout(kind, &scr14, 4, ratio, nm, n, in, out);
}

static void worked_numbers(void)
{
    win_box o[32];
    lay(TL_DWINDLE, 55, 1, 1, o);
    check_box(&o[0], 4, 18, 792, 578);
    lay(TL_DWINDLE, 55, 1, 2, o);
    check_box(&o[1], 4, 18, 433, 578);
    check_box(&o[0], 441, 18, 355, 578);
    lay(TL_DWINDLE, 55, 1, 3, o);
    check_box(&o[2], 4, 18, 433, 578);
    check_box(&o[1], 441, 18, 355, 315);
    check_box(&o[0], 441, 337, 355, 259);
    lay(TL_DWINDLE, 55, 1, 4, o);
    check_box(&o[3], 4, 18, 433, 578);
    check_box(&o[2], 441, 18, 355, 315);
    check_box(&o[1], 441, 337, 193, 259);
    check_box(&o[0], 638, 337, 158, 259);

    lay(TL_MASTER, 55, 1, 2, o);
    check_box(&o[0], 4, 18, 433, 578);
    check_box(&o[1], 441, 18, 355, 578);
    lay(TL_MASTER, 55, 2, 4, o);
    check_box(&o[0], 4, 18, 433, 287);
    check_box(&o[1], 4, 309, 433, 287);
    check_box(&o[2], 441, 18, 355, 287);
    check_box(&o[3], 441, 309, 355, 287);
    lay(TL_MASTER, 55, 3, 2, o);                           /* nmaster >= n: one full-width stack */
    check_box(&o[0], 4, 18, 792, 287);
    check_box(&o[1], 4, 309, 792, 287);

    lay(TL_MIRROR, 55, 1, 2, o);
    check_box(&o[1], 4, 18, 355, 578);
    check_box(&o[0], 363, 18, 433, 578);

    lay(TL_COLUMNS, 55, 1, 3, o);
    check_box(&o[0], 4, 18, 261, 578);
    check_box(&o[1], 269, 18, 261, 578);
    check_box(&o[2], 534, 18, 262, 578);

    lay(TL_ROWS, 55, 1, 4, o);
    check_box(&o[0], 4, 18, 792, 141);
    check_box(&o[1], 4, 163, 792, 141);
    check_box(&o[2], 4, 308, 792, 141);
    check_box(&o[3], 4, 453, 792, 143);

    lay(TL_GRID, 55, 1, 3, o);
    check_box(&o[0], 4, 18, 394, 287);
    check_box(&o[1], 402, 18, 394, 287);
    check_box(&o[2], 4, 309, 792, 287);
    lay(TL_GRID, 55, 1, 4, o);
    check_box(&o[0], 4, 18, 394, 287);
    check_box(&o[1], 402, 18, 394, 287);
    check_box(&o[2], 4, 309, 394, 287);
    check_box(&o[3], 402, 309, 394, 287);

    lay(TL_MONOCLE, 55, 1, 3, o);
    check_box(&o[0], 4, 18, 792, 578);
    check_box(&o[1], 4, 18, 792, 578);
    check_box(&o[2], 4, 18, 792, 578);

    lay(99, 55, 1, 2, o);                                  /* unknown kind = dwindle */
    check_box(&o[1], 4, 18, 433, 578);
}

static void min_max_every_layout(void)
{
    int k, n, i, wide;
    tile_win in[5];
    win_box o[5];
    for (k = 0; k < TL_COUNT; k++)
        for (n = 2; n <= 5; n++)
            for (wide = 0; wide < n; wide++) {
                for (i = 0; i < n; i++)
                    in[i] = mk(i + 1, "w");
                in[wide].box.minw = 300;
                tile_layout(k, &scr14, 4, 55, 2, n, in, o);
                CHECK(o[wide].width >= 300);
                for (i = 0; i < n; i++) {
                    CHECK(o[i].width > 0 && o[i].height > 0);
                    CHECK(o[i].left >= 0 && o[i].top >= 0);
                    CHECK(i == wide || (o[i].left + o[i].width <= 800 && o[i].top + o[i].height <= 600)); /* Min beats the room (win_fit) */
                }
            }
}

static int same(const win_box *a, const win_box *b, int n)
{
    return memcmp(a, b, sizeof(win_box) * (size_t)n) == 0;
}

static void knobs(void)
{
    win_box a[6], b[6];
    int k;
    for (k = 0; k < TL_COUNT; k++) {
        lay(k, 55, 1, 5, a);
        lay(k, 65, 1, 5, b);
        if (k == TL_DWINDLE || k == TL_MASTER || k == TL_MIRROR)
            CHECK(!same(a, b, 5));
        else
            CHECK(same(a, b, 5));
        lay(k, 55, 2, 5, b);
        if (k == TL_MASTER || k == TL_MIRROR)
            CHECK(!same(a, b, 5));
        else
            CHECK(same(a, b, 5));
    }
    for (k = 0; k < TL_COUNT; k++) {                       /* n = 1 and n = 32 must not crash */
        win_box big[32];
        tile_win in[32];
        int i;
        lay(k, 55, 2, 1, a);
        for (i = 0; i < 32; i++)
            in[i] = mk(i + 1, "w");
        tile_layout(k, &scr14, 4, 55, 8, 32, in, big);
        CHECK(big[31].width > 0);
    }
}

static void layout_names_and_steps(void)
{
    int k = -1, i;
    static const char *const names[TL_COUNT] = { "dwindle", "master-stack", "mirrored", "columns", "rows", "grid",
                                                 "monocle" };
    char up[32];
    for (i = 0; i < TL_COUNT; i++) {
        size_t j;
        CHECK(strcmp(tile_layout_name(i), names[i]) == 0);
        CHECK_INT(tile_layout_parse(names[i], &k), 1);
        CHECK_INT(k, i);
        for (j = 0; j <= strlen(names[i]); j++)
            up[j] = names[i][j] >= 'a' && names[i][j] <= 'z' ? (char)(names[i][j] - 32) : names[i][j];
        k = -1;
        CHECK_INT(tile_layout_parse(up, &k), 1);
        CHECK_INT(k, i);
    }
    CHECK(tile_layout_name(TL_COUNT) == NULL && tile_layout_name(-1) == NULL);
    CHECK_INT(tile_layout_parse("grid next", &k), 1);      /* a word may end at a space */
    CHECK_INT(k, TL_GRID);
    CHECK_INT(tile_layout_parse("", &k), 0);
    CHECK_INT(tile_layout_parse("x", &k), 0);
    CHECK_INT(tile_layout_parse("master", &k), 0);
    CHECK_INT(tile_layout_step(TL_MONOCLE, 1), TL_DWINDLE);
    CHECK_INT(tile_layout_step(TL_DWINDLE, -1), TL_MONOCLE);
    CHECK_INT(tile_layout_step(TL_ROWS, 1), TL_GRID);
    CHECK_INT(tile_layout_step(TL_ROWS, -1), TL_COLUMNS);
}

static void promote_and_cycle(void)
{
    long o[4] = { 1, 2, 3, 4 };
    CHECK_INT(tile_promote(o, 4, 3), 1);
    CHECK(o[0] == 3 && o[1] == 1 && o[2] == 2 && o[3] == 4);
    CHECK_INT(tile_promote(o, 4, 4), 1);
    CHECK(o[0] == 4 && o[1] == 3 && o[2] == 1 && o[3] == 2);
    CHECK_INT(tile_promote(o, 4, 4), 1);
    CHECK(o[0] == 4 && o[1] == 3);
    CHECK_INT(tile_promote(o, 4, 99), 0);
    CHECK(o[0] == 4 && o[1] == 3 && o[2] == 1 && o[3] == 2);
    CHECK_INT(tile_cycle(o, 4, 3, 1), 2);
    CHECK_INT(tile_cycle(o, 4, 3, -1), 0);
    CHECK_INT(tile_cycle(o, 4, 2, 1), 0);                  /* wraps forward */
    CHECK_INT(tile_cycle(o, 4, 4, -1), 3);                 /* wraps back */
    CHECK_INT(tile_cycle(o, 4, 99, 1), 0);                 /* untiled id: the first */
    CHECK_INT(tile_cycle(o, 4, 99, -1), 0);
    CHECK_INT(tile_cycle(o, 0, 4, 1), 0);
    CHECK_INT(tile_cycle(o, 1, 4, 1), 0);
}

static void bottom_reserve_and_bar(void)
{
    static const win_screen b16 = { 800, 600, 14, 16 };
    tile_win in[3];
    win_box o[3];
    int i;
    for (i = 0; i < 3; i++)
        in[i] = mk(i + 1, "w");
    tile_layout(TL_DWINDLE, &b16, 4, 55, 1, 1, in, o);
    check_box(&o[0], 4, 18, 792, 562);                 /* work area h 562 at gap 4 */
    tile_layout(TL_MONOCLE, &b16, 0, 55, 1, 1, in, o);
    check_box(&o[0], 0, 14, 800, 570);
    tile_layout(TL_ROWS, &b16, 0, 55, 1, 2, in, o);
    CHECK_INT(o[1].top + o[1].height, 584);            /* the last row ends on the reserve */
    in[0].title = "Amarchy bar";
    CHECK_INT(tile_classify(&in[0]), TILE_FLOAT);
    in[0].title = "Amarchy";
    CHECK_INT(tile_classify(&in[0]), TILE_FLOAT);
}

static void swap_and_floats(void)
{
    long o[4] = { 1, 2, 3, 4 };
    long live[3] = { 1, 3, 9 };
    tile_floats f;
    memset(&f, 0, sizeof f);
    CHECK_INT(tile_swap(o, 4, 1, 3), 1);
    CHECK_INT(o[0], 3); CHECK_INT(o[2], 1);
    CHECK_INT(tile_swap(o, 4, 3, 1), 1);               /* back the other way */
    CHECK_INT(o[0], 1); CHECK_INT(o[2], 3);
    CHECK_INT(tile_swap(o, 4, 1, 77), 0);              /* absent id: nothing moves */
    CHECK_INT(tile_swap(o, 4, 77, 1), 0);
    CHECK_INT(o[0], 1); CHECK_INT(o[1], 2);
    CHECK_INT(tile_float_set(&f, 2), 1);
    CHECK_INT(tile_float_set(&f, 3), 1);
    CHECK_INT(tile_float_set(&f, 3), 0);               /* twice: one entry */
    CHECK_INT(f.n, 2);
    CHECK_INT(tile_order_update(o, 4, live, 3), 3);    /* a retile leaves the float set alone */
    CHECK(tile_float_has(&f, 2) && tile_float_has(&f, 3));
    tile_float_prune(&f, live, 3);                     /* 2 closed */
    CHECK(!tile_float_has(&f, 2));
    CHECK(tile_float_has(&f, 3));
    CHECK_INT(tile_float_clear(&f, 3), 1);
    CHECK_INT(tile_float_clear(&f, 3), 0);
    CHECK_INT(f.n, 0);
    {
        int i, added = 0;
        for (i = 1; i <= TILE_MAX + 5; i++)
            added += tile_float_set(&f, i);
        CHECK_INT(added, TILE_MAX);                    /* full: the extras are refused */
    }
}

void suite_tile(void)
{
    state_file();
    master_stack_boxes();
    worked_numbers();
    min_max_every_layout();
    knobs();
    layout_names_and_steps();
    promote_and_cycle();
    min_size_beats_the_cell();
    classification();
    order_update();
    screen_switch();
    bottom_reserve_and_bar();
    swap_and_floats();
}
