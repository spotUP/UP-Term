/* Amarchy quick menu and key viewer, the portable part (plan
 * 2026-10-22-amarchy-commodity.md, Phase D). No OS calls, so tests/test_keymap.c covers it.
 * amarchy/menu.c (the amarchy-menu helper) draws it with libupkgui; the rows come from the
 * one keymap (config/keymap.c): the menu reads [menu], the viewer reads [bind]. */
#ifndef AMARCHY_MENU_CORE_H
#define AMARCHY_MENU_CORE_H

#include "../config/keymap.h"

#define MN_MAX_ITEMS 64

#define MN_ROW_GROUP (-1)
#define MN_ROW_INDEX (-2)       /* the action kind of an app or game from the index: X5 launches it */
#define MN_ROW_SETTING (-3)     /* a setting of a settings page group: ix is its SDI_* row in prefs/settings_def.h; label is the plain label, sd_row_label adds the value */
#define MN_ROW_CHOICE (-4)      /* one value of a cycle setting's submenu (node <page node>.<key>): ix is the SDI_* id, seg the value index in decimal, label the value text */
#define MN_ROW_PKGACT (-5)      /* a package action row (index_core.c): ix is the row in ix_index.pk, seg[0] the PK_ACT_* verb, label its words; ix -1 or an empty seg is an inert row (a note) */
#define MN_IS_GROUP(it) ((it)->row == MN_ROW_GROUP)
#define MN_IS_INDEX(it) ((it)->row == MN_ROW_INDEX)
#define MN_IS_SETTING(it) ((it)->row == MN_ROW_SETTING)
/* set by lf_init (leaf.c): fills the names of the Profile (1) or Theme (0) list, marked "* " when current */
extern int (*mn_list_hook)(int profile, char names[][KM_LABEL], int max);
extern int (*mn_net_hook)(char names[][KM_LABEL], int max);   /* the network panel's info rows (BAR_NET_NODE), 0 when unset */
#define MN_IS_CHOICE(it) ((it)->row == MN_ROW_CHOICE)
#define MN_IS_PKGACT(it) ((it)->row == MN_ROW_PKGACT)

/* The room of a row label. A settings row is "Label: Value" (sd_row_label), longer than the 31 characters of a
 * keymap label (KM_LABEL), and its value must not be cut off by the buffer. */
#define MN_LABEL 96

typedef struct mn_item {
    int  row;                   /* index into keymap.menu, MN_ROW_GROUP for a group (a node with children),
                                 * MN_ROW_INDEX for an app index row (amarchy/index_core.h),
                                 * MN_ROW_SETTING for a setting row (settings_def) */
    int  ix;                    /* for MN_ROW_INDEX: the row in the ix_index; MN_ROW_SETTING: the SDI_* id; else -1 */
    char seg[KM_ID];            /* the id segment: the node path of a group is node + "." + seg */
    char label[MN_LABEL];       /* a leaf's own label; a group's segment with a capital */
} mn_item;

/* Filter match of one label: 2 when the label starts with the filter, 1 when it contains it,
 * 0 when not; case is ignored; an empty filter matches everything (2). */
int mn_match(const char *label, const char *filter);

/* Rows whose action needs a terminal window (tab:, menuid:, slash:) are shown only when the menu
 * opened over an UP-Term window (TERM-ROWS-ONLY-OVER-TERMINAL). The one decision both menus use. */
int mn_kind_needs_terminal(int kind);
/* Conditions a menu is drawn under (`cond`, a bit set): a terminal window is present, a Directory Opus port
 * is present, the WORKBENCH ARexx port is present (OS 3.2 only). A row that needs one is shown only when its bit is set; MN_COND_ALL shows everything, which
 * mn_children does. The callers probe once at open: mn_cond(upactive_terminal_present(..),
 * upactive_dopus_present(), upactive_workbench_present()). */
#define MN_COND_TERM 1
#define MN_COND_DOPUS 2
#define MN_COND_WB 4
#define MN_COND_ALL (MN_COND_TERM | MN_COND_DOPUS | MN_COND_WB)
int mn_cond(int term, int dopus, int workbench);
/* the bits a [menu] row needs: MN_COND_DOPUS for every id under the `dopus` group, MN_COND_WB under `workbench`, MN_COND_TERM for the
 * terminal kinds (tab:, menuid:, slash:) */
int mn_row_needs(int kind, const char *id);
/* 1 when a row of action kind `kind` is shown, given term = a terminal window is present. */
int mn_row_visible(int kind, int term);
/* mn_children with that filter: a hidden row is left out, and a group with no visible row below it
 * (at any depth) is left out too. cond = MN_COND_ALL shows everything, which mn_children does. */
int mn_children_t(const keymap *k, const char *node, mn_item *out, int max, int term);

/* The entries one level below `node` ("" is the root, "apps" the Apps node), in file order: a
 * [menu] id one segment deeper is a leaf, a longer one makes a group (once). The count; at
 * most max. A row whose id equals the node itself is never its own child, except that a row of
 * kind KA_SETTINGS (`settings:<page>`) makes its id a group in its parent and, as the node, lists the
 * page's settings as MN_ROW_SETTING rows in settings_def table order. */
int mn_children(const keymap *k, const char *node, mn_item *out, int max);

/* MENU-DEEP-SEARCH: appends to out (n entries, at most max) one row per visible [menu] leaf that sits in a group, labelled
 * with its path ("System/Reboot"), row = its keymap row, so running it is the same as from its own level. A
 * settings page is left out (it is a group). The root level with a filter lists these, so typing finds
 * a leaf at any depth; a level below the root keeps its own entries. Count. */
int mn_deep_rows(const keymap *k, mn_item *out, int n, int max, int cond);

/* The entries that match filter, prefix matches first, each group in its order. Writes the
 * indices into items to idx; the count. */
int mn_filter(const mn_item *items, int n, const char *filter, int *idx, int max);

/* Where the entry labelled label sits among the c shown entries (items[idx[i]]); -1 when it is not shown.
 * A rebuilt level (the scan's status row changed) keeps the cursor on the same entry with it. */
int mn_pick(const mn_item *items, const int *idx, int c, const char *label);
/* The labels of the c rows shown (items[idx[0..c)]) joined by '|' into out (size n), whole labels only: the
 * value of the AMARCHY_ROWS sentinel both menu styles publish, so a rig reads the rows without screen text. */
void mn_rows_join(const mn_item *items, const int *idx, int c, char *out, int n);
/* The position of the setting row with SDI_* id ix: its label changes with its value, so a rebuild after a
 * set keeps the cursor by id. -1 when it is not listed. */
int mn_pick_setting(const mn_item *items, const int *idx, int c, int ix);
/* THEME-LIST-OPENS-ROW0: where the cursor lands when a level opens or is rebuilt (both menu styles call this one).
 * The setting row ix (>= 0) wins, then the entry labelled keep, else the entry labelled current_label (the saved theme
 * as the theme list shows it, "<name> (current)"), so the list opens on the saved theme and the live preview starts
 * on it instead of on row 0. -1 when none is shown: the caller keeps its first-row default. */
int mn_open_pos(const mn_item *items, const int *idx, int c, int ix, const char *keep, const char *current_label);

/* The node one level up ("apps.tools" -> "apps", "apps" -> ""). 0 at the root: Esc there leaves
 * the menu; otherwise 1 and out holds the parent. */
int mn_parent(const char *node, char *out, int n);
/* THEME-ENTER-BACK-ONE: a picked theme row (path "gtheme:<name>") saved the theme; Enter then backs out ONE level (the
 * list's parent, Style) and the menu stays open. 1 and out = the parent node; 0 when path is not a theme row or node is
 * the root. Both menus (menu.c, gui.c) take their next node from here. */
int mn_pick_back(const char *path, const char *node, char *out, int n);
/* What Escape and Backspace do in both Amarchy menus (gui.c and menu.c). Escape with a filter clears it,
 * Backspace with a filter drops its last character; with no filter both go up one level, and at the root
 * (node empty) both close the menu. bs is 1 for Backspace, 0 for Escape. */
enum { MN_BACK_CLEAR_FILTER, MN_BACK_DROP_CHAR, MN_BACK_UP, MN_BACK_CLOSE };
int mn_back(int bs, const char *filter, const char *node);
/* MENU-BS-REPEAT: a held Backspace repeats. The repeat that empties the query must not be followed by one
 * that goes up or closes: only a fresh press (no Backspace handled within MN_BS_REPEAT_TICKS) may. now is
 * in 1/50 s ticks (any steady clock). mn_back_rep answers as mn_back(1, ...) or MN_BACK_NONE (ignore the key). */
/* mn_back_held is the core: repeat is 1 when the key is an auto-repeat (the drawn menu reads it from
 * IEQUALIFIER_REPEAT, exact). mn_back_rep derives repeat from timing for callers with no such signal. */
int mn_back_held(int repeat, const char *filter, const char *node);
#define MN_BS_REPEAT_TICKS 20
#define MN_BACK_NONE 4
typedef struct { long last; } mn_bsguard;
void mn_bsguard_init(mn_bsguard *g);
int mn_back_rep(mn_bsguard *g, long now, const char *filter, const char *node);
/* The node an item opens (groups only): node + "." + seg. */
void mn_child_node(const char *node, const mn_item *it, char *out, int n);
/* 1 when the Right arrow enters the row, as Return does: a group, a settings page, a cycle or colour or list
 * setting (its submenu). 0 for a leaf, an index row, a choice row and a check/number/text setting: Right
 * never runs or changes them. The one decision the drawn and the text menu share. */
int mn_right_enters(const mn_item *it);

/* Every level a menu opened at `node` can show: node itself, then each group below it, each passed
 * to fn as its entries (mn_children) and their count. The window sizes (mn_osd_size here, the drawn
 * menu's amarchy/menu_geom.c) take their maxima over this one walk. */
typedef void (*mn_level_fn)(const mn_item *items, int n, void *ctx);
void mn_levels(const keymap *k, const char *node, mn_level_fn fn, void *ctx);
/* mn_levels over the tree mn_children_t(term) shows: the walk the drawn menu's sizes must use */
void mn_levels_t(const keymap *k, const char *node, mn_level_fn fn, void *ctx, int term);

/* The menu window's size in cells, so the OSD fits its content (plan 2026-10-23-amarchy-osd.md,
 * O5). The head lines and the side padding equal the library's UI_OSD_HEAD_LINES and UI_OSD_PAD
 * (amarchy/menu.c checks that at compile time; the commodity cannot include upkgui/ui.h). */
#define MN_OSD_PAD_LINES  1     /* blank rows above and below: a cell is about twice as tall as wide, so one row looks as wide as MN_OSD_PAD (2) columns */
#define MN_OSD_HEAD_LINES (MN_OSD_PAD_LINES + 2) /* padding, the filter line and the gap above the first entry */
#define MN_OSD_BOTTOM_LINES MN_OSD_PAD_LINES     /* padding below the last entry row */
#define MN_OSD_PAD        2     /* columns of padding on each side of the text */
#define MN_OSD_MIN_COLS   24    /* room for the filter line however short the labels are */
#define MN_OSD_MAX_ENTRIES 10   /* entry rows shown at once, text and drawn menu alike; a longer level scrolls */
/* Rows and columns for `node` and every level reachable from it (the menu is walked with
 * mn_children, as amarchy-menu walks it): rows = head lines + the most entries any of those
 * levels has, at most MN_OSD_MAX_ENTRIES (the list scrolls past that) + the bottom padding lines, cols = the widest "label  item" (a group's item is ">") + 2 * pad, at least
 * MN_OSD_MIN_COLS. Both come back through the pointers. */
void mn_osd_size(const keymap *k, const char *node, int *cols, int *rows);
/* mn_osd_size over the rows mn_row_visible(term) leaves: the size of the menu actually shown */
void mn_osd_size_t(const keymap *k, const char *node, int *cols, int *rows, int term);

/* One viewer line for a [bind] row: "Super+Shift+Space  Apps  (menu)". The spec column is padded
 * to MN_SPEC_COL. 1 when it fit. */
#define MN_SPEC_COL 22
int mn_spec(const km_bind *b, char *out, int n);
int mn_viewer_line(const km_bind *b, char *out, int n);
/* The whole viewer text, one line per [bind] row in table order, each ending in a newline.
 * The line count; the text is cut at a line when buf is full. */
int mn_viewer_text(const keymap *k, char *buf, long cap);

/* The TERM the helper must use: "vtcon" when the environment has none (TERM is unset system-wide
 * on an Amiga, and ncurses' newterm fails without it), else NULL (keep what is set). The helper
 * only ever runs in an XCON: window, which is the vtcon handler, so the default is that terminal's
 * own name; vsh sets it the same way. */
const char *mn_term_default(const char *term);

#endif
