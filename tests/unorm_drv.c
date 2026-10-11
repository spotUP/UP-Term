/* unorm_drv -- claude/unorm.c on standard input for tests/test_unorm.py:
 * each line is a sequence of code points in hex (space separated); the
 * line written back is un_nfkc of their UTF-8, as hex code points. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claude/unorm.h"
#include "claude/json.h"

static char line[1 << 16];
static char u8[1 << 16];

int main(void)
{
    while (fgets(line, sizeof(line), stdin)) {
        char *p = line, *e, *o = 0;
        long n = 0, on = 0, i;
        for (;;) {
            unsigned long c = strtoul(p, &e, 16);
            if (e == p)
                break;
            p = e;
            if (c < 0x80)
                u8[n++] = (char)c;
            else if (c < 0x800) {
                u8[n++] = (char)(0xC0 | (c >> 6));
                u8[n++] = (char)(0x80 | (c & 0x3F));
            } else if (c < 0x10000) {
                u8[n++] = (char)(0xE0 | (c >> 12));
                u8[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
                u8[n++] = (char)(0x80 | (c & 0x3F));
            } else {
                u8[n++] = (char)(0xF0 | (c >> 18));
                u8[n++] = (char)(0x80 | ((c >> 12) & 0x3F));
                u8[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
                u8[n++] = (char)(0x80 | (c & 0x3F));
            }
        }
        if (un_nfkc(u8, n, &o, &on))
            return 1;
        for (i = 0; i < on;) {
            unsigned long c;
            int k = (unsigned char)o[i] < 0x80 ? 1 : json_utf8(o + i, on - i, &c);
            if ((unsigned char)o[i] < 0x80)
                c = (unsigned char)o[i];
            if (k <= 0)
                return 2;
            printf(i ? " %lX" : "%lX", c);
            i += k;
        }
        putchar('\n');
        free(o);
    }
    return 0;
}
