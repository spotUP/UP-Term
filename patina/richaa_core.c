/* richaa_core -- see richaa_core.h */
#include "richaa_core.h"
#include "../render/emoji.h"

unsigned long rich_aa_calls;

int rich_corners(int depth, int x, int y, int w, int h, unsigned long fg, unsigned long bg, rich_put_fn put, void *ctx)
{
    int r, c, dx, dy, n = 0;
    if (!put || depth < RICH_MIN_DEPTH || w < 3 || h < 3)
        return 0;
    r = raa_radius(h);
    if (r > h / 2)
        r = h / 2;
    if (r > w / 2)
        r = w / 2;
    for (c = 0; c < 4; c++)
        for (dy = 0; dy < r; dy++)
            for (dx = 0; dx < r; dx++) {
                int px = (c & 1) ? w - 1 - dx : dx, py = (c & 2) ? h - 1 - dy : dy;
                int cov = raa_cover(w, h, r, px, py);
                if (cov >= 255)
                    continue;
                put(ctx, x + px, y + py, cov == 0 ? bg : ce_mix(fg, bg, cov));
                n++;
            }
    if (n)
        rich_aa_calls++;
    return n;
}
