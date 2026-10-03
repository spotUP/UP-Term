/* cellbench: what a full screen of each kind of cell costs the terminal
 * (the renderer's hot paths, apart): 20 screens of 80 x 24 each, the time
 * in 1/50 s. Output to the console it runs in; the table to stdout
 * (redirect it: cellbench >RAM:cb.txt).
 *   A  text, one colour             B  blanks, one background a row
 *   C  blanks, a background a cell  D  half blocks, one colour pair
 *   E  half blocks, a pair a cell   F  text, a foreground a cell
 *   G  nothing drawn: 20 x the bytes of E inside an OSC string (the
 *      parser's cost for the same bytes) */
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

static char buf[65536];
static long n;

static void s(const char *t) { while (*t) buf[n++] = *t++; }
static void num(int v) { if (v >= 100) buf[n++] = (char)('0' + v / 100); if (v >= 10) buf[n++] = (char)('0' + v / 10 % 10); buf[n++] = (char)('0' + v % 10); }

static long now(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return ds.ds_Minute * 3000L + ds.ds_Tick;
}

int main(void)
{
    static const char *const name[] = { "A text, one colour", "B blanks, a bg a row", "C blanks, a bg a cell",
                                        "D half blocks, one pair", "E half blocks, a pair a cell",
                                        "F text, a fg a cell", "G the bytes of E, not drawn" };
    BPTR con = Open((STRPTR)"*", MODE_OLDFILE);
    int mode, f, x, y;
    long t[7];
    if (!con)
        return 20;
    Write(con, (APTR)"\033[?1049h\033[?25l", 14);
    for (mode = 0; mode < 7; mode++) {
        long t0 = now();
        for (f = 0; f < 20; f++) {
            n = 0;
            s(mode == 6 ? "\033]9999;" : "\033[H");
            for (y = 0; y < 24; y++) {
                if (mode == 1) { s("\033[48;5;"); num(17 + (y + f) % 200); s("m"); }
                if (mode == 3 && y == 0) { s("\033[38;5;"); num(20 + f); s(";48;5;"); num(120 + f); s("m"); }
                if (mode == 0 && y == 0) { s("\033[0;38;5;"); num(40 + f); s("m"); }
                for (x = 0; x < 80; x++) {
                    int c = 17 + (x + y * 7 + f * 3) % 200;
                    switch (mode) {
                    case 0: buf[n++] = (char)('a' + (x + f) % 26); break;
                    case 1: buf[n++] = ' '; break;
                    case 2: s("\033[48;5;"); num(c); s("m "); break;
                    case 3: s("\342\226\200"); break;
                    case 4: case 6:
                        if (mode == 4) { s("\033[38;5;"); num(c); s(";48;5;"); num(c + 30); s("m\342\226\200"); }
                        else { s("x[38;5;"); num(c); s(";48;5;"); num(c + 30); s("mxxx"); }
                        break;
                    default: s("\033[38;5;"); num(c); s("m"); buf[n++] = (char)('a' + x % 26); break;
                    }
                }
                if (y < 23 && mode != 6) s("\r\n");
            }
            if (mode == 6) s("\007");
            Write(con, buf, n);
        }
        Write(con, (APTR)"\033[0m\033[2J", 8);
        t[mode] = now() - t0;
    }
    Write(con, (APTR)"\033[?25h\033[?1049l", 14);
    Close(con);
    for (mode = 0; mode < 7; mode++)
        Printf((STRPTR)"%-30s %4ld ticks  %5ld cells/s\n", (LONG)name[mode], t[mode],
               t[mode] ? 20L * 1920 * 50 / t[mode] : 0L);
    return 0;
}
