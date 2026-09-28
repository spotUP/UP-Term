/* Key encoding per personality. */
#include "harness.h"

static const char *key(vt_term *t, long k, int mods)
{
    static char buf[40];
    int n = vt_encode_key(t, k, mods, (vt_u8 *)buf);
    buf[n] = 0;
    return buf;
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

void suite_keys(void)
{
    mouse_reports_follow_the_modes();
    xterm_cursor_keys_follow_decckm();
    xterm_function_and_editing_keys();
    xterm_characters_are_utf8();
    xterm_return_obeys_lnm();
    amiga_keys_use_the_8bit_csi();
    pcansi_keys_are_plain_ansi();
    bracketed_paste_only_when_asked();
}
