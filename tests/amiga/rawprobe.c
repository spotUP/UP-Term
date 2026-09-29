/* rawprobe DEVICE SEQUENCE SECONDS -- open DEVICE raw, write SEQUENCE (\e and
 * \xHH escapes), then print every byte that arrives within SECONDS, hex. For
 * raw input event reports (SEQUENCE "\x9b1{") and layout (\x9b10u). */
#include <stdio.h>
#include <stdlib.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
    char seq[128], c;
    int n = 0, secs;
    const char *s;
    BPTR fh;
    if (argc < 4)
        return 20;
    for (s = argv[2]; *s && n < 120; s++) {
        if (s[0] == '\\' && s[1] == 'e') { seq[n++] = 0x1B; s++; }
        else if (s[0] == '\\' && s[1] == 'x' && s[2] && s[3]) {
            int v; sscanf(s + 2, "%2x", &v); seq[n++] = (char)v; s += 3;
        } else seq[n++] = *s;
    }
    secs = atoi(argv[3]);
    fh = Open((STRPTR)argv[1], MODE_OLDFILE);
    if (!fh)
        return 20;
    SetMode(fh, 1);
    Write(fh, seq, n);
    while (WaitForChar(fh, (LONG)secs * 1000000)) {
        if (Read(fh, &c, 1) != 1)
            break;
        printf("%02x ", (unsigned char)c);
        fflush(stdout);
    }
    printf("\n");
    SetMode(fh, 0);
    Close(fh);
    return 0;
}
