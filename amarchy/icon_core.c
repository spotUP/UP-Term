/* icon_core: see icon_core.h. */
#include <string.h>

#include "icon_core.h"

int ic_find(ic_cache *c, const char *path)
{
    int i;
    for (i = 0; i < IC_SLOTS; i++)
        if (c->state[i] != IC_EMPTY && !strcmp(c->path[i], path)) {
            c->used[i] = ++c->tick;
            return i;
        }
    return -1;
}

int ic_claim(ic_cache *c, const char *path)
{
    int i, s = 0;
    if (strlen(path) >= IX_PATH)
        return -1;
    for (i = 0; i < IC_SLOTS; i++) {
        if (c->state[i] == IC_EMPTY) {
            s = i;
            break;
        }
        if (c->used[i] < c->used[s])
            s = i;
    }
    strcpy(c->path[s], path);
    c->state[s] = IC_EMPTY;
    c->used[s] = ++c->tick;
    return s;
}

void ic_set(ic_cache *c, int slot, int state)
{
    if (slot >= 0 && slot < IC_SLOTS)
        c->state[slot] = state;
}

int ic_fit(int sw, int sh, int bw, int bh, int *dw, int *dh, int *dx, int *dy)
{
    if (sw < 1 || sh < 1 || bw < 1 || bh < 1)
        return 0;
    /* the larger scale down wins: dw / sw and dh / sh are both at most bw / sw and bh / sh */
    if ((long)bw * sh <= (long)bh * sw) {
        *dw = bw;
        *dh = (int)((long)sh * bw / sw);
    } else {
        *dh = bh;
        *dw = (int)((long)sw * bh / sh);
    }
    if (*dw < 1)
        *dw = 1;
    if (*dh < 1)
        *dh = 1;
    *dx = (bw - *dw) / 2;
    *dy = (bh - *dh) / 2;
    return 1;
}
