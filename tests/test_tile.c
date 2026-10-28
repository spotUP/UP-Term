#include <string.h>
#include "harness.h"
#include "../amarchy/tile_core.h"

static const win_screen scr = { 800, 600, 12 };

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

    tile_master_stack(&scr, 4, 55, 1, in, out);            /* work area 4,16 792x580 */
    check_box(&out[0], 4, 16, 792, 580);

    tile_master_stack(&scr, 4, 55, 2, in, out);            /* master (792-4)*55/100 = 433 */
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 355, 580);

    tile_master_stack(&scr, 4, 55, 3, in, out);
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 355, 288);
    check_box(&out[2], 441, 308, 355, 288);

    tile_master_stack(&scr, 4, 55, 4, in, out);            /* (580-8)/3 = 190, the last cell takes 192 */
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
    tile_master_stack(&scr, 4, 55, 2, in, out);
    check_box(&out[0], 4, 16, 433, 580);
    check_box(&out[1], 441, 16, 400, 580);
    in[1].box.minw = 50;
    in[1].box.maxh = 200;                                  /* a Max below the cell keeps the box small */
    tile_master_stack(&scr, 4, 55, 2, in, out);
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

void suite_tile(void)
{
    state_file();
    master_stack_boxes();
    min_size_beats_the_cell();
    classification();
    order_update();
}
