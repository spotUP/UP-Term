/* vterm_dump COLS ROWS < stream: libvterm's screen in vtdump's format (text
 * rows, then fg,bg,attr per cell in the engine's encoding, then @x,y).
 * The second reference for the xterm personality; built against a libvterm
 * source tree in build/third_party (make build/vterm_dump). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vterm.h"

static int utf8(char *o, unsigned c)
{
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F));
    o[2] = (char)(0x80 | (c & 0x3F));
    return 3;
}

static unsigned colour(VTermColor *c, int is_fg)
{
    if ((is_fg && VTERM_COLOR_IS_DEFAULT_FG(c)) || (!is_fg && VTERM_COLOR_IS_DEFAULT_BG(c)))
        return 0x100;
    if (VTERM_COLOR_IS_INDEXED(c))
        return c->indexed.idx;
    return 0x8000 | ((c->rgb.red >> 3) << 10) | ((c->rgb.green >> 3) << 5) | (c->rgb.blue >> 3);
}

int main(int argc, char **argv)
{
    static char buf[65536];
    int rows, cols, x, y;
    size_t n;
    VTerm *vt;
    VTermScreen *scr;
    VTermPos pos;
    if (argc < 3)
        return 2;
    cols = atoi(argv[1]);
    rows = atoi(argv[2]);
    vt = vterm_new(rows, cols);
    vterm_set_utf8(vt, 1);
    scr = vterm_obtain_screen(vt);
    vterm_screen_enable_altscreen(scr, 1);
    vterm_screen_reset(scr, 1);
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0)
        vterm_input_write(vt, buf, n);
    vterm_screen_flush_damage(scr);
    for (y = 0; y < rows; y++) {
        char line[8192];
        int len = 0;
        for (x = 0; x < cols; x++) {
            VTermScreenCell c;
            pos.row = y;
            pos.col = x;
            vterm_screen_get_cell(scr, pos, &c);
            if (c.width == 0 || (x > 0 && c.chars[0] == (uint32_t)-1))
                continue;
            len += utf8(line + len, c.chars[0] ? c.chars[0] : ' ');
            if (c.width == 2)
                x++;
        }
        line[len] = 0;
        printf("%s\n", line);
    }
    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            VTermScreenCell c;
            unsigned a = 0;
            pos.row = y;
            pos.col = x;
            vterm_screen_get_cell(scr, pos, &c);
            if (c.attrs.bold) a |= 1;
            if (c.attrs.underline) a |= 8;
            if (c.attrs.reverse) a |= 0x20;
            printf("%s%u,%u,%u,%u", x ? " " : "", colour(&c.fg, 1), colour(&c.bg, 0), a,
                   (unsigned)(c.chars[0] && c.chars[0] != (uint32_t)-1 ? c.chars[0] : 32));
        }
        printf("\n");
    }
    vterm_state_get_cursorpos(vterm_obtain_state(vt), &pos);
    printf("@%d,%d\n", pos.col < cols ? pos.col : cols - 1, pos.row);
    vterm_free(vt);
    return 0;
}
