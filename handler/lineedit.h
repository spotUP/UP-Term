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
    /* the first word as a command: 0 not known, 1 found, 2 not found;
     * valid while the first word is still cmd_word */
    int cmd_state;
    unsigned char cmd_word[64];
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
/* The first word of the line (up to the first space): its length, and
 * whether the colouring is still unknown for it. */
int  le_first_word(const le_line *le, unsigned char *out, int max);
/* The answer to "is the first word a command": green or red on screen,
 * applied only if the first word is still `word`. */
void le_set_command(le_line *le, const unsigned char *word, int found);
/* A completion menu: the names (NUL-separated) in columns under the line,
 * then prompt and line again below them. */
void le_show_list(le_line *le, const char *names, int len);
/* Replace the word ending at the cursor (from `from`) with `s`. */
void le_replace_word(le_line *le, int from, const unsigned char *s, int n);

/* History from outside (a saved history file): one line per call, oldest
 * first; empty lines and repeats of the last entry are skipped. */
/* KingCON's completion word (research/2026-10-02_kingcon-completion.md):
 * after an odd number of '"' before the cursor the word starts after the
 * last one (*quote_at = its index, spaces allowed); otherwise it runs back
 * to a space , > < or backtick (*quote_at = -1). Returns its start. */
int  le_kc_word(const le_line *le, int *quote_at);
/* Put entry (n bytes, in the line's encoding, ending in its suffix: ' '
 * for a file, '/' for a directory, ':' for a device) in place of the file
 * part of that word (after its last '/' or ':'), KingCON's way: when the
 * word was opened with '"' or the result holds a space it is quoted --
 * an opening '"' added at the word's start if there was none, and a
 * file's trailing space becoming '" '; a directory stays open. The text
 * after the cursor stays. */
void le_kc_insert(le_line *le, int start, int quote_at, const unsigned char *entry, int n);
/* One step of KingCON's inline cycle: the line as it was before the cycle
 * (snap, the snap_pos bytes before the cursor; what follows the cursor is
 * the line's own and stays) and entry put in as le_kc_insert does -- so a
 * quote one entry needed is gone again when the next does not. */
void le_kc_redo(le_line *le, const unsigned char *snap, int snap_pos, int start, int quote_at,
                const unsigned char *entry, int n);
/* KingCON's FNCMODE letters (W window, L list, B cycle, C common prefix
 * first, S silent; any case, other characters ignored) as LE_KC_* bits.
 * W clears L and B, as in KingCON. 0 letters: W. */
#define LE_KC_WINDOW 1
#define LE_KC_LIST   2
#define LE_KC_CYCLE  4
#define LE_KC_COMMON 8
#define LE_KC_SILENT 16
int  le_kc_fncmode(const char *letters);
void le_hist_add(le_line *le, const unsigned char *s, int n);

#endif
