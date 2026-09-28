/* reach -- the one reachability test (ledger V3), run on the rig by
 * tools/rig/reach.py. Opens an XCON: window through DOS, switches it raw,
 * and asks three things only the vtcon engine answers this way:
 *   ESC [ c        DA1        -> ESC [ ? 62 ; 22 c   (the ROM console does not answer DA)
 *   $9B SP q       Amiga size -> $9B 1;1;rows;cols SP r  (8-bit CSI in the xterm personality)
 *   ESC [ 6 n      DSR 6      -> ESC [ 3 ; 5 R after a move to row 3, column 5
 * and prints what came back, hex-escaped, one line per question.
 * Exit 0 only when all three match. */
#include <stdio.h>
#include <string.h>
#include <proto/dos.h>

static int ask(BPTR fh, const char *q, long qlen, char *out, int max)
{
    long n = 0;
    Write(fh, (APTR)q, qlen);
    while (n < max - 1 && WaitForChar(fh, 1000000)) {
        long r = Read(fh, out + n, 1);
        if (r <= 0)
            break;
        n += r;
        if (out[n - 1] == 'c' || out[n - 1] == 'r' || out[n - 1] == 'R')
            break;
    }
    out[n] = 0;
    return (int)n;
}

static void show(const char *label, const char *s, int n)
{
    int i;
    printf("%s ", label);
    for (i = 0; i < n; i++) {
        unsigned char b = (unsigned char)s[i];
        if (b < 0x20 || b >= 0x7F)
            printf("<%02x>", b);
        else
            putchar(b);
    }
    printf("\n");
}

int main(void)
{
    char buf[64];
    int n, ok = 1;
    BPTR fh = Open((STRPTR)"XCON:0/20/400/160/reach", MODE_OLDFILE);
    if (!fh) {
        printf("FAIL open\n");
        return 20;
    }
    SetMode(fh, 1);
    n = ask(fh, "\033[c", 3, buf, sizeof(buf));
    show("DA1", buf, n);
    ok &= n == 9 && !memcmp(buf, "\033[?62;22c", 9);
    n = ask(fh, "\x9b q", 3, buf, sizeof(buf));
    show("SIZE", buf, n);
    ok &= n > 8 && (unsigned char)buf[0] == 0x9B && !memcmp(buf + 1, "1;1;", 4) &&
          !memcmp(buf + n - 2, " r", 2);
    n = ask(fh, "\033[3;5H\033[6n", 10, buf, sizeof(buf));
    show("DSR", buf, n);
    ok &= n == 6 && !memcmp(buf, "\033[3;5R", 6);
    SetMode(fh, 0);
    Close(fh);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 10;
}
