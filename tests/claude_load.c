/* see claude_load.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claude_load.h"

char *claude_load(const char *name, long *n)
{
    char path[256];
    FILE *f;
    char *b;
    long len;
    strcpy(path, "tests/claude/");
    strcat(path, name);
    f = fopen(path, "rb");
    if (!f) {
        printf("  [ERROR] cannot open %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (char *)malloc((size_t)len + 1);
    if (b && (long)fread(b, 1, (size_t)len, f) != len) {
        free(b);
        b = 0;
    }
    fclose(f);
    if (b)
        b[len] = 0;
    *n = len;
    return b;
}
