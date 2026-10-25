/* index_load.c -- the cache file read for Amarchy's menus (index_core.h, ix_load_file). Its own file
 * so the scanner and the host tests do not link the stdio part. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index_core.h"
#include "clips_core.h"

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
    ix_load_clips(x);
}

void ix_load_clips(ix_index *x)
{
    clip_ring r;
    char lab[CLIPS_LABEL * 4 + 1], *labs[CLIPS_MAX];
    int i;
    memset(&r, 0, sizeof r);
    clips_load_file(&r, CLIPS_PATH);
    for (i = 0; i < r.n; i++) {
        lab[0] = 0;
        clips_label(r.text[i], r.len[i], lab, sizeof lab);
        labs[i] = (char *)malloc(strlen(lab) + 1);
        if (labs[i])
            strcpy(labs[i], lab);
        else
            break;
    }
    ix_add_clips(x, (const char *const *)labs, i);
    while (i-- > 0)
        free(labs[i]);
    clips_free(&r);
}
