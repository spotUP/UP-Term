/* keys -- the bytes a terminal sends for keys, decoded for C:Claude's own
 * line editor (ledger A3). Raw mode hands the client everything the
 * window's keyboard encoder (engine vt_encode_key) makes:
 *   - characters as UTF-8 (a byte that is no valid UTF-8 is Latin-1);
 *   - C0 controls: ^A..^Z, Tab, Return (CR), ^J (LF), Backspace (DEL/BS), Esc;
 *   - CSI / SS3 sequences, 7-bit (ESC [) or 8-bit (0x9B), with xterm's
 *     modifier parameter: arrows, Home/End, Insert/Delete, PgUp/PgDn,
 *     Shift+Tab (CSI Z), the Amiga's Shift+arrows (CSI T / S / SP @ / SP A);
 *   - the kitty protocol's CSI code ; mods u (pushed by the client: Esc and
 *     Shift+Enter come through unambiguous) and modifyOtherKeys'
 *     CSI 27 ; mods ; code ~;
 *   - bracketed paste (CSI 200~ .. CSI 201~), delivered as ONE key;
 *   - the cursor position report CSI row ; col R (the answer to DSR 6n).
 * A lone ESC is the Escape key once the caller says no more bytes are
 * coming now (the rest of a sequence always arrives in the same burst).
 * Portable C89, host-tested (tests/test_claude_tui.c). */
#ifndef CL_KEYS_H
#define CL_KEYS_H

#include "json.h"

enum {
    K_NONE, K_CHAR, K_ENTER, K_NEWLINE, K_TAB, K_BTAB, K_BS, K_DEL, K_ESC,
    K_UP, K_DOWN, K_LEFT, K_RIGHT, K_HOME, K_END, K_PGUP, K_PGDN, K_INS,
    K_CTRL,             /* ch: 'a'..'z' (Ctrl+letter, not Tab/Return/^J), '_' (Ctrl+_) */
    K_ALT,              /* ch: the character with Meta (ESC prefix) */
    K_PASTE,            /* text, n: the pasted bytes */
    K_CPR               /* row, col: a cursor position report */
};

#define KM_SHIFT 1
#define KM_ALT   2
#define KM_CTRL  4

typedef struct cl_key {
    int k;
    unsigned long ch;   /* K_CHAR, K_CTRL, K_ALT */
    int mods;           /* KM_* */
    const char *text;   /* K_PASTE: valid until the next keys_* call */
    long n;
    int row, col;       /* K_CPR */
} cl_key;

typedef struct cl_keys {
    unsigned char b[512];
    int n;
    int paste;          /* inside a bracketed paste */
    jw pb;              /* its bytes */
    jw out;             /* the last paste handed out */
} cl_keys;

void keys_init(cl_keys *k);
void keys_free(cl_keys *k);
/* bytes from the terminal */
void keys_feed(cl_keys *k, const char *s, long n);
/* The next key: 1 with *key set, 0 when nothing complete is there.
 * idle: no more bytes are coming now, so a lone ESC is the Escape key and
 * a cut sequence is dropped. */
int keys_next(cl_keys *k, cl_key *key, int idle);

#endif
