/* rowpen.c -- see rowpen.h. */
#include "rowpen.h"

void vrp_filled(vrp_row *row, int x0, int x1, int cols, int p)
{
    if (!VRP_FILL_NEEDED(row, x0, p))
        return; /* the cells held p: nothing changed */
    if (x1 >= cols || (p == row->pen && x1 >= row->from)) {
        /* to the row's end, or up to the cells that held p: from x0 on
         * it holds p */
        row->from = (short)x0;
        row->pen = (unsigned char)p;
        return;
    }
    VRP_DRAWN(row, x1);
}

void vrp_forget(vrp_row *rows, int n)
{
    int i;
    for (i = 0; i < n; i++)
        rows[i].from = VRP_NONE;
}

void vrp_clear(vrp_row *rows, int n, int p)
{
    int i;
    for (i = 0; i < n; i++) {
        rows[i].from = 0;
        rows[i].pen = (unsigned char)p;
    }
}

void vrp_scroll(vrp_row *rows, int top, int bottom, int k, int p)
{
    int i, h = bottom - top;
    if (h <= 0 || !k)
        return;
    if (k >= h || -k >= h) {
        vrp_clear(rows + top, h, p);
        return;
    }
    if (k > 0) {
        for (i = top; i < bottom - k; i++)
            rows[i] = rows[i + k];
        vrp_clear(rows + bottom - k, k, p);
    } else {
        for (i = bottom - 1; i >= top - k; i--)
            rows[i] = rows[i + k];
        vrp_clear(rows + top, -k, p);
    }
}
