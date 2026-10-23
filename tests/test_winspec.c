#include "harness.h"
#include "../handler/winspec.h"

/* The Workbench icon's window "XCON:0/20/0/400/UP-Term/CLOSE": width 0 is the
 * screen's width, height 0 the screen's height, from the spec's edge on. */
static void sizes(void)
{
    CHECK_INT(winspec_size(0, 0, 1280), 1280);   /* icon: as wide as the screen */
    CHECK_INT(winspec_size(0, 10, 1280), 1270);  /* from the left edge on */
    CHECK_INT(winspec_size(0, 20, 512), 492);    /* height 0 with top 20 */
    CHECK_INT(winspec_size(640, 0, 1280), 640);  /* a given size is unchanged */
    CHECK_INT(winspec_size(400, 20, 512), 400);
    CHECK_INT(winspec_size(0, 2000, 1280), 1280); /* edge off the screen: the screen */
}

/* A CON: window (amiga personality) gets the same ground as XCON:; colour
 * options of the spec keep theirs. */
static void ground(void)
{
    unsigned long fg = 1, bg = 2;
    CHECK_INT(winspec_ground(0, &fg, &bg), 1);
    CHECK_INT(fg == 0xC0C0C0UL && bg == 0x000000UL, 1);
    fg = 1;
    bg = 2;
    CHECK_INT(winspec_ground(1, &fg, &bg), 0); /* DARK / LIGHT / FG / BG chose */
    CHECK_INT(fg == 1 && bg == 2, 1);
}

static winspec_opts parse(const char *f, int *hit)
{
    winspec_opts o = {0, 0, 0, 0, 0};
    *hit = winspec_option(f, &o);
    return o;
}

static void options(void)
{
    int hit;
    winspec_opts o = parse("CELLS 34x9", &hit);
    CHECK_INT(hit, 1);
    CHECK_INT(o.cols == 34 && o.rows == 9, 1);
    o = parse("cells 7X3", &hit);
    CHECK_INT(hit == 1 && o.cols == 7 && o.rows == 3, 1);
    parse("CELLS 34", &hit);  CHECK_INT(hit, 0);
    parse("CELLS 0x9", &hit); CHECK_INT(hit, 0);
    parse("CELLS 3x0", &hit); CHECK_INT(hit, 0);
    parse("CELLS 3x9z", &hit); CHECK_INT(hit, 0);
    parse("CELLSX 3x9", &hit); CHECK_INT(hit, 0);
    o = parse("CENTRE", &hit);  CHECK_INT(hit == 1 && o.centre == 1, 1);
    o = parse("center", &hit);  CHECK_INT(hit == 1 && o.centre == 1, 1);
    o = parse("FRONT", &hit);   CHECK_INT(hit == 1 && o.front == 1 && o.centre == 0, 1);
    o = parse("CloseInactive", &hit);
    CHECK_INT(hit == 1 && o.close_inactive == 1 && o.front == 0, 1);
    parse("CLOSE", &hit);       CHECK_INT(hit, 0); /* the existing CLOSE gadget is not it */
    parse("NOBORDER", &hit);    CHECK_INT(hit, 0);
}

/* The window box: cells override w/h, centre overrides x/y, the screen clamps. */
static void box(void)
{
    winspec_opts none = {0, 0, 0, 0, 0};
    winspec_opts cells = {34, 9, 0, 0, 0};
    winspec_opts cc = {34, 9, 1, 0, 0};
    winspec_opts ce = {0, 0, 1, 0, 0};
    int x, y, w, h;
    /* no option: the spec's box stands */
    x = 10; y = 20; w = 300; h = 100;
    winspec_box(&none, 8, 16, 4, 11, 4, 2, 640, 256, &x, &y, &w, &h);
    CHECK_INT(x == 10 && y == 20 && w == 300 && h == 100, 1);
    /* CELLS: 34x9 of 8x16 plus borders 4/11/4/2, spec w/h ignored, x/y kept */
    x = 10; y = 20; w = 1; h = 1;
    winspec_box(&cells, 8, 16, 4, 11, 4, 2, 640, 256, &x, &y, &w, &h);
    CHECK_INT(w, 34 * 8 + 8);
    CHECK_INT(h, 9 * 16 + 13);
    CHECK_INT(x == 10 && y == 20, 1);
    /* CENTRE with CELLS: x/y of the spec are overridden */
    x = 5; y = 5; w = 0; h = 0;
    winspec_box(&cc, 8, 16, 4, 11, 4, 2, 640, 256, &x, &y, &w, &h);
    CHECK_INT(x, (640 - 280) / 2);
    CHECK_INT(y, (256 - 157) / 2);
    /* borderless: no border sizes */
    x = 0; y = 0; w = 0; h = 0;
    winspec_box(&cc, 8, 16, 0, 0, 0, 0, 640, 256, &x, &y, &w, &h);
    CHECK_INT(w == 272 && h == 144 && x == 184 && y == 56, 1);
    /* CENTRE alone centres the spec's own size */
    x = 0; y = 0; w = 200; h = 100;
    winspec_box(&ce, 8, 16, 4, 11, 4, 2, 640, 256, &x, &y, &w, &h);
    CHECK_INT(x == 220 && y == 78 && w == 200 && h == 100, 1);
    /* larger than the screen: shrunk to it, at 0,0 */
    x = 50; y = 50; w = 0; h = 0;
    winspec_box(&cc, 8, 16, 4, 11, 4, 2, 200, 100, &x, &y, &w, &h);
    CHECK_INT(w == 200 && h == 100 && x == 0 && y == 0, 1);
    /* hanging over the right / bottom edge without CENTRE: moved in */
    x = 600; y = 250; w = 100; h = 50;
    winspec_box(&none, 8, 16, 4, 11, 4, 2, 640, 256, &x, &y, &w, &h);
    CHECK_INT(x == 540 && y == 206, 1);
    /* a negative origin: moved in */
    x = -5; y = -5; w = 100; h = 50;
    winspec_box(&none, 8, 16, 4, 11, 4, 2, 640, 256, &x, &y, &w, &h);
    CHECK_INT(x == 0 && y == 0, 1);
}

void suite_winspec(void)
{
    sizes();
    ground();
    options();
    box();
}
