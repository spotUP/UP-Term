/* Amarchy quick menu and key viewer, the portable part (plan
 * 2026-10-22-amarchy-commodity.md, Phase D). No OS calls, so tests/test_keymap.c covers it.
 * amarchy/menu.c (the amarchy-menu helper) draws it with libupkgui; the rows come from the
 * one keymap (config/keymap.c): the menu reads [menu], the viewer reads [bind]. */
#ifndef AMARCHY_MENU_CORE_H
#define AMARCHY_MENU_CORE_H

#include "../config/keymap.h"

#define MN_MAX_ITEMS 64

typedef struct mn_item {
    int  row;                   /* index into keymap.menu, -1 for a group (a node with children) */
    char seg[KM_ID];            /* the id segment: the node path of a group is node + "." + seg */
    char label[KM_LABEL];       /* a leaf's own label; a group's segment with a capital */
} mn_item;

/* Filter match of one label: 2 when the label starts with the filter, 1 when it contains it,
 * 0 when not; case is ignored; an empty filter matches everything (2). */
int mn_match(const char *label, const char *filter);

/* The entries one level below `node` ("" is the root, "apps" the Apps node), in file order: a
 * [menu] id one segment deeper is a leaf, a longer one makes a group (once). The count; at
 * most max. A row whose id equals the node itself is never its own child. */
int mn_children(const keymap *k, const char *node, mn_item *out, int max);

/* The entries that match filter, prefix matches first, each group in its order. Writes the
 * indices into items to idx; the count. */
int mn_filter(const mn_item *items, int n, const char *filter, int *idx, int max);

/* The node one level up ("apps.tools" -> "apps", "apps" -> ""). 0 at the root: Esc there leaves
 * the menu; otherwise 1 and out holds the parent. */
int mn_parent(const char *node, char *out, int n);
/* The node an item opens (groups only): node + "." + seg. */
void mn_child_node(const char *node, const mn_item *it, char *out, int n);

/* One viewer line for a [bind] row: "Super+Shift+Space  Apps  (menu)". The spec column is padded
 * to MN_SPEC_COL. 1 when it fit. */
#define MN_SPEC_COL 22
int mn_spec(const km_bind *b, char *out, int n);
int mn_viewer_line(const km_bind *b, char *out, int n);
/* The whole viewer text, one line per [bind] row in table order, each ending in a newline.
 * The line count; the text is cut at a line when buf is full. */
int mn_viewer_text(const keymap *k, char *buf, long cap);

#endif
