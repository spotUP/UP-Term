/* menu_tree.c -- the node paths of the [menu] table: a node's entries, a child's path and the
 * OSD size. Its own file so the Amarchy commodity links only these; amarchy-menu links it
 * with menu_core.c. */
#include <string.h>
#include "menu_core.h"

static void cpy(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}

int mn_kind_needs_terminal(int kind)
{
    return kind == KA_TAB || kind == KA_MENUID || kind == KA_SLASH;
}

int mn_row_visible(int kind, int term)
{
    return term || !mn_kind_needs_terminal(kind);
}

/* 1 when some [menu] row whose id starts with prefix (a group's path and its dot) is visible */
static int group_visible(const keymap *k, const char *prefix, int plen, int term)
{
    int i;
    for (i = 0; i < k->nmenu; i++)
        if (!strncmp(k->menu[i].id, prefix, plen) && mn_row_visible(k->menu[i].kind, term))
            return 1;
    return 0;
}

int mn_children(const keymap *k, const char *node, mn_item *out, int max)
{
    return mn_children_t(k, node, out, max, 1);
}

int mn_children_t(const keymap *k, const char *node, mn_item *out, int max, int term)
{
    int i, j, n = 0, nl = (int)strlen(node);
    for (i = 0; i < k->nmenu && n < max; i++) {
        const char *id = k->menu[i].id, *rest, *dot;
        int seglen, dup = 0;
        if (nl) {
            if (strncmp(id, node, nl) || id[nl] != '.')
                continue;
            rest = id + nl + 1;
        } else
            rest = id;
        if (!*rest)
            continue;
        dot = strchr(rest, '.');
        seglen = dot ? (int)(dot - rest) : (int)strlen(rest);
        if (seglen >= KM_ID)
            continue;
        if (dot) {
            if (!group_visible(k, id, (int)(dot - id) + 1, term))
                continue;
            for (j = 0; j < n; j++)
                if (out[j].row == MN_ROW_GROUP && !strncmp(out[j].seg, rest, seglen) && !out[j].seg[seglen])
                    dup = 1;
            if (dup)
                continue;
            out[n].row = MN_ROW_GROUP;
            out[n].ix = -1;
            memcpy(out[n].seg, rest, seglen);
            out[n].seg[seglen] = 0;
            out[n].label[0] = (char)((rest[0] >= 'a' && rest[0] <= 'z') ? rest[0] - 32 : rest[0]);
            cpy(out[n].label + 1, rest + 1, seglen < KM_LABEL ? seglen : KM_LABEL);
        } else {
            if (!mn_row_visible(k->menu[i].kind, term))
                continue;
            out[n].row = i;
            out[n].ix = -1;
            cpy(out[n].seg, rest, KM_ID);
            cpy(out[n].label, k->menu[i].label, KM_LABEL);
        }
        n++;
    }
    return n;
}

void mn_child_node(const char *node, const mn_item *it, char *out, int n)
{
    if (*node) {
        cpy(out, node, n);
        strncat(out, ".", n - strlen(out) - 1);
        strncat(out, it->seg, n - strlen(out) - 1);
    } else
        cpy(out, it->seg, n);
}

/* `node` and every group below it, each level once, through fn: each id under node names its groups
 * by its prefixes up to a dot. Not recursive, one item buffer: the commodity calls this on its own
 * small stack. A group whose segment mn_children would skip (KM_ID or longer) is not reachable and is
 * left out, as there. A level shared by two ids is visited once per id; the callers keep maxima. */
void mn_levels_t(const keymap *k, const char *node, mn_level_fn fn, void *ctx, int term)
{
    mn_item it[MN_MAX_ITEMS];
    char g[KM_ID * 2];
    int i, nl = (int)strlen(node);
    fn(it, mn_children_t(k, node, it, MN_MAX_ITEMS, term), ctx);
    for (i = 0; i < k->nmenu; i++) {
        const char *id = k->menu[i].id, *seg, *dot;
        if (nl && (strncmp(id, node, nl) || id[nl] != '.'))
            continue;
        for (seg = id + (nl ? nl + 1 : 0); (dot = strchr(seg, '.')) != NULL; seg = dot + 1) {
            if (dot - seg >= KM_ID || dot - id >= (int)sizeof g)
                break;
            memcpy(g, id, dot - id);
            g[dot - id] = 0;
            fn(it, mn_children_t(k, g, it, MN_MAX_ITEMS, term), ctx);
        }
    }
}

void mn_levels(const keymap *k, const char *node, mn_level_fn fn, void *ctx)
{
    mn_levels_t(k, node, fn, ctx, 1);
}

typedef struct osd_acc { int maxn, maxw; } osd_acc;

/* the most entries and the widest text of one level */
static void osd_level(const mn_item *it, int n, void *ctx)
{
    osd_acc *a = (osd_acc *)ctx;
    int i;
    if (n > a->maxn)
        a->maxn = n;
    for (i = 0; i < n; i++) {
        int w = (int)strlen(it[i].label) + 2 + (MN_IS_GROUP(&it[i]) ? 1 : 0);
        if (w > a->maxw)
            a->maxw = w;
    }
}

void mn_osd_size_t(const keymap *k, const char *node, int *cols, int *rows, int term)
{
    osd_acc a;
    int c;
    a.maxn = a.maxw = 0;
    mn_levels_t(k, node, osd_level, &a, term);
    c = a.maxw + 2 * MN_OSD_PAD;
    *cols = c < MN_OSD_MIN_COLS ? MN_OSD_MIN_COLS : c;
    *rows = MN_OSD_HEAD_LINES + a.maxn + MN_OSD_BOTTOM_LINES;
}

void mn_osd_size(const keymap *k, const char *node, int *cols, int *rows)
{
    mn_osd_size_t(k, node, cols, rows, 1);
}
