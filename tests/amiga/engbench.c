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

/* vtengine_68k.s against what its C says: the cells written, where it
 * stops (a wide glyph's halves), what it leaves alone. 0 when right. */
long vt_asm_put_run(vt_cell *c, const vt_u8 *b, long n, const vt_cell *proto);
void vt_asm_fill(vt_cell *c, long n, const vt_cell *proto);
void vt_asm_rows_up(void **p, long k);
void vt_asm_rows_down(void **p, long k);

static int asm_check(void)
{
    static vt_cell c[12];
    vt_cell p;
    int i;
    for (i = 0; i < 12; i++) { c[i].ch = '.'; c[i].fg = 1; c[i].bg = 2; c[i].attr = 0; c[i].width = 1; c[i].deco = 0; c[i].ext = 0; c[i].pad = 0; }
    c[6].width = 2; c[7].width = 0; /* a wide glyph in cells 6 and 7 */
    p.fg = 0x11223344UL; p.bg = 0x55667788UL; p.ch = 0; p.attr = 0xA5C3; p.width = 1; p.deco = 9; p.ext = 7; p.pad = 0;
    if (vt_asm_put_run(c, (const vt_u8 *)"abcdefgh", 8, &p) != 6) return 1;   /* stops before the wide glyph */
    for (i = 0; i < 6; i++)
        if (c[i].ch != (vt_u16)('a' + i) || c[i].fg != p.fg || c[i].bg != p.bg || c[i].attr != p.attr ||
            c[i].width != 1 || c[i].deco != 9 || c[i].ext != 7) return 2;
    if (c[6].ch != '.' || c[6].width != 2 || c[6].fg != 1 || c[7].width != 0 || c[8].ch != '.') return 3;
    if (vt_asm_put_run(c + 8, (const vt_u8 *)"xy", 2, &p) != 2 || c[9].ch != 'y' || c[10].ch != '.') return 4;
    if (vt_asm_put_run(c + 4, (const vt_u8 *)"zzz", 3, &p) != 2 || c[5].ch != 'z' || c[6].width != 2) return 5;
    c[5].width = 1; c[6].width = 0;                                             /* a second half as the neighbour */
    if (vt_asm_put_run(c + 4, (const vt_u8 *)"kk", 2, &p) != 1 || c[4].ch != 'k' || c[5].ch != 'z') return 7;
    if (vt_asm_put_run(c, (const vt_u8 *)"q", 0, &p) != 0 || c[0].ch != 'a') return 6;
    if (vt_asm_put_run(c, (const vt_u8 *)"A~\033B", 4, &p) != 2 || c[1].ch != '~' || c[2].ch != 'c') return 8;  /* ESC ends the run */
    if (vt_asm_put_run(c, (const vt_u8 *)"\177", 1, &p) != 0 || vt_asm_put_run(c, (const vt_u8 *)"\200", 1, &p) != 0 ||
        vt_asm_put_run(c, (const vt_u8 *)"\037", 1, &p) != 0 || vt_asm_put_run(c, (const vt_u8 *)" ", 1, &p) != 1 || c[0].ch != ' ') return 9;
    for (i = 0; i < 12; i++) c[i].ch = (vt_u16)i;
    p.ch = 'F';
    vt_asm_fill(c + 2, 5, &p);
    for (i = 0; i < 12; i++)
        if (i >= 2 && i < 7 ? (c[i].ch != 'F' || c[i].fg != p.fg || c[i].bg != p.bg || c[i].attr != p.attr || c[i].ext != 7)
                            : c[i].ch != (vt_u16)i) return 10;
    vt_asm_fill(c, 0, &p);
    if (c[0].ch != 0) return 11;
    {
        static void *r[6];
        static char m[6];
        for (i = 0; i < 6; i++) r[i] = &m[i];
        vt_asm_rows_up(r + 1, 3);       /* 0 2 3 4 4 5 */
        if (r[0] != &m[0] || r[1] != &m[2] || r[2] != &m[3] || r[3] != &m[4] || r[4] != &m[4] || r[5] != &m[5]) return 12;
        for (i = 0; i < 6; i++) r[i] = &m[i];
        vt_asm_rows_down(r + 4, 3);     /* 0 1 1 2 3 5 */
        if (r[0] != &m[0] || r[1] != &m[1] || r[2] != &m[1] || r[3] != &m[2] || r[4] != &m[3] || r[5] != &m[5]) return 13;
        vt_asm_rows_up(r, 0);
        vt_asm_rows_down(r + 5, 0);
        if (r[0] != &m[0] || r[5] != &m[5]) return 14;
    }
    return 0;
}

int main(int argc, char **argv)
{
    static const char *const name[] = { "plain lines", "newlines", "colour a char", "256 pair a cell", "frame repaint", "ins/del line" };
    static vt_callbacks cb;
    int reps = argc > 2 ? atoi(argv[2]) : 3, w, r, pers;
    cb.damage = damage;
    cb.scroll = scroll;
    w = asm_check();
    Printf((STRPTR)"asm: %s (%ld)\n", (LONG)(w ? "WRONG" : "ok"), (LONG)w);
    if (w)
        return 20;
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
