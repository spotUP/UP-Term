/* ixargv ... -- the argv an ixemul program gets from its AmigaDOS argument
 * line (ixemul-vtcon library/_cli_parse.c), in readitem's notation: one
 * line per argument, bytes outside 33..126 and < > as <hex>.
 * tools/rig/ixargv_rig.py compares it with what was meant. */
#include <stdio.h>

int main(int argc, char **argv)
{
    int i;
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
