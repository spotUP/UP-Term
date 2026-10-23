/* The ids of the window menu's items (GadTools user data). The menu
 * (handler/vtcon_handler.c) and the slash commands (handler/slash.c) name
 * the same actions by them, so a typed "/cursor bar" runs what the menu
 * item Cursor > Bar runs. */
#ifndef MENU_IDS_H
#define MENU_IDS_H

#define MENU_ID_LIST(X) \
    X(MENU_COPY) X(MENU_PASTE) X(MENU_FIND) X(MENU_PREFS) \
    X(MENU_CLOSE) X(MENU_TAB_NEW) X(MENU_TAB_NEXT) X(MENU_TAB_PREV) \
    X(MENU_TAB_CLOSE) X(MENU_KC_FILE) X(MENU_KC_COMMAND) X(MENU_KC_DEVICE) \
    X(MENU_KC_CACHE) X(MENU_KC_RESET) X(MENU_KC_PURGE) X(MENU_KC_INFO) \
    X(MENU_SET_BLOCK) X(MENU_SET_UNDERLINE) X(MENU_SET_BAR) X(MENU_SET_BLINK) \
    X(MENU_SET_BELL_NONE) X(MENU_SET_BELL_BEEP) X(MENU_SET_BELL_VISUAL) X(MENU_SET_BOLD) \
    X(MENU_SET_META_AMIGA) X(MENU_SET_META_ALT) X(MENU_SET_COPY) X(MENU_SET_WHEEL) \
    X(MENU_SET_UNIX) X(MENU_SET_KINGCON) X(MENU_SET_KC_W) X(MENU_SET_KC_L) \
    X(MENU_SET_KC_B) X(MENU_SET_KC_C) X(MENU_SET_KC_S) X(MENU_SET_FONT) \
    X(MENU_SET_THEME) X(MENU_SET_SAVE) X(MENU_SET_SB_NONE) X(MENU_SET_SB_500) \
    X(MENU_SET_SB_1000) X(MENU_SET_SB_2000) X(MENU_SET_SB_5000) X(MENU_ABOUT) \
    X(MENU_SELECT_ALL) X(MENU_FIND_NEXT) X(MENU_CLEAR_SCREEN) X(MENU_CLEAR_SB) \
    X(MENU_RESET) X(MENU_FONT_BIGGER) X(MENU_FONT_SMALLER) X(MENU_SIZE_80X24) \
    X(MENU_SIZE_132X43) X(MENU_SCREEN_WB) X(MENU_SCREEN_OWN) X(MENU_SCREEN_FULL) \
    X(MENU_SET_REFLOW) X(MENU_SET_SCROLLBAR) X(MENU_SET_BS_DEL) X(MENU_SET_BS_BS) \
    X(MENU_SET_CLIP_WRITE) X(MENU_SET_CLIP_READ_WRITE) X(MENU_SET_CLIP_OFF) X(MENU_DEMO) \
    X(MENU_SET_AMIGA_KEYS) X(MENU_SET_DITHER) X(MENU_SET_PAD_BG) X(MENU_SET_PAD_EXTEND) \
    X(MENU_CLAUDE) X(MENU_SET_TERM_VTCON) X(MENU_SET_TERM_XTERM) X(MENU_SET_TERM_SCREEN) \
    X(MENU_SET_TERM_NONE) X(MENU_SET_COLORS_RGB) X(MENU_SET_COLORS_256) X(MENU_SET_COLORS_NONE) \
    X(MENU_SET_COLORS_16) X(MENU_HELP_SLASH) X(MENU_TUTORIAL) \
    X(MENU_SET_OUTLINE_AA)

/* The ids count up from 1 in this order (the order is the value: a profile
 * place or a saved id must not move). handler/keymap_act.c builds its name
 * table (menuid:MENU_COPY in the keymap file) from the same list, so a new
 * item is named once. */
enum {
    MENU_ID_NONE_ = 0,
#define MENU_ID_ENUM(n) n,
    MENU_ID_LIST(MENU_ID_ENUM)
#undef MENU_ID_ENUM
    MENU_SET_PROFILE0 = 100, /* + the profile's place in the file */
    MENU_TAB_GOTO0 = 200     /* + n (1..9): the keymap's tab:goto n */
};

#endif
