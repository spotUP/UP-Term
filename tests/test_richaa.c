/* RL20 rich Amarchy UI box corners (amarchy/richaa_core.c): reachability through rich_corners, the mix per pixel, the
 * depth and size fallbacks. The glue (amarchy/gui.c fill_round) hands rich_corners the same put callback shape. */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../amarchy/richaa_core.h"

typedef struct fb {
    unsigned long px[32 * 32];
} fb;

static void put(void *ctx, int x, int y, unsigned long rgb)
{
    ((fb *)ctx)->px[y * 32 + x] = rgb;
}

static void t_corners(void)
{
    static fb f;
    unsigned long before = rich_aa_calls;
    int w = 20, h = 12, i, mid = 0, bg = 0, n;
    for (i = 0; i < 32 * 32; i++)
        f.px[i] = 0xDEADBEUL;                       /* untouched marker */
    n = rich_corners(8, 2, 3, w, h, 0xFFFFFF, 0x000000, put, &f);
    CHECK(n > 0);
    CHECK_INT((int)(rich_aa_calls - before), 1);    /* the AA code ran */
    CHECK(f.px[3 * 32 + 2 + w / 2] == 0xDEADBEUL);  /* the box's middle is not touched */
    CHECK(f.px[3 * 32 + 2] == 0x000000UL);          /* the outermost corner pixel is background again */
    for (i = 0; i < 32 * 32; i++) {
        unsigned long v = f.px[i];
        if (v == 0xDEADBEUL)
            continue;
        if (v == 0)
            bg++;
        else if (v != 0xFFFFFFUL && (v & 0xFF) > 0 && (v & 0xFF) < 255)
            mid++;
    }
    CHECK(bg >= 4 && mid >= 4);                     /* every corner has background and intermediate shades */
    /* mirror: the four corners blend identically */
    CHECK(f.px[3 * 32 + 3] == f.px[3 * 32 + 2 + w - 2]);
    CHECK(f.px[3 * 32 + 3] == f.px[(3 + h - 1) * 32 + 2 + w - 2]);
}

static void t_fallbacks(void)
{
    static fb f;
    unsigned long before = rich_aa_calls;
    CHECK_INT(rich_corners(2, 0, 0, 20, 12, 0xFFFFFF, 0, put, &f), 0);   /* depth 2: stock */
    CHECK_INT(rich_corners(3, 0, 0, 20, 12, 0xFFFFFF, 0, put, &f), 0);
    CHECK_INT(rich_corners(8, 0, 0, 2, 12, 0xFFFFFF, 0, put, &f), 0);    /* too small */
    CHECK_INT(rich_corners(8, 0, 0, 20, 12, 0xFFFFFF, 0, 0, &f), 0);
    CHECK(rich_aa_calls == before);
    CHECK(f.px[0] == 0);
}

void suite_richaa(void)
{
    t_corners();
    t_fallbacks();
}
