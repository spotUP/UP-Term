/* desk_expect: host helper of the P1a desktop rig. Prints to stdout the bytes desk_core makes for a theme file, so the
 * rig compares the Amiga's ENV:Sys files against the pure writers instead of re-implementing them in Python.
 *   desk_expect <theme> palette <ncolors>
 *   desk_expect <theme> wbpattern <depth>              (tiled, no previous file)
 *   desk_expect <theme> pointer <basefile>
 *   desk_expect <theme> font <basefile|-> <fontsdir>   (- = desk_font_default; a font counts as present when <fontsdir>/<name> exists) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../amarchy/desk_core.h"

/* gthemes_core.c names this table in gt_save_text, which this tool never calls; the stub keeps prefs out of the link */
char prefs_paths_amarchy[256];

static unsigned char buf[16384];

static long slurp(const char *p, unsigned char *b, long cap)
{
    FILE *f = fopen(p, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(b, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

static int exists(void *ctx, const char *name)
{
    char p[512];
    FILE *f;
    sprintf(p, "%s/%s", (const char *)ctx, name);
    f = fopen(p, "rb");
    if (f)
        fclose(f);
    return f != 0;
}

int main(int argc, char **argv)
{
    static char text[8192];
    gtheme th;
    long n = 0, tl;
    if (argc >= 3 && !strcmp(argv[2], "check")) {
        char why[96];
        tl = slurp(argv[1], (unsigned char *)text, sizeof text - 1);
        if (tl < 0)
            return 2;
        gt_parse(text, tl, &th);
        if (desk_check(&th, why, (int)sizeof why) < 0) {
            printf("%s\n", why);
            return 1;
        }
        return 0;
    }
    if (argc < 4)
        return 2;
    tl = slurp(argv[1], (unsigned char *)text, sizeof text - 1);
    if (tl < 0)
        return 2;
    gt_parse(text, tl, &th);
    if (!strcmp(argv[2], "palette"))
        n = desk_palette(&th, atoi(argv[3]), buf, sizeof buf);
    else if (!strcmp(argv[2], "wbpattern"))
        n = desk_wbpattern(&th, atoi(argv[3]), 1, 0, 0, buf, sizeof buf);
    else if (!strcmp(argv[2], "pointer")) {
        n = slurp(argv[3], buf, sizeof buf);
        if (n < 0 || desk_pointer(&th, buf, n) < 0)
            return 3;
    } else if (!strcmp(argv[2], "font") && argc > 4) {
        n = strcmp(argv[3], "-") ? slurp(argv[3], buf, sizeof buf) : desk_font_default(buf, sizeof buf);
        if (n < 0 || desk_font_pens(buf, n) < 0 || desk_font_name(&th, buf, n, exists, argv[4]) < 0)
            return 3;
    } else
        return 2;
    if (n <= 0)
        return 3;
    fwrite(buf, 1, (size_t)n, stdout);
    return 0;
}
