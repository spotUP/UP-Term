/* amarchy_tile_boxes -- the master-stack boxes AmTile must produce, from the product's own code (amarchy/tile_core.c,
 * win_core.c) built for the host: tools/rig/amarchy_tile_rig.py compares the rig's windows with them (plan
 * 2026-10-28-amarchy-autotile.md, Phase 4).
 *     amarchy_tile_boxes SCREEN_W SCREEN_H TITLE GAP RATIO N [MINW MINH [MAXW MAXH]]
 * prints "box <i> <left> <top> <width> <height>" for i = 0..N-1, master first. Window limits default to the
 * ones tests/test_tile.c uses (50 x 30, no maximum). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../amarchy/tile_core.h"

#define MAXN 32

int main(int argc, char **argv)
{
    win_screen s;
    tile_win in[MAXN];
    win_box out[MAXN];
    int gap, ratio, n, i, a[4] = { 50, 30, 0, 0 };

    if (argc < 7) {
        fprintf(stderr, "usage: %s SCREEN_W SCREEN_H TITLE GAP RATIO N [MINW MINH [MAXW MAXH]]\n", argv[0]);
        return 2;
    }
    s.width = atoi(argv[1]);
    s.height = atoi(argv[2]);
    s.title = atoi(argv[3]);
    gap = atoi(argv[4]);
    ratio = atoi(argv[5]);
    n = atoi(argv[6]);
    for (i = 0; i < 4 && 7 + i < argc; i++)
        a[i] = atoi(argv[7 + i]);
    if (n < 1 || n > MAXN)
        return 2;
    memset(in, 0, sizeof in);
    for (i = 0; i < n; i++) {
        in[i].id = i + 1;
        in[i].flags = TW_SIZEGADGET;
        in[i].title = "w";
        in[i].box.minw = a[0];
        in[i].box.minh = a[1];
        in[i].box.maxw = a[2];
        in[i].box.maxh = a[3];
    }
    tile_layout(TL_MASTER, &s, gap, ratio, 1, n, in, out);
    for (i = 0; i < n; i++)
        printf("box %d %d %d %d %d\n", i, out[i].left, out[i].top, out[i].width, out[i].height);
    return 0;
}
