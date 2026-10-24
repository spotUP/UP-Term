/* index_load.c -- the cache file read for Amarchy's menus (index_core.h, ix_load_file). Its own file
 * so the scanner and the host tests do not link the stdio part. */
#include <stdio.h>
#include <stdlib.h>
#include "index_core.h"

void ix_load_file(const keymap *k, ix_index *x, const char *path)
{
    FILE *f = fopen(path, "r");
    char *buf = 0;
    long size = 0;
    size_t got = 0;
    /* the whole file, however big: a cache of thousands of rows is read in full */
    if (f && !fseek(f, 0, SEEK_END) && (size = ftell(f)) > 0 && !fseek(f, 0, SEEK_SET) &&
        (buf = malloc(size + 1)))
        got = fread(buf, 1, size, f);
    if (f)
        fclose(f);
    if (buf)
        buf[got] = 0;
    ix_load_text(k, x, got ? buf : 0);
    free(buf);
}
