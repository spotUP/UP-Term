/* index_load.c -- the cache file read for Amarchy's menus (index_core.h, ix_load_file). Its own file
 * so the scanner and the host tests do not link the stdio part. */
#include <stdio.h>
#include "index_core.h"

void ix_load_file(const keymap *k, ix_index *x, const char *path)
{
    static char buf[IX_MAX_ROWS * (IX_LABEL + IX_PATH + 16)];
    FILE *f = fopen(path, "r");
    size_t got = 0;
    if (f) {
        got = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
    }
    buf[got] = 0;
    ix_load_text(k, x, got ? buf : 0);
}
