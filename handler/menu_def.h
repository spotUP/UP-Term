/* The window menu's rows (UP-Term, Edit, View), one MD_ROW(type, label, key, id)
 * each. The handler (handler/vtcon_handler.c) defines MD_ROW as a struct
 * NewMenu initializer; the host test (tests/test_menudef.c) defines it as a
 * plain row, so it counts the rows the handler really builds the menu from.
 * Command keys come from the keymap (menu_add, kact_comm_key); only Split
 * right keeps its own D, which the keymap does not bind. */
#define MENU_DEF_ROWS \
    MD_ROW(NM_TITLE, "UP-Term", 0, 0) \
    MD_ROW(NM_ITEM, "New tab", 0, MENU_TAB_NEW) \
    MD_ROW(NM_ITEM, "Next tab", 0, MENU_TAB_NEXT) \
    MD_ROW(NM_ITEM, "Previous tab", 0, MENU_TAB_PREV) \
    MD_ROW(NM_ITEM, "Close tab", 0, MENU_TAB_CLOSE) \
    MD_ROW(NM_ITEM, "Split right", "D", MENU_SPLIT_RIGHT) \
    MD_ROW(NM_ITEM, "Split down", 0, MENU_SPLIT_DOWN) \
    MD_ROW(NM_ITEM, "Close pane", 0, MENU_PANE_CLOSE) \
    MD_ROW(NM_ITEM, "Zoom pane", 0, MENU_PANE_ZOOM) \
    MD_ROW(NM_ITEM, "Previous pane", 0, MENU_PANE_PREV) \
    MD_ROW(NM_ITEM, "Next pane", 0, MENU_PANE_NEXT) \
    MD_ROW(NM_ITEM, "Claude", 0, MENU_CLAUDE) \
    MD_ROW(NM_ITEM, "pi", 0, MENU_PI) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Preferences...", 0, MENU_PREFS) \
    MD_ROW(NM_ITEM, "About UP-Term...", 0, MENU_ABOUT) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Close window", 0, MENU_CLOSE) \
    MD_ROW(NM_TITLE, "Edit", 0, 0) \
    MD_ROW(NM_ITEM, "Copy", 0, MENU_COPY) \
    MD_ROW(NM_ITEM, "Paste", 0, MENU_PASTE) \
    MD_ROW(NM_ITEM, "Select all", 0, MENU_SELECT_ALL) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Find...", 0, MENU_FIND) \
    MD_ROW(NM_ITEM, "Find next", 0, MENU_FIND_NEXT) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Clear screen", 0, MENU_CLEAR_SCREEN) \
    MD_ROW(NM_ITEM, "Clear scrollback", 0, MENU_CLEAR_SB) \
    MD_ROW(NM_ITEM, "Reset terminal", 0, MENU_RESET) \
    MD_ROW(NM_TITLE, "View", 0, 0) \
    MD_ROW(NM_ITEM, "Bigger font", 0, MENU_FONT_BIGGER) \
    MD_ROW(NM_ITEM, "Smaller font", 0, MENU_FONT_SMALLER) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "80 x 24", 0, MENU_SIZE_80X24) \
    MD_ROW(NM_ITEM, "132 x 43", 0, MENU_SIZE_132X43) \
    MD_ROW(NM_END, 0, 0, 0)
