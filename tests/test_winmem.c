/* What one window costs at open, counted on the host
 * (research/2026-10-04_window-memory.md): the engine's blocks for the
 * measured window (77 x 20 cells, the built-in 500 lines of scrollback),
 * the line editor (its struct and its blocks) and the profile table the
 * handler allocates per window. The handler itself has no host build: its
 * own struct is held by a compile-time bound in the 68k build
 * (handler/vtcon_handler.c, con_size_bound). The numbers are the host's
 * (64-bit pointers); the 68k ones are in the research file. */
#include "harness.h"
#include "../handler/lineedit.h"
#include "../config/upconf.h"

/* Host bytes for one window before this work (2026-10-04, same counting):
 * engine 53,125 + le_line 36,136 + upconf 56,240 = 145,501; after it
 * 53,125 + 2,664 + 17,844 = 73,633. The engine's part is unchanged (its
 * grid and scrollback ring are the screen itself). */
#define WINMEM_BEFORE 145501L
/* The bound: after the cut, with a little room. A climb past it fails. */
#define WINMEM_BOUND 76000L

static void nothing(void *u, const unsigned char *b, long n)
{
    (void)u;
    (void)b;
    (void)n;
}

static void a_window_open_costs_less_than_the_bound(void)
{
    static le_line le;
    long before = vt_count_live, engine, total;
    vt_term *t = vt_new(77, 20, 500, 0, 0);
    CHECK(t != 0);
    engine = vt_count_live - before;
    le_init(&le, t, nothing, 0);
    total = vt_count_live - before + (long)sizeof(le_line) + (long)sizeof(upconf);
    printf("  window open on the host: engine %ld + le_line %ld + upconf %ld = %ld bytes"
           " (was %ld)\n", engine, (long)sizeof(le_line), (long)sizeof(upconf), total, WINMEM_BEFORE);
    CHECK(total <= WINMEM_BOUND);

    /* the first line typed and Returned: the history and undo blocks come
     * then, a few hundred bytes, not the old 34 KB */
    {
        long open = vt_count_live;
        le_hist_add(&le, (const unsigned char *)"list ram:\n", 10);
        CHECK(vt_count_live - open <= 512);
    }
    le_free(&le);
    vt_free(t);
    CHECK_INT(vt_count_live, before); /* closed: every byte back */
}

void suite_winmem(void)
{
    a_window_open_costs_less_than_the_bound();
}
