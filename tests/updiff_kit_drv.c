/* updiff_kit_drv -- a kit's own manifest through ud_compare (install/updiff.c),
 * old and new the same file: what upupdate does when the owner runs Install
 * over the same kit. dist-check runs it on every archive's Files/MANIFEST, so a
 * kit the parser cannot read (more handed# lines or parts than its fixed
 * limits) fails the build instead of turning the owner's Update page into a
 * full install ("is not a kit manifest").
 *
 *   updiff_kit_drv <kit>/Files/MANIFEST
 *
 * Exit 0 when the manifest compares clean: no file new, changed or gone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "install/updiff.h"

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *t;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    t = (char *)malloc((size_t)n + 1);
    if (t && fread(t, 1, (size_t)n, f) != (size_t)n) {
        free(t);
        t = 0;
    }
    if (t) t[n] = 0;
    fclose(f);
    return t;
}

static void none(void *ctx, const char *part, const char *line)
{
    (void)ctx;
    (void)part;
    (void)line;
}

int main(int argc, char **argv)
{
    char *a, *b;
    ud_result *r;
    int rc;
    if (argc != 2) {
        fprintf(stderr, "usage: updiff_kit_drv <kit>/Files/MANIFEST\n");
        return 2;
    }
    a = slurp(argv[1]);
    b = slurp(argv[1]);
    r = (ud_result *)malloc(sizeof(ud_result));
    if (!a || !b || !r) {
        fprintf(stderr, "updiff_kit_drv: cannot read %s\n", argv[1]);
        return 1;
    }
    rc = ud_compare(a, b, "VTC:distkit", r, none, 0);
    if (rc != 0) {
        fprintf(stderr, "updiff_kit_drv: upupdate cannot read %s (not a kit manifest to install/updiff.c: raise its limits)\n", argv[1]);
        return 1;
    }
    if (r->files || r->gone) {
        fprintf(stderr, "updiff_kit_drv: %s against itself: %d new or changed, %d gone\n", argv[1], r->files, r->gone);
        return 1;
    }
    printf("updiff_kit_drv: %s reads clean (%d parts, %d handed)\n", argv[1], r->nparts, r->nhanded);
    free(a);
    free(b);
    free(r);
    return 0;
}
