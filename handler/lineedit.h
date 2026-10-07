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
/* History and undo are kept packed, each in one block that grows as lines
 * come (LE_MALLOC: AllocVec in the handler): the same 100 lines of up to
 * 255 bytes and 8 snapshots of up to LE_MAX bytes as before, but a line
 * costs its length, not the largest one -- 34 KB a window less, nothing
 * allocated until a line is entered (research/2026-10-04_window-memory.md).
 * le_free gives the blocks back. */
#define LE_HIST_BYTES ((long)LE_HIST * LE_HIST_LEN)
#define LE_UNDO_BYTES ((long)LE_UNDO * LE_MAX)

typedef struct le_state {
    unsigned char buf[LE_MAX];
    int len, pos;
} le_state;

/* An undo snapshot: len bytes at undo_buf + at, the cursor at pos. */
typedef struct le_snap {
    long at;
    int len, pos;
} le_snap;

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
    unsigned char *hist;    /* the lines, each NUL-terminated, oldest first (LE_MALLOC) */
    long hist_used, hist_cap;
    unsigned short hist_at[LE_HIST]; /* where line i starts in hist */
    int hist_n, hist_pos;
    /* Ctrl-R incremental search */
    int searching;
    unsigned char pat[64];
    int pat_len, search_idx;
    le_state before_search;
    /* undo: a stack of snapshots, their bytes packed in undo_buf (LE_MALLOC) */
    le_snap undo[LE_UNDO];
    unsigned char *undo_buf;
    long undo_used, undo_cap;
    int undo_n, typing;     /* typing: the last change was an inserted char */
    /* the first word as a command: 0 not known, 1 found, 2 not found;
     * valid while the first word is still cmd_word */
    int cmd_state;
    unsigned char cmd_word[64];
    /* W44: the first word is being typed -- what the keys must still rest
     * before its colour shows (0: it shows once its answer is in) -- and
     * the colour its cells have on screen (0 plain, as cmd_state) */
    long cmd_rest_us;
    int cmd_drawn;
    /* what the editor writes to the screen (echo) */
    void (*out)(void *user, const unsigned char *b, long n);
    void *user;
} le_line;

void le_init(le_line *le, vt_term *t, void (*out)(void *, const unsigned char *, long), void *user);
/* The history and undo blocks back (le_init starts without any). Safe on a
 * line le_init made or on an all-zero one; le_init may follow. */
void le_free(le_line *le);
/* One key: `key` as vt_encode_key takes it, `mods` VT_MOD_*, `bytes` what
 * the key encodes to (a character's bytes in the terminal's charset).
 * Returns 1 when Return completed the line: le->buf[0..le->len) then holds
 * it with the '\n', and the caller takes it and calls le_reset. */
int  le_key(le_line *le, long key, int mods, const unsigned char *bytes, int n);
void le_reset(le_line *le);
/* The window was resized: a reflow may have moved the line. Where it
 * starts is worked out again from the cursor, which the engine kept on
 * its character. */
void le_resized(le_line *le);
/* The first word of the line (up to the first space): its length, and
 * whether the colouring is still unknown for it. */
int  le_first_word(const le_line *le, unsigned char *out, int max);
/* The answer to "is the first word a command": green or red on screen,
 * applied only if the first word is still `word`. While the word is being
 * typed it stays plain (the answer kept) until le_command_rested. */
void le_set_command(le_line *le, const unsigned char *word, int found);
/* W44 (owner 2026-10-05: the colour flipped red and green while a word was
 * typed): a key that changes the first word with the cursor in it shows
 * the word plain and starts a rest of LE_CMD_REST_US; a key that leaves
 * the word (a space, Return, a move out of it) ends the rest at once, and
 * a coloured word nobody edits keeps its colour. */
#define LE_CMD_REST_US 300000L
/* The caller's clock waited waited_us (the frame clock, as le_menu_rested):
 * once the keys have rested LE_CMD_REST_US, the word's colour goes on
 * screen -- its cells only -- if its answer is in (else when it comes).
 * 1 when the rest ended, then 0 until a key starts another. */
int  le_command_rested(le_line *le, long waited_us);
/* The word was put in, not typed (a completion, a stuffed line): no rest,
 * its colour at once. */
void le_command_now(le_line *le);
/* The line is read by a program, not a shell: the first word plain again
 * (W31: C:Claude's "hello" showed red as an unknown command). */
void le_no_command(le_line *le);
/* A completion menu: the names (NUL-separated) in columns under the line,
 * then prompt and line again below them. */
void le_show_list(le_line *le, const char *names, int len);
/* KingCON's printed list (FNCMODE L, Ctrl+D): 19-character columns,
 * (width + 1) / 19 of them where width is the last column's index (at
 * least one), a name over 18 characters (its suffix counted) cut to 15
 * and "..." (research/2026-10-02_kingcon-completion.md). */
void le_kc_show_list(le_line *le, const char *names, int len);
/* Replace the word ending at the cursor (from `from`) with `s`. */
void le_replace_word(le_line *le, int from, const unsigned char *s, int n);

/* V47 medium mode (SetMode 2): the report a key makes for the Shell, or 0
 * when the key is an editing key. TAB, Shift+TAB, Up and Down (no other
 * modifier on Up and Down) give CSI(0x9b) code;length;cursor+1 U -- codes
 * 12, 13, 2, 3 -- into out (LE_MEDIUM_MAX bytes; the result is the byte
 * count) and the caller sends the line (le->buf, le->len) after it. The
 * Shell answers with ACTION_FORCE. */
#define LE_MEDIUM_MAX 24
int le_medium_report(const le_line *le, long key, int mods, unsigned char *out);

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
/* The list for a shell's `history` builtin (ACTION_VTCON_HISTORY): the lines are numbered from 0, oldest first.
 * le_hist_get copies line i (NUL-terminated, cut to max-1) into buf and returns its length, -1 if there is none;
 * le_hist_del removes line i (0 ok, -1 none); le_hist_clear empties the list. The saved history file is not
 * touched by any of them. */
int  le_hist_count(const le_line *le);
int  le_hist_get(const le_line *le, int i, unsigned char *buf, int max);
int  le_hist_del(le_line *le, int i);
void le_hist_clear(le_line *le);

/* A list to choose from with the keys, drawn under a finished line (W30:
 * /theme with no name): one name a row, the chosen one as a reverse bar,
 * the marked one (what the window has now) with a '*', a faint row under
 * them ("k of n: " and the keys); past LE_MENU_ROWS names it scrolls. Keys: Up and
 * Down (both ends wrap), Shift+Up/Down or Page Up/Down a page, Home/End,
 * a letter the next name starting with it, Return or Enter takes it,
 * Escape or Ctrl-G cancels. The menu is the caller's (the line editor is
 * not in it): the caller routes keys to le_menu_key while m->open. */
#define LE_MENU_ROWS 12
/* How long the bar rests before le_menu_rested says so (microseconds). */
#define LE_MENU_REST_US 250000L
typedef struct le_menu {
    const char *names;  /* NUL-separated, n of them: the caller's, kept while open */
    int n, sel, mark;   /* mark -1: none */
    int top, rows;      /* the first name shown, the rows of names shown */
    long at;            /* the first row, absolute (grid row + vt_lines_scrolled) */
    int open;
    int w;              /* the bar's width: the widest name, cut to the window */
    int drawn_top, drawn_sel; /* what the screen shows; drawn_top -1: nothing (the
                         * next le_menu_draw is whole -- set it when the screen
                         * lost the list, a resize) */
    long rest_us;       /* the bar moved: what it must still rest (0: nothing due) */
} le_menu;
enum { LE_MENU_NONE, LE_MENU_MOVED, LE_MENU_TAKE, LE_MENU_CANCEL };
/* Open at the cursor (a row's start: the line was ended), drawn; sel the
 * name chosen first. Nothing opens for n 0. */
void le_menu_open(le_line *le, le_menu *m, const char *names, int n, int sel, int mark);
/* One key, as le_key takes it: what it did to m (no drawing: MOVED wants
 * le_menu_draw). */
int  le_menu_key(le_menu *m, long key, int mods, const unsigned char *b, int nb);
/* What changed since the last draw (owner 2026-10-05: the whole list a key
 * was slow): the row the bar left, the row it is on, and the "k of n" row.
 * Names scrolled by less than a page move with DL / IL (a blit) and only
 * the rows coming in are drawn -- when nothing is under the list, which
 * those would move too; otherwise, and the first time, the whole list. */
void le_menu_draw(le_line *le, le_menu *m);
/* The caller's clock waited waited_us. 1 once the bar has rested
 * LE_MENU_REST_US since it last moved, then 0 until it moves again: what
 * is costly for the chosen name goes there, not on every key (/theme put
 * each theme the bar passed on the window, a full repaint each). */
int  le_menu_rested(le_menu *m, long waited_us);
/* Off the screen, the cursor where its first row was (the reader's next
 * prompt goes there); m->open 0, a rest still due cancelled. */
void le_menu_close(le_line *le, le_menu *m);
/* name i */
const char *le_menu_name(const le_menu *m, int i);

#endif
