/* engbench: the engine alone on the 68k, no window -- what a byte costs
 * the parser and the cell model (ledger S1). Each workload goes through
 * vt_feed + vt_flush into an 80 x 32 terminal with callbacks that draw
 * nothing; the time is in 1/50 s, the rate in bytes a second.
 *   engbench [REPS n]      (default 3: the best of them) */
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "../../engine/vtengine.h"

static void damage(void *u, int x0, int y0, int x1, int y1) { (void)u; (void)x0; (void)y0; (void)x1; (void)y1; }
static void scroll(void *u, int t, int b, int n) { (void)u; (void)t; (void)b; (void)n; }

static char buf[70000];
static long n;

static void s(const char *t) { while (*t) buf[n++] = *t++; }
static void num(int v) { if (v >= 100) buf[n++] = (char)('0' + v / 100); if (v >= 10) buf[n++] = (char)('0' + v / 10 % 10); buf[n++] = (char)('0' + v % 10); }

static long now(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return ds.ds_Minute * 3000L + ds.ds_Tick;
}

static void build(int w)
{
    int i, x, y;
    n = 0;
    switch (w) {
    case 0: /* 816 lines of 78 characters */
        for (i = 0; i < 816; i++) { for (x = 0; x < 78; x++) buf[n++] = (char)('a' + (i + x) % 26); s("\r\n"); }
        break;
    case 1: /* 12000 newlines */
        for (i = 0; i < 12000; i++) buf[n++] = '\n';
        break;
    case 2: /* a colour before every character */
        for (i = 0; i < 4000; i++) { s("\033[3"); num(1 + i % 7); s("m"); buf[n++] = (char)('a' + i % 26); if (i % 78 == 77) s("\r\n"); }
        break;
    case 3: /* a 256-colour pair and a half block a cell: UPDemo's plasma */
        for (y = 0; y < 24; y++) for (x = 0; x < 80; x++) { s("\033[38;5;"); num(17 + (x + y) % 200); s(";48;5;"); num(40 + (x * y) % 200); s("m\342\226\200"); }
        break;
    case 4: /* position, erase to the end of the line, text: a full-screen repaint */
        for (i = 0; i < 20; i++) for (y = 1; y <= 30; y++) { s("\033["); num(y); s(";1H\033[K"); for (x = 0; x < 50; x++) buf[n++] = (char)('a' + (x + y + i) % 26); }
        break;
    default: /* insert and delete lines */
        for (i = 0; i < 300; i++) s("\033[5;1H\033[L\033[M");
        break;
    }
}

int main(int argc, char **argv)
{
    static const char *const name[] = { "plain lines", "newlines", "colour a char", "256 pair a cell", "frame repaint", "ins/del line" };
    static vt_callbacks cb;
    int reps = argc > 2 ? atoi(argv[2]) : 3, w, r, pers;
    cb.damage = damage;
    cb.scroll = scroll;
    for (pers = 0; pers < 2; pers++) {
        Printf((STRPTR)"%s\n", (LONG)(pers ? "amiga dialect" : "xterm dialect"));
        for (w = 0; w < 6; w++) {
            long best = 0x7FFFFFFF, i;
            build(w);
            for (r = 0; r < reps; r++) {
                vt_term *t = vt_new(80, 32, 500, &cb, 0);
                long t0;
                if (!t)
                    return 20;
                vt_set_personality(t, pers ? VT_AMIGA : VT_XTERM);
                t0 = now();
                for (i = 0; i < n; i += 4096) { /* 4K writes, a flush each: a frame's worth */
                    vt_feed(t, (const vt_u8 *)buf + i, n - i < 4096 ? n - i : 4096);
                    vt_flush(t);
                }
                t0 = now() - t0;
                if (t0 < best)
                    best = t0;
                vt_free(t);
            }
            Printf((STRPTR)"  %-16s %6ld bytes %4ld ticks %7ld bytes/s\n", (LONG)name[w], n, best,
                   best ? n * 50 / best : 0L);
        }
    }
    return 0;
}
