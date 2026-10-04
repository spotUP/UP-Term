/* Key encoding per personality. */
#include "harness.h"

static const char *key(vt_term *t, long k, int mods)
{
    static char buf[40];
    int n = vt_encode_key(t, k, mods, (vt_u8 *)buf);
    buf[n] = 0;
    return buf;
}

/* ---- phase A5: modes that change what keys and the mouse send ---- */

static void meta_escape_and_modify_other_keys(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    CHECK_STR(key(t, 'a', VT_MOD_ALT), "\033a");
    h_put(t, "\033[?1034h");
    CHECK_INT((unsigned char)key(t, 'a', VT_MOD_ALT)[0], 0xE1); /* Meta sets the 8th bit */
    CHECK_STR(key(t, VT_KEY_ESCAPE, 0), "\033");
    h_put(t, "\033[?7727h");
    CHECK_STR(key(t, VT_KEY_ESCAPE, 0), "\033O[");
    CHECK_STR(key(t, 'a', VT_MOD_CTRL), "\001");
    h_put(t, "\033[>4;1m");
    CHECK_STR(key(t, 'a', VT_MOD_CTRL), "\001");          /* level 1: plain still says it */
    CHECK_STR(key(t, '1', VT_MOD_CTRL), "\033[27;5;49~");  /* Ctrl+1 has no plain form */
    CHECK_STR(key(t, 'A', VT_MOD_CTRL | VT_MOD_SHIFT), "\033[27;6;65~");
    h_put(t, "\033[>4;2m");
    CHECK_STR(key(t, 'a', VT_MOD_CTRL), "\033[27;5;97~");  /* level 2: every modified key */
    h_reply_clear();
    h_put(t, "\033[?4m");
    CHECK_STR(h_reply, "\033[>4;2m");
    vt_free(t);
}

/* gap #6: Alt+Backspace is readline's backward-kill-word only with the ESC */
static void alt_backspace_return_tab_escape_send_the_esc_prefix(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    CHECK_STR(key(t, VT_KEY_BACKSPACE, VT_MOD_ALT), "\033\177");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_ALT), "\033\r");
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_ALT), "\033\t");
    CHECK_STR(key(t, VT_KEY_ESCAPE, VT_MOD_ALT), "\033\033");
    CHECK_STR(key(t, VT_KEY_BACKSPACE, VT_MOD_CTRL), "\010");  /* xterm: Ctrl+Backspace is BS */
    CHECK_STR(key(t, VT_KEY_BACKSPACE, VT_MOD_CTRL | VT_MOD_ALT), "\033\010");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_CTRL), "\r");       /* no form without modifyOtherKeys */
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_SHIFT | VT_MOD_ALT), "\033\033[Z");
    h_put(t, "\033[20h");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_ALT), "\033\r\n");
    vt_free(t);
    t = h_new(80, 24, VT_AMIGA);
    CHECK_STR(key(t, VT_KEY_BACKSPACE, VT_MOD_ALT), "\010");   /* the console has no Meta prefix */
    vt_free(t);
}

/* gap #6: Ctrl+Enter, Shift+Enter, Ctrl+Tab as xterm's modifyOtherKeys says them */
static void modify_other_keys_reports_modified_return_and_tab(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    h_put(t, "\033[>4;1m");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_CTRL), "\033[27;5;13~");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_SHIFT), "\033[27;2;13~");
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_CTRL), "\033[27;5;9~");
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_SHIFT), "\033[Z");          /* back-tab stays */
    CHECK_STR(key(t, VT_KEY_BACKSPACE, VT_MOD_CTRL), "\010");       /* its own key at level 1 */
    CHECK_STR(key(t, VT_KEY_ESCAPE, VT_MOD_CTRL), "\033[27;5;27~");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_ALT), "\033\r");         /* level 1: Meta has a plain form */
    CHECK_STR(key(t, VT_KEY_RETURN, 0), "\r");
    h_put(t, "\033[>4;2m");
    CHECK_STR(key(t, VT_KEY_RETURN, VT_MOD_ALT), "\033[27;3;13~");
    CHECK_STR(key(t, VT_KEY_BACKSPACE, VT_MOD_CTRL), "\033[27;5;127~");
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_SHIFT), "\033[Z");
    vt_free(t);
}

static void utf8_mouse_reaches_past_column_223(void)
{
    vt_term *t = h_new(400, 24, VT_XTERM);
    char buf[40];
    int n;
    h_put(t, "\033[?1000h\033[?1005h");
    n = vt_encode_mouse(t, 0, 0, 300, 5, 0, (vt_u8 *)buf);
    buf[n] = 0;
    /* ESC [ M, 32, then 333 and 38 as UTF-8 */
    CHECK_STR(buf, "\033[M \xc5\x8d&");
    vt_free(t);
}

static void xterm_cursor_keys_follow_decckm(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    CHECK_STR(key(t, VT_KEY_UP, 0), "\033[A");
    h_put(t, "\033[?1h");
    CHECK_STR(key(t, VT_KEY_UP, 0), "\033OA");
    CHECK_STR(key(t, VT_KEY_LEFT, VT_MOD_CTRL), "\033[1;5D");
    CHECK_STR(key(t, VT_KEY_HOME, 0), "\033OH");
    vt_free(t);
}

static void xterm_function_and_editing_keys(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    CHECK_STR(key(t, VT_KEY_F1, 0), "\033OP");
    CHECK_STR(key(t, VT_KEY_F4, VT_MOD_SHIFT), "\033[1;2S");
    CHECK_STR(key(t, VT_KEY_F5, 0), "\033[15~");
    CHECK_STR(key(t, VT_KEY_F12, 0), "\033[24~");
    CHECK_STR(key(t, VT_KEY_DELETE, 0), "\033[3~");
    CHECK_STR(key(t, VT_KEY_PAGE_DOWN, VT_MOD_ALT), "\033[6;3~");
    CHECK_STR(key(t, VT_KEY_BACKSPACE, 0), "\177");
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_SHIFT), "\033[Z");
    vt_free(t);
}

static void xterm_characters_are_utf8(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    CHECK_STR(key(t, 0xE5, 0), "\xc3\xa5");
    CHECK_STR(key(t, 'c', VT_MOD_CTRL), "\003");
    CHECK_STR(key(t, 'x', VT_MOD_ALT), "\033x");
    vt_free(t);
}

static void xterm_return_obeys_lnm(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    CHECK_STR(key(t, VT_KEY_RETURN, 0), "\r");
    h_put(t, "\033[20h");
    CHECK_STR(key(t, VT_KEY_RETURN, 0), "\r\n");
    vt_free(t);
}

static void amiga_keys_use_the_8bit_csi(void)
{
    vt_term *t = h_new(10, 2, VT_AMIGA);
    CHECK_STR(key(t, VT_KEY_UP, 0), "\x9b" "A");
    CHECK_STR(key(t, VT_KEY_UP, VT_MOD_SHIFT), "\x9b" "T");
    CHECK_STR(key(t, VT_KEY_LEFT, VT_MOD_SHIFT), "\x9b" " A");
    CHECK_STR(key(t, VT_KEY_F1, 0), "\x9b" "0~");
    CHECK_STR(key(t, VT_KEY_F10, VT_MOD_SHIFT), "\x9b" "19~");
    CHECK_STR(key(t, VT_KEY_HELP, 0), "\x9b" "?~");
    CHECK_STR(key(t, VT_KEY_BACKSPACE, 0), "\010");
    CHECK_STR(key(t, VT_KEY_DELETE, 0), "\177");
    CHECK_STR(key(t, 0xE5, 0), "\xe5");
    CHECK_STR(key(t, VT_KEY_F11, 0), "\x9b" "20~");
    CHECK_STR(key(t, VT_KEY_F12, VT_MOD_SHIFT), "\x9b" "31~");
    CHECK_STR(key(t, VT_KEY_INSERT, 0), "\x9b" "40~");
    CHECK_STR(key(t, VT_KEY_END, VT_MOD_SHIFT), "\x9b" "55~");
    CHECK_STR(key(t, VT_KEY_TAB, VT_MOD_SHIFT), "\x9b" "Z");
    vt_free(t);
}

static void pcansi_keys_are_plain_ansi(void)
{
    vt_term *t = h_new(10, 2, VT_PCANSI);
    h_put(t, "\033[?1h");
    CHECK_STR(key(t, VT_KEY_UP, VT_MOD_SHIFT), "\033[A");
    CHECK_STR(key(t, 0xC7, 0), "\x80"); /* CP437 */
    CHECK_STR(key(t, VT_KEY_BACKSPACE, 0), "\010");
    vt_free(t);
}

static void bracketed_paste_only_when_asked(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    vt_u8 b[16];
    CHECK_INT(vt_encode_paste(t, 0, b), 0);
    h_put(t, "\033[?2004h");
    CHECK_INT(vt_encode_paste(t, 0, b), 6);
    CHECK(memcmp(b, "\033[200~", 6) == 0);
    vt_free(t);
}

static const char *mouse(vt_term *t, int btn, int kind, int x, int y, int mods)
{
    static char buf[40];
    int n = vt_encode_mouse(t, btn, kind, x, y, mods, (vt_u8 *)buf);
    buf[n] = 0;
    return buf;
}

static void mouse_reports_follow_the_modes(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    CHECK_STR(mouse(t, 0, 0, 0, 0, 0), "");
    h_put(t, "\033[?1000h");
    CHECK_STR(mouse(t, 0, 0, 4, 2, 0), "\033[M %#");
    CHECK_STR(mouse(t, 0, 1, 4, 2, 0), "\033[M#%#");
    CHECK_STR(mouse(t, 0, 2, 4, 2, 0), ""); /* no motion without ?1002 */
    h_put(t, "\033[?1006h");
    CHECK_STR(mouse(t, 2, 0, 9, 19, VT_MOD_CTRL), "\033[<18;10;20M");
    CHECK_STR(mouse(t, 2, 1, 9, 19, 0), "\033[<2;10;20m");
    CHECK_STR(mouse(t, 64, 0, 0, 0, 0), "\033[<64;1;1M");
    h_put(t, "\033[?1002h");
    CHECK_STR(mouse(t, 0, 2, 1, 1, 0), "\033[<32;2;2M");
    vt_free(t);
}

static void keypad_follows_deckpam(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    CHECK_STR(key(t, VT_KEY_KP_5, 0), "5");
    h_put(t, "\033=");
    CHECK_STR(key(t, VT_KEY_KP_5, 0), "\033Ou");
    CHECK_STR(key(t, VT_KEY_KP_MINUS, 0), "\033Om");
    CHECK_STR(key(t, VT_KEY_KP_ENTER, 0), "\033OM");
    CHECK_STR(key(t, VT_KEY_KP_LPAREN, 0), "(");
    h_put(t, "\033>");
    CHECK_STR(key(t, VT_KEY_KP_SLASH, 0), "/");
    vt_free(t);
    t = h_new(10, 2, VT_AMIGA);
    CHECK_STR(key(t, VT_KEY_KP_7, 0), "7");
    vt_free(t);
}

void suite_keys(void)
{
    keypad_follows_deckpam();
    mouse_reports_follow_the_modes();
    xterm_cursor_keys_follow_decckm();
    xterm_function_and_editing_keys();
    xterm_characters_are_utf8();
    xterm_return_obeys_lnm();
    amiga_keys_use_the_8bit_csi();
    pcansi_keys_are_plain_ansi();
    bracketed_paste_only_when_asked();
    meta_escape_and_modify_other_keys();
    utf8_mouse_reaches_past_column_223();
    alt_backspace_return_tab_escape_send_the_esc_prefix();
    modify_other_keys_reports_modified_return_and_tab();
}
