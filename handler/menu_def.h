/* The window menu's rows (UP-Term, Edit, View), one MD_ROW(type, label, key, id)
 * each. The handler (handler/vtcon_handler.c) defines MD_ROW as a struct
 * NewMenu initializer; the host test (tests/test_menudef.c) defines it as a
 * plain row, so it counts the rows the handler really builds the menu from. */
#define MENU_DEF_ROWS \
    MD_ROW(NM_TITLE, "UP-Term", 0, 0) \
    MD_ROW(NM_ITEM, "New tab", "T", MENU_TAB_NEW) \
    MD_ROW(NM_ITEM, "Next tab", ".", MENU_TAB_NEXT) \
    MD_ROW(NM_ITEM, "Previous tab", ",", MENU_TAB_PREV) \
    MD_ROW(NM_ITEM, "Close tab", 0, MENU_TAB_CLOSE) \
    MD_ROW(NM_ITEM, "Split right", "D", MENU_SPLIT_RIGHT) \
    MD_ROW(NM_ITEM, "Split down", 0, MENU_SPLIT_DOWN) \
    MD_ROW(NM_ITEM, "Close pane", 0, MENU_PANE_CLOSE) \
    MD_ROW(NM_ITEM, "Zoom pane", 0, MENU_PANE_ZOOM) \
    MD_ROW(NM_ITEM, "Claude", "L", MENU_CLAUDE) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Preferences...", 0, MENU_PREFS) \
    MD_ROW(NM_ITEM, "About UP-Term...", 0, MENU_ABOUT) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Close window", 0, MENU_CLOSE) \
    MD_ROW(NM_TITLE, "Edit", 0, 0) \
    MD_ROW(NM_ITEM, "Copy", "C", MENU_COPY) \
    MD_ROW(NM_ITEM, "Paste", "V", MENU_PASTE) \
    MD_ROW(NM_ITEM, "Select all", "A", MENU_SELECT_ALL) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Find...", "F", MENU_FIND) \
    MD_ROW(NM_ITEM, "Find next", "G", MENU_FIND_NEXT) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "Clear screen", "K", MENU_CLEAR_SCREEN) \
    MD_ROW(NM_ITEM, "Clear scrollback", 0, MENU_CLEAR_SB) \
    MD_ROW(NM_ITEM, "Reset terminal", 0, MENU_RESET) \
    MD_ROW(NM_TITLE, "View", 0, 0) \
    MD_ROW(NM_ITEM, "Bigger font", "+", MENU_FONT_BIGGER) \
    MD_ROW(NM_ITEM, "Smaller font", "-", MENU_FONT_SMALLER) \
    MD_ROW(NM_ITEM, NM_BARLABEL, 0, 0) \
    MD_ROW(NM_ITEM, "80 x 24", 0, MENU_SIZE_80X24) \
    MD_ROW(NM_ITEM, "132 x 43", 0, MENU_SIZE_132X43) \
    MD_ROW(NM_END, 0, 0, 0)
