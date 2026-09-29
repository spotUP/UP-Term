/* romprobe CASES DEVICE -- ask a console where the cursor is after each case.
 *
 * Opens DEVICE (for example "CON:0/12/656/216/probe"), raw mode, reads the
 * window size with the window status request, then for every line
 * "label<TAB>bytes" of CASES (C escapes: \e \xHH \n \r \t \b \\) sends
 * ESC c (reset), the bytes, CSI 6 n, and prints "label row;col" from the
 * cursor report ("label -" when none came). Run against the ROM CON: it
 * answers the matrix's open questions (Q1, Q2) with the real console;
 * tools/probe_compare.py runs the same cases through the engine. */
#include <stdio.h>
#include <string.h>
#include <proto/dos.h>

static int unescape(const char *s, char *o)
{
    int n = 0;
    while (*s && *s != '\n' && *s != '\r') {
        if (*s == '\\' && s[1]) {
            s++;
            switch (*s) {
            case 'e': o[n++] = 0x1B; s++; break;
            case 'n': o[n++] = '\n'; s++; break;
            case 'r': o[n++] = '\r'; s++; break;
            case 't': o[n++] = '\t'; s++; break;
            case 'b': o[n++] = '\b'; s++; break;
            case '\\': o[n++] = '\\'; s++; break;
            case 'x': {
                int v = 0, k;
                s++;
                for (k = 0; k < 2 && *s; k++, s++) {
                    char c = *s;
                    v = v * 16 + (c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0');
                }
                o[n++] = (char)v;
                break;
            }
            default: o[n++] = *s++; break;
            }
        } else {
            o[n++] = *s++;
        }
    }
    return n;
}

static int reply(BPTR fh, char *buf, int max, char final)
{
    int n = 0;
    while (n < max - 1 && WaitForChar(fh, 1500000)) {
        if (Read(fh, buf + n, 1) != 1)
            break;
        n++;
        if (buf[n - 1] == final)
            break;
    }
    buf[n] = 0;
    return n;
}

int main(int argc, char **argv)
{
    static char line[512], bytes[512], buf[64];
    FILE *f;
    BPTR fh;
    int n;
    if (argc < 3) {
        printf("usage: romprobe CASES DEVICE\n");
        return 20;
    }
    f = fopen(argv[1], "r");
    if (!f) {
        printf("cannot open %s\n", argv[1]);
        return 20;
    }
    fh = Open((STRPTR)argv[2], MODE_OLDFILE);
    if (!fh) {
        printf("cannot open %s\n", argv[2]);
        return 20;
    }
    SetMode(fh, 1);
    Write(fh, "\x9b" "0 q", 4);
    n = reply(fh, buf, sizeof(buf), 'r');
    printf("size %s\n", n > 5 ? buf + 5 : "-"); /* skip 9B "1;1;" */
    while (fgets(line, sizeof(line), f)) {
        char *tab = strchr(line, '\t');
        int len, i;
        if (line[0] == '#' || !tab)
            continue;
        *tab = 0;
        len = unescape(tab + 1, bytes);
        Write(fh, "\033c", 2);
        Write(fh, bytes, len);
        Write(fh, "\x9b" "6n", 3);
        n = reply(fh, buf, sizeof(buf), 'R');
        printf("%s ", line);
        if (n < 3) {
            printf("-\n");
            continue;
        }
        for (i = 0; i < n; i++) /* the report: 9B or ESC [ then row;col R */
            if (buf[i] >= '0' && buf[i] <= '9')
                break;
        buf[n - 1] = 0;
        printf("%s\n", buf + i);
    }
    SetMode(fh, 0);
    Close(fh);
    fclose(f);
    return 0;
}
