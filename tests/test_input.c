/* The window's input policy (render/vtinput): wheel, buttons, motion,
 * clicks, console keys, paste -- what render/vtwin.c does with each event. */
#include "harness.h"
#include "../render/vtinput.h"

/* an 80 x 24 text area of 8 x 8 cells, 4 pixels in, 11 down (a window's
 * border and title bar) */
static vti_geom geom(void)
{
    vti_geom g;
    g.ox = 4;
    g.oy = 11;
    g.cw = 8;
    g.ch = 8;
    g.cols = 80;
    g.rows = 24;
    return g;
}

static const char *wheel(vt_term *t, int view, int up, int px, int py, int *lines)
{
    static char buf[40];
    vti_geom g = geom();
    int n = vti_wheel(&g, t, view, up, 0, px, py, (vt_u8 *)buf, lines);
    buf[n] = 0;
    return buf;
}

/* gap #1: the report named window pixels as cells (tmux picked the wrong pane,
 * and past pixel 222 the legacy form fell back to UP-Term's scrollback) */
static void wheel_reports_name_the_cell_under_the_pointer(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    int lines, x = -1, y = -1;
    vti_geom g = geom();
    CHECK_INT(vti_cell_at(&g, 4 + 8 * 30 + 3, 11 + 8 * 10 + 7, &x, &y), 1);
    CHECK_INT(x, 30);
    CHECK_INT(y, 10);
    CHECK_INT(vti_cell_at(&g, 3, 20, &x, &y), 0);
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");        /* no mouse mode: the scrollback */
    CHECK_INT(lines, VTI_WHEEL_LINES);
    h_put(t, "\033[?1000h\033[?1006h");
    /* pixel (300, 200) is cell (37, 23): SGR says 38;24 */
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "\033[<64;38;24M");
    CHECK_STR(wheel(t, 0, 0, 4, 11, &lines), "\033[<65;1;1M");
    h_put(t, "\033[?1006l");
    /* the legacy form: pixel 600 is cell 74, within one byte (the pixel was not) */
    CHECK_STR(wheel(t, 0, 1, 600, 12, &lines), "\033[M`k!");
    CHECK_STR(wheel(t, 0, 1, 2, 2, &lines), "\033[M`!!");     /* over the border: the nearest cell */
    CHECK_STR(wheel(t, 5, 1, 300, 200, &lines), "");         /* a scrolled-back view keeps it */
    CHECK_INT(lines, VTI_WHEEL_LINES);
    CHECK_STR(wheel(t, 5, 0, 300, 200, &lines), "");
    CHECK_INT(lines, -VTI_WHEEL_LINES);
    vt_free(t);
}

/* gap #8: less, man, git log on the alternate screen scroll with the wheel
 * once the program sets ?1007 (xterm's alternateScroll; off until asked) */
static void wheel_on_the_alternate_screen_sends_cursor_keys(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    int lines;
    h_put(t, "\033[?1049h");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");             /* xterm's default: off */
    h_reply_clear();
    h_put(t, "\033[?1007$p");
    CHECK_STR(h_reply, "\033[?1007;2$y");
    h_put(t, "\033[?1007h");
    h_reply_clear();
    h_put(t, "\033[?1007$p");
    CHECK_STR(h_reply, "\033[?1007;1$y");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "\033[A\033[A\033[A");
    CHECK_STR(wheel(t, 0, 0, 300, 200, &lines), "\033[B\033[B\033[B");
    h_put(t, "\033[?1h");                                          /* DECCKM: as the keys say it */
    CHECK_STR(wheel(t, 0, 0, 300, 200, &lines), "\033OB\033OB\033OB");
    h_put(t, "\033[?1000h\033[?1006h");                            /* a mouse mode wins */
    CHECK_STR(wheel(t, 0, 1, 4, 11, &lines), "\033[<64;1;1M");
    h_put(t, "\033[?1000l\033[?1049l");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");             /* the main screen: scrollback */
    CHECK_INT(lines, VTI_WHEEL_LINES);
    h_put(t, "\033[?1049h\033c");                                  /* RIS: off again */
    h_put(t, "\033[?1049h");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");
    vt_free(t);
}

static const char *button(vti_mouse *m, vt_term *t, int btn, int down, int x, int y, int mods,
                          int selecting, int *action)
{
    static char buf[40];
    int n = 0;
    *action = vti_button(m, t, btn, down, x, y, 1, mods, selecting, 0, (vt_u8 *)buf, &n);
    buf[n] = 0;
    return buf;
}

static const char *motion(vti_mouse *m, vt_term *t, int x, int y)
{
    static char buf[40];
    int n = vti_motion(m, t, x, y, 0, (vt_u8 *)buf);
    buf[n] = 0;
    return buf;
}

/* gap #3: vim visual drag, tmux pane resize, htop/fzf drag need ?1002 / ?1003 */
static void motion_reaches_the_program_in_the_motion_modes(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    vti_mouse m;
    int a;
    vti_mouse_reset(&m);
    h_put(t, "\033[?1006h\033[?1000h");
    CHECK_STR(button(&m, t, 0, 1, 2, 3, 0, 0, &a), "\033[<0;3;4M");
    CHECK_INT(a, VTI_REPORT);
    CHECK_INT(vti_wants_motion(&m, t), 0);                   /* ?1000: clicks only */
    CHECK_STR(motion(&m, t, 5, 3), "");
    CHECK_STR(button(&m, t, 0, 0, 5, 3, 0, 0, &a), "\033[<0;6;4m");
    h_put(t, "\033[?1002h");
    CHECK_INT(vti_wants_motion(&m, t), 0);                   /* no button down */
    CHECK_STR(motion(&m, t, 6, 3), "");
    CHECK_STR(button(&m, t, 0, 1, 6, 3, 0, 0, &a), "\033[<0;7;4M");
    CHECK_INT(vti_wants_motion(&m, t), 1);
    CHECK_STR(motion(&m, t, 6, 3), "");                      /* the same cell: once */
    CHECK_STR(motion(&m, t, 7, 3), "\033[<32;8;4M");
    CHECK_STR(motion(&m, t, 7, 4), "\033[<32;8;5M");
    /* the release goes to the program even off the grid: the last cell */
    a = vti_button(&m, t, 0, 0, 0, 0, 0, 0, 0, 0, (vt_u8 *)h_reply, &h_reply_len);
    CHECK_INT(a, VTI_REPORT);
    h_reply[h_reply_len] = 0;
    CHECK_STR(h_reply, "\033[<0;8;5m");
    CHECK_INT(vti_wants_motion(&m, t), 0);
    h_put(t, "\033[?1003h");
    CHECK_INT(vti_wants_motion(&m, t), 1);                   /* any motion, no button */
    CHECK_STR(motion(&m, t, 10, 10), "\033[<35;11;11M");
    CHECK_STR(motion(&m, t, 10, 10), "");
    h_put(t, "\033[?1003l\033[?1002l");
    CHECK_STR(motion(&m, t, 11, 10), "");
    vt_free(t);
}

/* ?1002 never reports a move with no button down, even when asked to */
static void button_motion_mode_has_no_buttonless_moves(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    char buf[40];
    int n;
    h_put(t, "\033[?1002h\033[?1006h");
    n = vt_encode_mouse(t, 3, 2, 1, 1, 0, (vt_u8 *)buf);
    CHECK_INT(n, 0);
    h_put(t, "\033[?1003h");
    n = vt_encode_mouse(t, 3, 2, 1, 1, 0, (vt_u8 *)buf);
    buf[n] = 0;
    CHECK_STR(buf, "\033[<35;2;2M");
    vt_free(t);
}

/* without a mouse mode (or with Shift) the left button selects, as before */
static void the_left_button_selects_when_nobody_asked(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    vti_mouse m;
    int a;
    vti_mouse_reset(&m);
    CHECK_STR(button(&m, t, 0, 1, 2, 3, 0, 0, &a), "");
    CHECK_INT(a, VTI_SELECT);
    CHECK_STR(button(&m, t, 0, 0, 4, 3, 0, 1, &a), "");
    CHECK_INT(a, VTI_SELECT_END);
    h_put(t, "\033[?1000h");
    CHECK_STR(button(&m, t, 0, 1, 2, 3, VT_MOD_SHIFT, 0, &a), "");
    CHECK_INT(a, VTI_SELECT);                                /* Shift: the mouse is ours */
    CHECK_STR(button(&m, t, 0, 0, 2, 3, 0, 1, &a), "");
    CHECK_INT(a, VTI_SELECT_END);                            /* the drag that began ends here */
    vt_free(t);
}

/* gap #14 / audit 6: no middle button, and the modifiers were always 0 */
static void middle_button_and_modifiers_reach_the_program(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    vti_mouse m;
    int a, btn = -1, down = -1, lines;
    char buf[40];
    vti_geom g = geom();
    vti_mouse_reset(&m);
    CHECK_INT(vti_button_code(VTI_CODE_MBUTTON, &btn, &down), 1);
    CHECK_INT(btn, 1);
    CHECK_INT(down, 1);
    CHECK_INT(vti_button_code(VTI_CODE_MBUTTON | VTI_CODE_UP, &btn, &down), 1);
    CHECK_INT(down, 0);
    CHECK_INT(vti_button_code(VTI_CODE_LBUTTON, &btn, &down), 1);
    CHECK_INT(btn, 0);
    CHECK_INT(vti_button_code(VTI_CODE_RBUTTON | VTI_CODE_UP, &btn, &down), 1);
    CHECK_INT(btn, 2);
    CHECK_INT(vti_button_code(0x45, &btn, &down), 0);
    CHECK_INT(vti_mods(VTI_QUAL_CONTROL, 0), VT_MOD_CTRL);
    CHECK_INT(vti_mods(VTI_QUAL_LCOMMAND, 0), VT_MOD_ALT);             /* Left Amiga is Meta */
    CHECK_INT(vti_mods(0x0010, 0), 0);                                  /* Alt is the keymap's */
    CHECK_INT(vti_mods(0x0020 | 0x0002, 1), VT_MOD_ALT | VT_MOD_SHIFT); /* meta_alt */
    CHECK_INT(vti_mods(VTI_QUAL_LCOMMAND, 1), 0);
    h_put(t, "\033[?1000h\033[?1006h");
    CHECK_STR(button(&m, t, 1, 1, 2, 3, 0, 0, &a), "\033[<1;3;4M");
    CHECK_STR(button(&m, t, 1, 0, 2, 3, 0, 0, &a), "\033[<1;3;4m");
    CHECK_STR(button(&m, t, 0, 1, 2, 3, vti_mods(VTI_QUAL_CONTROL, 0), 0, &a), "\033[<16;3;4M");
    CHECK_STR(button(&m, t, 0, 0, 2, 3, vti_mods(VTI_QUAL_LCOMMAND, 0), 0, &a), "\033[<8;3;4m");
    a = vti_wheel(&g, t, 0, 1, VT_MOD_CTRL, 4, 11, (vt_u8 *)buf, &lines);
    buf[a] = 0;
    CHECK_STR(buf, "\033[<80;1;1M");
    vt_free(t);
}

/* gap #14: double-click selects a word, triple-click the line */
static void double_click_word_triple_click_line(void)
{
    vt_term *t = h_new(20, 4, VT_XTERM);
    vti_mouse m;
    int a, n, x0 = -1, x1 = -1, y0 = -1, y1 = -1;
    vt_u8 buf[40];
    vti_mouse_reset(&m);
    h_put(t, "ls ~/src/vt-con.c (x)  ");
    /* the run of clicks: on the same cell, each within the double-click time */
    CHECK_INT(vti_button(&m, t, 0, 1, 6, 0, 1, 0, 0, 1, buf, &n), VTI_SELECT);
    CHECK_INT(m.clicks, 1);                       /* the first click has nothing before it */
    vti_button(&m, t, 0, 0, 6, 0, 1, 0, 1, 0, buf, &n);
    CHECK_INT(vti_button(&m, t, 0, 1, 6, 0, 1, 0, 0, 1, buf, &n), VTI_SELECT);
    CHECK_INT(m.clicks, 2);
    vti_button(&m, t, 0, 0, 6, 0, 1, 0, 1, 0, buf, &n);
    vti_button(&m, t, 0, 1, 6, 0, 1, 0, 0, 1, buf, &n);
    CHECK_INT(m.clicks, 3);
    vti_button(&m, t, 0, 0, 6, 0, 1, 0, 1, 0, buf, &n);
    vti_button(&m, t, 0, 1, 6, 0, 1, 0, 0, 1, buf, &n);
    CHECK_INT(m.clicks, 1);                       /* a fourth starts over */
    vti_button(&m, t, 0, 0, 6, 0, 1, 0, 1, 0, buf, &n);
    vti_button(&m, t, 0, 1, 7, 0, 1, 0, 0, 1, buf, &n);
    CHECK_INT(m.clicks, 1);                       /* another cell: a new run */
    vti_button(&m, t, 0, 0, 7, 0, 1, 0, 1, 0, buf, &n);
    vti_button(&m, t, 0, 1, 7, 0, 1, 0, 0, 0, buf, &n);
    CHECK_INT(m.clicks, 1);                       /* too slow: a new run */
    a = vti_button(&m, t, 0, 0, 7, 0, 1, 0, 1, 0, buf, &n);
    CHECK_INT(a, VTI_SELECT_END);
    /* the word: a path counts as one, brackets and blanks end it */
    CHECK_INT(vti_word(t, 6, 0, &x0, &x1), 1);
    CHECK_INT(x0, 3);
    CHECK_INT(x1, 16);                            /* ~/src/vt-con.c */
    CHECK_INT(vti_word(t, 19, 0, &x0, &x1), 1);
    CHECK_INT(x0, 19);                            /* "(x)  " wrapped: ( on col 18 */
    vti_word(t, 18, 0, &x0, &x1);
    CHECK_INT(x0, 18);
    CHECK_INT(x1, 18);                            /* a bracket stands alone */
    vti_word(t, 0, 1, &x0, &x1);
    CHECK_INT(x0, 0);
    CHECK_INT(x1, 0);                             /* ")" */
    vti_word(t, 2, 1, &x0, &x1);
    CHECK_INT(x0, 1);                             /* a run of blanks */
    CHECK_INT(vti_word(t, 0, 9, &x0, &x1), 0);    /* no such row */
    /* the line: the rows a wrap joined */
    vti_line(t, 1, &y0, &y1);
    CHECK_INT(y0, 0);
    CHECK_INT(y1, 1);
    vti_line(t, 3, &y0, &y1);
    CHECK_INT(y0, 3);
    CHECK_INT(y1, 3);
    vt_free(t);
}

/* gap #14: middle-click pastes (on the release, as xterm) when no program
 * took the mouse, or with Shift */
static void middle_click_pastes_when_nobody_asked(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    vti_mouse m;
    int a;
    vti_mouse_reset(&m);
    CHECK_STR(button(&m, t, 1, 1, 2, 3, 0, 0, &a), "");
    CHECK_INT(a, VTI_NONE);
    CHECK_STR(button(&m, t, 1, 0, 2, 3, 0, 0, &a), "");
    CHECK_INT(a, VTI_PASTE);
    h_put(t, "\033[?1000h\033[?1006h");
    button(&m, t, 1, 1, 2, 3, 0, 0, &a);
    CHECK_INT(a, VTI_REPORT);
    button(&m, t, 1, 0, 2, 3, 0, 0, &a);
    CHECK_INT(a, VTI_REPORT);                     /* the program's middle button */
    button(&m, t, 1, 1, 2, 3, VT_MOD_SHIFT, 0, &a);
    CHECK_INT(a, VTI_NONE);
    button(&m, t, 1, 0, 2, 3, VT_MOD_SHIFT, 0, &a);
    CHECK_INT(a, VTI_PASTE);                      /* Shift: ours */
    CHECK_STR(button(&m, t, 2, 0, 2, 3, VT_MOD_SHIFT, 0, &a), "");
    CHECK_INT(a, VTI_NONE);                       /* the right button pastes nothing */
    vt_free(t);
}

/* gap #12: terminfo's kPRV / kNXT never reached a program -- the window
 * always took Shift+PgUp/PgDn for its scrollback */
static void shift_page_keys_reach_full_screen_programs(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    char buf[40];
    int n;
    CHECK_INT(vti_page_keys_scroll(t), 1);         /* the shell: the scrollback */
    h_put(t, "\033[?1049h");
    CHECK_INT(vti_page_keys_scroll(t), 0);         /* vim, less: theirs */
    n = vt_encode_key(t, VT_KEY_PAGE_UP, VT_MOD_SHIFT, (vt_u8 *)buf);
    buf[n] = 0;
    CHECK_STR(buf, "\033[5;2~");
    h_put(t, "\033[?1049l\033[?1000h");
    CHECK_INT(vti_page_keys_scroll(t), 0);         /* a program holding the mouse */
    h_put(t, "\033[?1000l");
    CHECK_INT(vti_page_keys_scroll(t), 1);
    vt_free(t);
}

/* audit 1, ?2004: a pasted ESC [ 201 ~ ended the brackets early and the
 * rest ran as typed commands */
static void bracketed_paste_drops_escape_and_controls(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    CHECK_INT(vti_paste_keeps(t, 0x1B), 1);       /* no brackets: typed as it is */
    h_put(t, "\033[?2004h");
    CHECK_INT(vti_paste_keeps(t, 0x1B), 0);
    CHECK_INT(vti_paste_keeps(t, 0x03), 0);
    CHECK_INT(vti_paste_keeps(t, 0x7F), 0);
    CHECK_INT(vti_paste_keeps(t, 0x9B), 0);       /* C1 CSI */
    CHECK_INT(vti_paste_keeps(t, '\t'), 1);
    CHECK_INT(vti_paste_keeps(t, '\n'), 1);
    CHECK_INT(vti_paste_keeps(t, '\r'), 1);
    CHECK_INT(vti_paste_keeps(t, '['), 1);
    CHECK_INT(vti_paste_keeps(t, 0xE5), 1);
    vt_free(t);
}

static void a_drag_step_repaints_the_rows_whose_span_changed(void)
{
    long y0 = -1, y1 = -1;
    /* 100 rows selected; the end moves one cell on its own row: one row */
    CHECK_INT(vti_sel_dirty(1, 5, 10, 20, 110, 1, 5, 10, 21, 110, &y0, &y1), 1);
    CHECK_INT(y0, 110);
    CHECK_INT(y1, 110);
    /* the end moves down two rows: the old end row to the new one */
    CHECK_INT(vti_sel_dirty(1, 5, 10, 20, 110, 1, 5, 10, 3, 112, &y0, &y1), 1);
    CHECK_INT(y0, 110);
    CHECK_INT(y1, 112);
    /* dragging up past the anchor (ends swap): any order of ends is the same selection */
    CHECK_INT(vti_sel_dirty(1, 20, 110, 5, 10, 1, 5, 10, 20, 110, &y0, &y1), 0);
    /* the start end moves up three rows: those rows plus the old start row */
    CHECK_INT(vti_sel_dirty(1, 5, 10, 20, 110, 1, 5, 7, 20, 110, &y0, &y1), 1);
    CHECK_INT(y0, 7);
    CHECK_INT(y1, 10);
    /* nothing moved: nothing to paint */
    CHECK_INT(vti_sel_dirty(1, 5, 10, 20, 110, 1, 5, 10, 20, 110, &y0, &y1), 0);
    /* a new selection paints its rows; a cleared one paints the old rows */
    CHECK_INT(vti_sel_dirty(0, 0, 0, 0, 0, 1, 2, 4, 9, 6, &y0, &y1), 1);
    CHECK_INT(y0, 4);
    CHECK_INT(y1, 6);
    CHECK_INT(vti_sel_dirty(1, 2, 4, 9, 6, 0, 0, 0, 0, 0, &y0, &y1), 1);
    CHECK_INT(y0, 4);
    CHECK_INT(y1, 6);
    CHECK_INT(vti_sel_dirty(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &y0, &y1), 0);
}

void suite_input(void)
{
    a_drag_step_repaints_the_rows_whose_span_changed();
    bracketed_paste_drops_escape_and_controls();
    shift_page_keys_reach_full_screen_programs();
    middle_click_pastes_when_nobody_asked();
    double_click_word_triple_click_line();
    middle_button_and_modifiers_reach_the_program();
    motion_reaches_the_program_in_the_motion_modes();
    button_motion_mode_has_no_buttonless_moves();
    the_left_button_selects_when_nobody_asked();
    wheel_reports_name_the_cell_under_the_pointer();
    wheel_on_the_alternate_screen_sends_cursor_keys();
}
