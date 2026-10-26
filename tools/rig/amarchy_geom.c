/* amarchy_geom -- the numbers amarchy-gui's window must have, from the same code (menu_geom.c, menu_core.c,
 * menu_icon.c, the default keymap) built for the host: tools/rig/amarchy_gui_rig.py compares the rig's window
 * with them (G7).
 *     amarchy_geom [--wb] [--term] SCREEN_W SCREEN_H FONT_YSIZE FONT_XSIZE RES_X RES_Y [label=pixels ...]
 * --wb: the WORKBENCH port exists (OS 3.2 rig): the workbench group shows; the levels printed are root, apps, screens, workbench.
 * --term: an UP-Term window is under the menu (all rows); default is over Workbench, where tab:, menuid: and
 * slash: rows are hidden (mn_children_t / mg_width_t, as gui.c's term_present).
 * (RES_X, RES_Y: the screen mode's DisplayInfo Resolution ticks of one pixel, 0 0 = square)
 * prints "need <label>" for every text the font must measure and has no label=pixels yet (the width then
 * assumes a fixed-width font: XSIZE per character), "icon <width> <height>", and per level
 * "level <node> <window x> <y> <w> <h> <entries> <visible>" and "cell <node> <icon x> <icon y> <width> <height> <row 2 icon y>" (window-relative). Same order as gui.c: mg_metrics_for ->
 * mi_size/mi_width -> mg_width -> mn_children -> mg_layout. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../amarchy/menu_core.h"
#include "../../amarchy/menu_geom.h"
#include "../../amarchy/menu_icon.h"
#include "../../config/keymap.h"
#include "../../render/unifont.h"
#include "keymap_default.inc"

static char *tab_s[256];
static int tab_n[256];
static int ntab, fixed_cw;

static int text_len(const char *s, void *ctx)
{
    static char seen[256][96];
    static int nseen;
    int i;
    (void)ctx;
    for (i = 0; i < ntab; i++)
        if (!strcmp(tab_s[i], s))
            return tab_n[i];
    for (i = 0; i < nseen; i++)
        if (!strcmp(seen[i], s))
            return (int)strlen(s) * fixed_cw;
    if (nseen < 256 && strlen(s) < 96) {
        strcpy(seen[nseen++], s);
        printf("need %s\n", s);
    }
    return (int)strlen(s) * fixed_cw;
}

/* the emoji pages are not loaded on the host */
const vt_u8 *uf_page(uf_cache *c, int page, long *len)
{
    (void)c; (void)page; (void)len;
    return 0;
}

int main(int argc, char **argv)
{
    static keymap km;
    static mn_item it[MN_MAX_ITEMS];
    const char *nodes[4] = { "", "apps", "screens", "workbench" };
    int cond0 = 0;
    mg_font f;
    mg_screen s;
    mg_metrics m;
    int icon_w, icon_h, chev_w, w, i, term = 0;
    if (argc > 1 && !strcmp(argv[1], "--term")) {
        term = 1;
        argv++;
        argc--;
    }
    if (argc > 1 && !strcmp(argv[1], "--wb")) {      /* the rig runs OS 3.2: the WORKBENCH port exists, the Workbench group shows */
        cond0 = MN_COND_WB;
        argv++;
        argc--;
    }
    if (argc < 7)
        return 2;
    s.w = atoi(argv[1]);
    s.h = atoi(argv[2]);
    f.ysize = atoi(argv[3]);
    f.cw = fixed_cw = atoi(argv[4]);
    s.rx = atoi(argv[5]);
    s.ry = atoi(argv[6]);
    for (i = 7; i < argc && ntab < 256; i++) {
        char *eq = strrchr(argv[i], '=');
        if (!eq)
            return 2;
        *eq = 0;
        tab_s[ntab] = argv[i];
        tab_n[ntab++] = atoi(eq + 1);
    }
    keymap_parse(&km, km_default_text, (long)strlen(km_default_text));
    mg_metrics_for(&m, &f, &s);
    icon_h = mi_count() > 0 ? mi_size(&m) : 0;
    icon_w = mi_count() > 0 ? mi_width(&m, &s) : 0;
    chev_w = text_len(">", 0);
    printf("icon %d %d\n", icon_w, icon_h);
    for (i = 0; i < 4; i++) {
        mg_geom g;
        int n = mn_children_t(&km, nodes[i], it, MN_MAX_ITEMS, term | cond0);
        if (!nodes[i][0]) {
            int j;
            for (j = 0; j < n; j++)
                printf("row %s %d\n", it[j].seg, j);
        }
        w = mg_width_t(&km, nodes[i], &f, icon_w, chev_w, text_len, 0, &s, term | cond0);
        mg_layout(&g, &f, w, n, &s);
        printf("level %s %d %d %d %d %d %d\n", nodes[i][0] ? nodes[i] : "root", g.x, g.y, g.w, g.h, g.n, g.visible);
        /* the first row's icon cell (draw_row/draw_icon): window-relative x, y, side; the second row's y */
        printf("cell %s %d %d %d %d %d\n", nodes[i][0] ? nodes[i] : "root", g.list_x + g.m.row_pad,
               mg_row_y(&g, 0) + (g.m.row_h - icon_h) / 2, icon_w, icon_h,
               mg_row_y(&g, 1) + (g.m.row_h - icon_h) / 2);
    }
    return 0;
}
