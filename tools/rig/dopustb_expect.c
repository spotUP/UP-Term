/* dopustb_expect: host helper of the DOpus toolbar rig (DT-6). Prints what dopustb_core makes for a theme file and a
 * screen, so the rig compares the Amiga's bank path and brush CMAP against the pure functions, not a Python copy.
 *   dopustb_expect <theme> <font_h> <rx> <ry> [icon-style]  ->  "set <name>" and "cmap <48 hex digits>" lines
 * icon-style "outline" picks the Lucide family; anything else or absent picks filled, the default (DT-9). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../amarchy/dopustb_core.h"

/* gthemes_core.c names this table in gt_save_text, which this tool never calls; the stub keeps prefs out of the link */
const prefs_paths prefs_paths_amarchy;

static char buf[65536];

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    gtheme t;
    dt_set s;
    char name[DT_SET_NAME_MAX];
    uint8_t rgb[DT_PENS][3];
    int i;
    if ((argc != 5 && argc != 6) || !(f = fopen(argv[1], "rb")))
        return 2;
    n = (long)fread(buf, 1, sizeof buf, f);
    fclose(f);
    gt_parse(buf, n, &t);
    dt_pick_set((unsigned)atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), argc == 6 && !strcmp(argv[5], "outline"), &s);
    dt_set_name(&s, name, sizeof name);
    dt_cmap(&t, rgb);
    printf("set %s\ncmap ", name);
    for (i = 0; i < DT_PENS * 3; i++)
        printf("%02x", rgb[i / 3][i % 3]);
    printf("\n");
    return 0;
}
