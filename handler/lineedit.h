/* The cooked-mode line editor of the console: what the user edits before
 * Return hands the line to the program. Portable C on top of the engine,
 * so the host tests drive it key by key and read the screen back.
 *
 * Keys (Amiga con-handler conventions, plus what modern shells taught):
 *   Left/Right, Shift+Left/Right or Ctrl-A/Ctrl-E (start/end),
 *   Ctrl+Left/Right or Meta-B/Meta-F (word back/forward), Backspace, Del,
 *   Ctrl-X (kill line), Ctrl-K (kill to end), Ctrl-U (kill to start),
 *   Ctrl-W or Meta-Backspace (kill word back), Meta-D (kill word forward),
 *   Up/Down (history), Shift+Up/Down (history entries starting with what
 *   is typed), Ctrl-R (incremental search, again for older; Return runs,
 *   Ctrl-G cancels, any movement keeps the found line), Right or End at
 *   the end of the line (take the grey suggestion), Ctrl-_ (undo),
 *   Ctrl-L (clear the window, keep prompt and line), Return.
 * Meta is Left Amiga + key (the handler's choice: Alt is the keymap's). */
#ifndef LINEEDIT_H
#define LINEEDIT_H
#include "../engine/vtengine.h"

#define LE_MAX 1024
#define LE_HIST 100
#define LE_HIST_LEN 256
#define LE_UNDO 8

typedef struct le_state {
    unsigned char buf[LE_MAX];
    int len, pos;
} le_state;

typedef struct le_line {
    vt_term *t;
    unsigned char buf[LE_MAX];
    int len, pos;           /* bytes; pos is on a character boundary */
    int shown;              /* cells the line (and its grey tail) took on screen */
    long start_row;         /* absolute row (grid row + vt_lines_scrolled) */
    int start_col;
    int started;
    int utf8;               /* characters are UTF-8 (xterm personality) */
    int suggest;            /* show history suggestions (default on) */
    unsigned char hist[LE_HIST][LE_HIST_LEN];
    int hist_n, hist_pos;
    /* Ctrl-R incremental search */
    int searching;
    unsigned char pat[64];
    int pat_len, search_idx;
    le_state before_search;
    /* undo */
    le_state undo[LE_UNDO];
    int undo_n, typing;     /* typing: the last change was an inserted char */
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
/* History from outside (a saved history file): one line per call, oldest
 * first; empty lines and repeats of the last entry are skipped. */
void le_hist_add(le_line *le, const unsigned char *s, int n);

#endif
