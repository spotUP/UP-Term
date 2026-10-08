/* ixargv ... -- the argv an ixemul program gets from its AmigaDOS argument
 * line (ixemul-vtcon library/_cli_parse.c), in readitem's notation: one
 * line per argument, bytes outside 33..126 and < > as <hex>.
 * tools/rig/ixargv_rig.py compares it with what was meant. With IXARGV_OUT
 * set it writes to that file (a command AmigaDOS Run starts has no output
 * the rig sees). */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int i;
    const char *out = getenv("IXARGV_OUT");
    if (out && !freopen(out, "w", stdout))
        return 20;
    printf("argc %d\n", argc - 1);
    for (i = 1; i < argc; i++) {
        const unsigned char *s = (const unsigned char *)argv[i];
        printf("arg [");
        for (; *s; s++)
            if (*s > 32 && *s < 127 && *s != '<' && *s != '>')
                putchar(*s);
            else
                printf("<%02x>", *s);
        printf("]\n");
    }
    return 0;
}
