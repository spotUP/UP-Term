/* pick_list: the pure part of the list picker (see pick_list.h). */
#include "pick_list.h"

int pk_initial(int n, int current)
{
    return current >= 0 && current < n ? current : 0;
}

int pk_rows(int n)
{
    return n < 1 ? 1 : n > PK_ROWS ? PK_ROWS : n;
}

int pk_top(int n, int rows, int sel)
{
    int top = sel - rows / 2, max = n - rows;
    if (max < 0)
        max = 0;
    return top < 0 ? 0 : top > max ? max : top;
}

int pk_result(int n, int sel, int confirmed)
{
    return confirmed && sel >= 0 && sel < n ? sel : -1;
}
