/* The cooked-mode line editor of the console: what the user edits before
 * Return hands the line to the program. Portable C on top of the engine,
 * so the host tests drive it key by key and read the screen back.
 *
 * Keys (Amiga con-handler conventions, plus the Unix ones people expect):
 *   Left/Right, Shift+Left/Right or Ctrl-A/Ctrl-E (start/end), Backspace,
 *   Del, Ctrl-X (kill line), Ctrl-K (kill to end), Ctrl-U (kill to start),
 *   Ctrl-W (kill word back), Up/Down (history), Shift+Up/Down (history
 *   entries starting with what is typed), Return. */
#ifndef LINEEDIT_H
#define LINEEDIT_H
#include "../engine/vtengine.h"

#define LE_MAX 1024
#define LE_HIST 20

typedef struct le_line {
    vt_term *t;
    unsigned char buf[LE_MAX];
    int len, pos;           /* bytes; pos is on a character boundary */
    int shown;              /* cells the line took on screen last time */
    long start_row;         /* absolute row (grid row + vt_lines_scrolled) */
    int start_col;
    int started;
    int utf8;               /* characters are UTF-8 (xterm personality) */
    unsigned char hist[LE_HIST][LE_MAX / 4];
    int hist_n, hist_pos;
    /* what the editor writes to the screen (echo) */
    void (*out)(void *user, const unsigned char *b, long n);
    void *user;
} le_line;

void le_init(le_line *le, vt_term *t, void (*out)(void *, const unsigned char *, long), void *user);
/* One key: `key` as vt_encode_key takes it, `mods` VT_MOD_*, `bytes` what
 * the key encodes to (a character's bytes in the terminal's charset).
 * Returns 1 when Return completed the line: le->buf[0..le->len) then holds
 * it with the '\n', and the caller takes it and calls le_reset. */
int  le_key(le_line *le, long key, int mods, const unsigned char *bytes, int n);
void le_reset(le_line *le);

#endif
