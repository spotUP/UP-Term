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

void suite_winspec(void)
{
    sizes();
    ground();
}
