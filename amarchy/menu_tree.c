/* menu_tree.c -- the node paths of the [menu] table: a node's entries, a child's path and the
 * OSD size. Its own file so the Amarchy commodity links only these; amarchy-menu links it
 * with menu_core.c. */
#include <string.h>
#include "menu_core.h"
#include "bar_core.h"
#include "../prefs/settings_def.h"

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

int mn_cond(int term, int dopus, int workbench)
{
    return (term ? MN_COND_TERM : 0) | (dopus ? MN_COND_DOPUS : 0) | (workbench ? MN_COND_WB : 0);
}

int mn_row_needs(int kind, const char *id)
{
    /* the Style > Terminal and Agents rows are shown with no terminal open: picking one opens a terminal first (lf_plan_t) */
    return (mn_kind_needs_terminal(kind) && strncmp(id, "style.terminal.", 15) && strncmp(id, "agents.", 7) ? MN_COND_TERM : 0) |
           (!strncmp(id, "dopus.", 6) ? MN_COND_DOPUS : 0) |
           (!strncmp(id, "workbench.", 10) ? MN_COND_WB : 0);
}

int mn_row_visible(int kind, int term)
{
    return term || !mn_kind_needs_terminal(kind);
}

static int row_shown(const km_menu *m, int cond)
{
    int need = mn_row_needs(m->kind, m->id);
    return (need & cond) == need;
}

/* 1 when some [menu] row whose id starts with prefix (a group's path and its dot) is visible */
static int group_visible(const keymap *k, const char *prefix, int plen, int cond)
{
    int i;
    for (i = 0; i < k->nmenu; i++)
        if (!strncmp(k->menu[i].id, prefix, plen) && row_shown(&k->menu[i], cond))
            return 1;
    return 0;
}

int mn_children(const keymap *k, const char *node, mn_item *out, int max)
{
    return mn_children_t(k, node, out, max, MN_COND_ALL);
}

/* the row of the keymap that is the settings page group `node` (id == node, kind settings:), else -1 */
static int settings_row(const keymap *k, const char *node)
{
    int i;
    for (i = 0; i < k->nmenu; i++)
        if (k->menu[i].kind == KA_SETTINGS && !strcmp(k->menu[i].id, node))
            return i;
    return -1;
}

/* the page's settings in table order: the rows the Prefs window shows on that page */
static int setting_rows(const keymap *k, int row, mn_item *out, int max)
{
    int page = sd_page_by_name(k->menu[row].arg), i, n = 0;
    for (i = 0; page >= 0 && i < sd_nsettings && n < max; i++) {
        const sd_setting *s = &sd_settings[i];
        if (s->page != page)
            continue;
        out[n].row = MN_ROW_SETTING;
        out[n].ix = s->id;
        cpy(out[n].seg, s->key ? s->key : "", KM_ID);
        sd_label_plain(s, out[n].label, KM_LABEL);
        n++;
    }
    return n;
}

/* node is "<settings page node>.<key>" of a cycle setting: that setting, else 0 */
static const sd_setting *choice_setting(const keymap *k, const char *node)
{
    int i, page;
    for (i = 0; i < k->nmenu; i++) {
        const char *id = k->menu[i].id;
        size_t l = strlen(id);
        int j;
        if (k->menu[i].kind != KA_SETTINGS || strncmp(node, id, l) || node[l] != '.')
            continue;
        page = sd_page_by_name(k->menu[i].arg);
        for (j = 0; page >= 0 && j < sd_nsettings; j++) {
            const sd_setting *s = &sd_settings[j];
            if (s->page == page && (s->kind == SD_CYCLE || SD_HAS_PICKER(s) || SD_HAS_LIST(s)) && s->key && !strcmp(s->key, node + l + 1))
                return s;
        }
    }
    return 0;
}

int (*mn_list_hook)(int profile, char names[][KM_LABEL], int max);
int (*mn_net_hook)(char names[][KM_LABEL], int max);

/* the network panel's info rows (interface, address, state) as inert note rows, ahead of its Up and Down rows */
static int net_rows(mn_item *out, int n, int max)
{
    static char nm[BAR_NET_ROWS][KM_LABEL];
    int i, c = mn_net_hook ? mn_net_hook(nm, BAR_NET_ROWS) : 0;
    for (i = 0; i < c && n < max; i++, n++) {
        out[n].row = MN_ROW_PKGACT;
        out[n].ix = -1;
        out[n].seg[0] = 0;
        cpy(out[n].label, nm[i], KM_LABEL);
    }
    return n;
}

/* the names of the Theme or Profile list, one MN_ROW_CHOICE row each (seg the index), then the action rows */
static int list_rows(const sd_setting *s, mn_item *out, int max)
{
    static char nm[24][KM_LABEL];
    static const char act[3] = { 'N', 'R', 'D' };
    static const char *const text[3] = { "New...", "Rename...", "Delete..." };
    int v, n = 0, c = mn_list_hook ? mn_list_hook(s->kind == SD_PROFILE, nm, 24) : 0;
    for (v = 0; v < c + 3 && n < max; v++, n++) {
        out[n].row = MN_ROW_CHOICE;
        out[n].ix = s->id;
        if (v < c) {
            out[n].seg[0] = (char)(v >= 10 ? '0' + v / 10 : '0' + v);
            out[n].seg[1] = (char)(v >= 10 ? '0' + v % 10 : 0);
            cpy(out[n].label, nm[v], KM_LABEL);
        } else {
            out[n].seg[0] = act[v - c];
            out[n].seg[1] = 0;
            cpy(out[n].label, text[v - c], KM_LABEL);
        }
        out[n].seg[2] = 0;
    }
    return n;
}

/* the values of a cycle setting, one MN_ROW_CHOICE row each */
static int choice_rows(const sd_setting *s, mn_item *out, int max)
{
    int v, n = 0, nv = s->nvalues;
    if (SD_HAS_LIST(s))
        return list_rows(s, out, max);
    if (SD_HAS_PICKER(s))
        nv = s->kind == SD_COLOUR ? SD_PICK_CUSTOM + 1 : 16;   /* the 16 palette entries, and Custom for a swatch */
    for (v = 0; v < nv && n < max; v++, n++) {
        out[n].row = MN_ROW_CHOICE;
        out[n].ix = s->id;
        out[n].seg[0] = (char)(v >= 10 ? '0' + v / 10 : '0' + v);   /* a cycle has under 100 values */
        out[n].seg[1] = (char)(v >= 10 ? '0' + v % 10 : 0);
        out[n].seg[2] = 0;
        cpy(out[n].label, SD_HAS_PICKER(s) ? "" : s->values[v].text, KM_LABEL);   /* a picker row is labelled by lf_setting_labels */
    }
    return n;
}

int mn_children_t(const keymap *k, const char *node, mn_item *out, int max, int cond)
{
    int i, j, n = 0, nl = (int)strlen(node);
    const sd_setting *cs;
    if (nl && (cs = choice_setting(k, node)) != 0)
        return choice_rows(cs, out, max);
    /* a settings page node lists its table rows first, then any static children the file hangs under the
     * same id (settings.amarchy: the page and the Rescan, Preferences, Menu font and Edit rows) */
    if (nl && (i = settings_row(k, node)) >= 0)
        n = setting_rows(k, i, out, max);
    if (!strcmp(node, BAR_NET_NODE))
        n = net_rows(out, n, max);
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
            if (!group_visible(k, id, (int)(dot - id) + 1, cond))
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
        } else if (k->menu[i].kind == KA_SETTINGS) {
            /* a settings page: a group here, its rows below it; static children of the same id already made
             * this group, so it is listed once */
            for (j = 0; j < n; j++)
                if (out[j].row == MN_ROW_GROUP && !strcmp(out[j].seg, rest))
                    dup = 1;
            if (dup)
                continue;
            out[n].row = MN_ROW_GROUP;
            out[n].ix = -1;
            cpy(out[n].seg, rest, KM_ID);
            cpy(out[n].label, k->menu[i].label, KM_LABEL);
        } else {
            if (!row_shown(&k->menu[i], cond))
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

int mn_deep_rows(const keymap *k, mn_item *out, int n, int max, int cond)
{
    int i;
    for (i = 0; i < k->nmenu && n < max; i++) {
        const char *id = k->menu[i].id, *seg;
        char *w = out[n].label, *end = out[n].label + MN_LABEL - 1;
        int j, depth = 0;
        if (!strchr(id, '.') || k->menu[i].kind == KA_SETTINGS || !row_shown(&k->menu[i], cond))
            continue;
        for (seg = id; *seg; seg++)
            if (*seg == '.')
                depth++;
        for (seg = id; depth >= 0 && w < end; depth--) {   /* "system.reboot" -> "System/" then the leaf's own label */
            const char *dot = strchr(seg, '.');
            if (!dot)
                break;
            *w++ = (char)((*seg >= 'a' && *seg <= 'z') ? *seg - 32 : *seg);
            for (j = 1; seg + j < dot && w < end; j++)
                *w++ = seg[j];
            if (w < end)
                *w++ = '/';
            seg = dot + 1;
        }
        *w = 0;
        cpy(w, k->menu[i].label, (int)(end - w) + 1);
        out[n].row = i;
        out[n].ix = -1;
        cpy(out[n].seg, strrchr(id, '.') + 1, KM_ID);
        n++;
    }
    return n;
}

int mn_right_enters(const mn_item *it)
{
    if (MN_IS_GROUP(it))
        return 1;
    if (MN_IS_SETTING(it)) {
        const sd_setting *s = &sd_settings[it->ix];
        return s->kind == SD_CYCLE || SD_HAS_PICKER(s) || SD_HAS_LIST(s);
    }
    return 0;
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
void mn_levels_t(const keymap *k, const char *node, mn_level_fn fn, void *ctx, int cond)
{
    mn_item it[MN_MAX_ITEMS];
    char g[KM_ID * 2];
    int i, nl = (int)strlen(node);
    fn(it, mn_children_t(k, node, it, MN_MAX_ITEMS, cond), ctx);
    for (i = 0; i < k->nmenu; i++) {
        const char *id = k->menu[i].id, *seg, *dot;
        if (nl && (strncmp(id, node, nl) || id[nl] != '.'))
            continue;
        for (seg = id + (nl ? nl + 1 : 0); (dot = strchr(seg, '.')) != NULL; seg = dot + 1) {
            if (dot - seg >= KM_ID || dot - id >= (int)sizeof g)
                break;
            memcpy(g, id, dot - id);
            g[dot - id] = 0;
            fn(it, mn_children_t(k, g, it, MN_MAX_ITEMS, cond), ctx);
        }
        if (k->menu[i].kind == KA_SETTINGS)
            fn(it, mn_children_t(k, id, it, MN_MAX_ITEMS, cond), ctx);
    }
}

void mn_levels(const keymap *k, const char *node, mn_level_fn fn, void *ctx)
{
    mn_levels_t(k, node, fn, ctx, MN_COND_ALL);
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

void mn_osd_size_t(const keymap *k, const char *node, int *cols, int *rows, int cond)
{
    osd_acc a;
    int c;
    a.maxn = a.maxw = 0;
    mn_levels_t(k, node, osd_level, &a, cond);
    c = a.maxw + 2 * MN_OSD_PAD;
    *cols = c < MN_OSD_MIN_COLS ? MN_OSD_MIN_COLS : c;
    if (a.maxn > MN_OSD_MAX_ENTRIES)
        a.maxn = MN_OSD_MAX_ENTRIES;   /* the window fits the shown rows, not the tallest level: the list scrolls */
    *rows = MN_OSD_HEAD_LINES + a.maxn + MN_OSD_BOTTOM_LINES;
}

void mn_osd_size(const keymap *k, const char *node, int *cols, int *rows)
{
    mn_osd_size_t(k, node, cols, rows, MN_COND_ALL);
}
