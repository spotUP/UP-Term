/* desk_expect: host helper of the P1a desktop rig. Prints to stdout the bytes desk_core makes for a theme file, so the
 * rig compares the Amiga's ENV:Sys files against the pure writers instead of re-implementing them in Python.
 *   desk_expect <theme> palette <ncolors>
 *   desk_expect <theme> wbpattern <depth>              (tiled, no previous file)
 *   desk_expect <theme> pointer <basefile>
 *   desk_expect <theme> font <basefile|-> <fontsdir>   (- = desk_font_default; a font counts as present when <fontsdir>/<name> exists)
 *   desk_expect - pens                                 (desk_pens slots 0..11, "pens: n n ..." like dripens; slot 11 as 2, R1.17 pen 11)
 *   desk_expect - fields <palette|wbpattern|pointer|font> <file> [ncolors]
 *       the THEMED FIELDS of a prefs file as "name value" lines (the stock editors rewrite ENV:Sys files in their own layout
 *       after USE, so the rig compares these fields of the expected bytes against those of the settled file) */
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

static unsigned g16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static unsigned long g32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static void hexline(const char *name, const unsigned char *p, long n)
{
    long i;
    printf("%s ", name);
    for (i = 0; i < n; i++)
        printf("%02x", p[i]);
    printf("\n");
}

/* walks the chunks of an IFF PREF file; returns the next chunk's body or 0 */
static const unsigned char *chunk(const unsigned char *b, long len, long *pos, char *id, long *size)
{
    long at = *pos;
    if (at + 8 > len)
        return 0;
    memcpy(id, b + at, 4);
    id[4] = 0;
    *size = (long)g32(b + at + 4);
    if (at + 8 + *size > len)
        return 0;
    *pos = at + 8 + *size + (*size & 1);
    return b + at + 8;
}

static int fields(const char *part, const unsigned char *b, long len, int ncol)
{
    long pos = 12, size, wb = 0;
    char id[5], name[40];
    const unsigned char *body;
    int i, found = 0;
    if (len < 12 || memcmp(b, "FORM", 4) || memcmp(b + 8, "PREF", 4))
        return 3;
    while ((body = chunk(b, len, &pos, id, &size)) != 0) {
        if (!strcmp(part, "palette") && !strcmp(id, "PALT") && size >= 400) {
            for (i = 0; i < DK_SLOTS; i++) {
                printf("pen4[%d] %u\n", i, g16(body + 16 + 2 * i));
                printf("pen8[%d] %u\n", i, g16(body + 80 + 2 * i));
            }
            for (i = 0; i < ncol; i++)
                printf("color[%d] idx=%u %04x %04x %04x\n", i, g16(body + 144 + 8 * i), g16(body + 146 + 8 * i),
                       g16(body + 148 + 8 * i), g16(body + 150 + 8 * i));
            found++;
        } else if (!strcmp(part, "wbpattern") && !strcmp(id, "PTRN") && size >= 24) {
            unsigned which = g16(body + 16), dl = g16(body + 22);
            if (which > 1)
                continue; /* SCREEN is the user's own */
            sprintf(name, "ptrn%u", which);
            printf("%s.flags %u\n%s.depth %u\n%s.datalen %u\n", name, g16(body + 18), name, body[21], name, dl);
            if (24 + (long)dl <= size) {
                strcat(name, ".data");
                hexline(name, body + 24, dl);
            }
            found++;
        } else if (!strcmp(part, "pointer") && !strcmp(id, "PNTR") && size >= 32) {
            unsigned depth = g16(body + 24), nc = (1u << depth) - 1;
            if (depth > 8 || size < 32 + 3L * (long)nc)
                continue;
            sprintf(name, "pntr%ld.rgb", wb);
            hexline(name, body + 32, 3L * (long)nc);
            wb++;
            found++;
        } else if (!strcmp(part, "font") && !strcmp(id, "FONT") && size >= 156 && g16(body + 14) == 0) {
            printf("wbfont.frontpen %u\nwbfont.backpen %u\nwbfont.drawmode %u\nwbfont.specialdrawmode %u\n", body[16],
                   body[17], body[18], body[19]);
            printf("wbfont.name %.127s\nwbfont.ysize %u\n", (const char *)body + 28, g16(body + 24));
            found++;
        }
    }
    return found ? 0 : 3;
}

int main(int argc, char **argv)
{
    static char text[8192];
    gtheme th;
    long n = 0, tl;
    if (argc >= 3 && !strcmp(argv[2], "pens")) {
        unsigned char want[DK_SLOTS];
        int i;
        desk_pens(want);
        printf("pens:");
        for (i = 0; i < 12; i++)
            printf(" %d", i == 11 ? 2 : want[i]);
        printf("\n");
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[2], "fields")) {
        n = slurp(argv[4], buf, sizeof buf);
        if (n < 0)
            return 2;
        return fields(argv[3], buf, n, argc > 5 ? atoi(argv[5]) : 8);
    }
    if (argc >= 3 && !strcmp(argv[2], "check")) {
        char why[96];
        tl = slurp(argv[1], (unsigned char *)text, sizeof text - 1);
        if (tl < 0)
            return 2;
        gt_parse(text, tl, &th);
        if (desk_check(&th, why, (int)sizeof why) != 0) {
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
