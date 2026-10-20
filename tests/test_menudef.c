/* handler/menu_def.h: the rows the handler builds the window menu from. Each
 * split/pane action is one menu item, built once, and no id is built twice. */
#include <string.h>
#include "harness.h"

#define NM_END 0
#define NM_TITLE 1
#define NM_ITEM 2
#define NM_BARLABEL "-"
#include "../handler/menu_ids.h"
#include "../handler/menu_def.h"

typedef struct { int type; const char *label, *key; int id; } md_row;
#define MD_ROW(type, label, key, id) { type, label, key, id },
static const md_row ROWS[] = { MENU_DEF_ROWS };
#define NROWS ((int)(sizeof(ROWS) / sizeof(ROWS[0])))

static int items_with(int id)
{
    int i, n = 0;
    for (i = 0; i < NROWS; i++)
        if (ROWS[i].type == NM_ITEM && ROWS[i].id == id) n++;
    return n;
}

static void each_split_item_is_built_once(void)
{
    CHECK_INT(items_with(MENU_SPLIT_RIGHT), 1);
    CHECK_INT(items_with(MENU_SPLIT_DOWN), 1);
    CHECK_INT(items_with(MENU_PANE_CLOSE), 1);
    CHECK_INT(items_with(MENU_PANE_ZOOM), 1);
}

static void no_id_is_built_twice_and_the_table_ends_with_nm_end(void)
{
    int i, j, dup = 0;
    for (i = 0; i < NROWS; i++)
        for (j = i + 1; j < NROWS; j++)
            if (ROWS[i].type == NM_ITEM && ROWS[j].type == NM_ITEM && ROWS[i].id && ROWS[i].id == ROWS[j].id)
                dup++;
    CHECK_INT(dup, 0);
    CHECK_INT(ROWS[NROWS - 1].type, NM_END);
}

static void the_split_items_sit_in_the_first_menu(void)
{
    int i, first_end = NROWS;
    for (i = 1; i < NROWS; i++)
        if (ROWS[i].type == NM_TITLE) { first_end = i; break; }
    for (i = 0; i < first_end; i++)
        if (ROWS[i].id == MENU_SPLIT_RIGHT) CHECK_STR(ROWS[i].key, "D");
    CHECK_INT(items_with(MENU_PANE_PREV), 1);
    CHECK_INT(items_with(MENU_PANE_NEXT), 1);
}

void suite_menudef(void)
{
    each_split_item_is_built_once();
    no_id_is_built_twice_and_the_table_ends_with_nm_end();
    the_split_items_sit_in_the_first_menu();
}
