/* vtdump COLS ROWS [personality] < stream: prints the grid, one row per line,
 * trailing blanks kept, then the colours and attributes of every cell, so a reference emulator can be diffed against it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/vtengine.h"

static int utf8(char *o, unsigned c)
{
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F));
    o[2] = (char)(0x80 | (c & 0x3F));
    return 3;
}

int main(int argc, char **argv)
{
    static vt_u8 buf[65536];
    vt_term *t;
    size_t n;
    int y, x, cols, rows, cx, cy;
    char line[8192];
    if (argc < 3)
        return 2;
    cols = atoi(argv[1]);
    rows = atoi(argv[2]);
    t = vt_new(cols, rows, 0, 0, 0);
    if (argc > 3 && !strcmp(argv[3], "amiga"))
        vt_set_personality(t, VT_AMIGA);
    else if (argc > 3 && !strcmp(argv[3], "pcansi"))
        vt_set_personality(t, VT_PCANSI);
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0)
        vt_write(t, buf, (long)n);
    for (y = 0; y < rows; y++) {
        int len = 0, nc;
        const vt_cell *c = vt_row(t, y, &nc);
        for (x = 0; x < nc; x++)
            if (c[x].width)
                len += utf8(line + len, c[x].ch);
        line[len] = 0;
        printf("%s\n", line);
    }
    /* Then per cell: fg bg attr, space separated, one row per line. */
    for (y = 0; y < rows; y++) {
        int nc;
        const vt_cell *c = vt_row(t, y, &nc);
        for (x = 0; x < nc; x++)
            printf("%s%lu,%lu,%u,%u", x ? " " : "", (unsigned long)c[x].fg, (unsigned long)c[x].bg,
                   /* inverse as shown: the cell's XOR the screen's */
                   c[x].attr ^ ((vt_modes(t) & VT_MODE_SCREEN_REVERSE) ? VT_ATTR_INVERSE : 0),
                   c[x].ch);
        printf("\n");
    }
    if (argc > 4 && !strcmp(argv[4], "sizes")) {
        /* the DEC line size of each row (tools/rig/vttest_rig.py) */
        printf("sizes");
        for (y = 0; y < rows; y++)
            printf(" %d", vt_row_size(t, y));
        printf("\n");
    }
    vt_cursor(t, &cx, &cy);
    printf("@%d,%d\n", cx, cy);
    {
        const char *kinds[16];
        long counts[16], u = vt_unhandled(t, kinds, counts, 16);
        int i;
        printf("unhandled %ld", u);
        for (i = 0; i < 16 && kinds[i]; i++)
            printf(" [%s]x%ld", kinds[i], counts[i]);
        printf("\n");
    }
    vt_free(t);
    return 0;
}
