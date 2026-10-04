/* wprobe: where a write's time goes (ledger S1) -- conbench's cursor-pos,
 * insdel-char and bytewise shapes, each as conbench sends it (one Write()
 * an item) and as one Write() of the same bytes, with a barrier
 * (WaitForChar) after each. Ticks of 1/50 s, to stdout: wprobe >RAM:w.txt */
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

static char buf[40000];
static long len[2500], n, items;
static BPTR con;

static long now(void) { struct DateStamp ds; DateStamp(&ds); return ds.ds_Minute * 3000L + ds.ds_Tick; }
static void s(const char *t) { while (*t) buf[n++] = *t++; }
static void num(int v) { if (v >= 10) buf[n++] = (char)('0' + v / 10); buf[n++] = (char)('0' + v % 10); }

static void build(int w)
{
    int i;
    long at;
    n = 0;
    items = w == 6 ? 2400 : w >= 4 ? 200 : 700;
    for (i = 1; i <= items; i++) {
        at = n;
        switch (w) {
        case 0: s("\033["); num(i % 20 + 1); s(";"); num(i % 60 + 1); s("H-cursor-"); break;
        case 1: s("\033["); num(i % 20 + 1); s(";"); num(i % 60 + 1); s("H"); break;       /* the move alone */
        case 2: s("\r-cursor-abcdefg"); break;                                             /* no escape */
        case 3: s("\033["); num(i % 20 + 1); s(";1H-cursor-"); break;                      /* always column 1 */
        case 4: s("\033["); num(i % 18 + 2); s(";5Habcdefgh\033["); num(i % 18 + 2); s(";5H\033[4@\033[4P"); break;
        case 5: s("\033["); num(i % 18 + 2); s(";5Habcdefgh"); break;                      /* insdel-char without the shifts */
        default: buf[n++] = (char)('a' + i % 26); break;
        }
        len[i - 1] = n - at;
    }
}

int main(void)
{
    static const char *const name[] = { "cursor-pos", "the move alone", "CR + text, no escape", "move to column 1 + text",
                                        "insdel-char", "its text, no shifts", "bytewise" };
    long t[7][2];
    int w, i;
    con = Open((STRPTR)"*", MODE_OLDFILE);
    if (!con)
        return 20;
    for (w = 0; w < 7; w++) {
        long t0, at = 0;
        build(w);
        Write(con, (APTR)"\033[H\033[J", 6);
        WaitForChar(con, 0);
        t0 = now();
        for (i = 0; i < items; i++) { Write(con, buf + at, len[i]); at += len[i]; }
        WaitForChar(con, 0);
        t[w][0] = now() - t0;
        Write(con, (APTR)"\033[H\033[J", 6);
        WaitForChar(con, 0);
        t0 = now();
        Write(con, buf, n);
        WaitForChar(con, 0);
        t[w][1] = now() - t0;
    }
    Write(con, (APTR)"\033[H\033[J", 6);
    Close(con);
    for (w = 0; w < 7; w++)
        Printf((STRPTR)"%-26s a write each %4ld ticks   one write %4ld ticks\n", (LONG)name[w], t[w][0], t[w][1]);
    return 0;
}
