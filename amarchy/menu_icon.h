/* menu_icon -- the drawn Amarchy menu's icons (plan 2026-10-23-amarchy-drawn-menu.md, G3).
 *
 * dist/menu-icons names a Symbols Nerd Font glyph for a node id; tools/gen_menu_icons.py turns them at
 * build time into ONE colour-emoji page (the tools/gen_emoji.py converter and its page layout,
 * render/emoji.h) compiled into the program as build/amiga/menu_icons.inc. The glyph is white, its alpha
 * the coverage; the program draws the coverage in the row's text colour. No OS calls: a host suite. */
#ifndef AMARCHY_MENU_ICON_H
#define AMARCHY_MENU_ICON_H

#include "../render/emoji.h"
#include "menu_geom.h"

#define MI_MAX 32               /* the largest icon square, pixels */

/* The generated page file (for ce_page_check) and its length. */
const vt_u8 *mi_page(long *len);
/* The icons: count, and the node id of the i-th. */
int mi_count(void);
const char *mi_id(int i);
/* The glyph of node id in the page, 0 when the node has no icon. */
const vt_u8 *mi_find(const char *id);
/* The icon square for these metrics: half the row's height, MI_MAX at most, 8 at least. */
int mi_size(const mg_metrics *m);
/* The glyph's coverage (0..255) averaged to an s x s square into out (s * s bytes, s <= MI_MAX),
 * row after row. 0 when s is out of range or the glyph is bad. */
int mi_render(const vt_u8 *glyph, int s, vt_u8 *out);

#endif
