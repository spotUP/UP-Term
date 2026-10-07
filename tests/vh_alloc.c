/* Counting allocator for the leak gate (V44): tools/bashdiff.py --leak runs build/vsh_host_leak, whose
 * sources are compiled with -Dmalloc=vh_malloc (and calloc, realloc, free), so every block the shell
 * core and the host driver take is counted and a block still live after sh_shell_free is a leak. The
 * tally is exact and needs no heap statistics of the operating system (macOS had none that stdio's
 * own lazy blocks did not disturb). This file is built without the macros. */
#include <stdlib.h>

static long live_blocks;

long vh_live(void)
{
    return live_blocks;
}

void *vh_malloc(size_t n)
{
    void *p = malloc(n);
    if (p)
        live_blocks++;
    return p;
}

void *vh_calloc(size_t n, size_t m)
{
    void *p = calloc(n, m);
    if (p)
        live_blocks++;
    return p;
}

void *vh_realloc(void *old, size_t n)
{
    void *p = realloc(old, n);
    if (p && !old)
        live_blocks++;
    else if (!p && old && n == 0)
        live_blocks--;
    return p;
}

void vh_free(void *p)
{
    if (p)
        live_blocks--;
    free(p);
}
