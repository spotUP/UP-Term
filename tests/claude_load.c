/* see claude_load.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claude_load.h"
#include "harness.h"

int claude_load_errors;

static char *load(const char *name, long *n, int loud)
{
    char path[256];
    FILE *f;
    char *b;
    long len;
    strcpy(path, "tests/claude/");
    strcat(path, name);
    f = fopen(path, "rb");
    if (!f) {
        if (loud) {
            printf("  [ERROR] cannot open %s\n", path);
            claude_load_errors++;
        }
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

char *claude_load(const char *name, long *n)
{
    return load(name, n, 1);
}

char *claude_load_opt(const char *name, long *n)
{
    return load(name, n, 0);
}

int claude_golden(const char *name, const char *got)
{
    long n;
    char *want;
    int ok;
    if (getenv("CL_UPDATE_GOLDEN")) {
        char p[256];
        FILE *f;
        strcpy(p, "tests/claude/");
        strcat(p, name);
        f = fopen(p, "wb");
        if (f) {
            fputs(got, f);
            fclose(f);
        }
    }
    want = claude_load(name, &n);
    CHECK(want != 0);
    ok = want && !strcmp(got, want);
    if (want)
        CHECK_STR(got, want);
    free(want);
    return ok;
}
