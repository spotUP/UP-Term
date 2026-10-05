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
    K_CPR,              /* row, col: a cursor position report */
    K_FOCUS             /* row: 1 the window got the focus, 0 lost it (?1004's CSI I / CSI O) */
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

/* ---- keybindings (A4 gaps 3) ----
 * Claude Code's keybindings.json (ENVARC:Claude/keybindings.json here): an
 * object whose "bindings" array holds blocks {"context": "Chat",
 * "bindings": {"ctrl+e": "chat:externalEditor", "ctrl+s": null}}. A
 * keystroke is modifiers and a key joined by '+' (ctrl/control, shift,
 * alt/opt/option/meta, cmd/command/super/win), a chord two keystrokes
 * separated by a space (each within 3 s of the one before). null unbinds.
 * The defaults are Claude Code's own (km_init); the file's bindings go over
 * them; problems (bad JSON, an unknown context or action, a reserved key,
 * a misspelled modifier, a duplicate) are collected as warnings and the
 * rest still applies. The screen asks km_action for a key in the contexts
 * active at the moment and does what the action says. */
enum {
    KC_GLOBAL, KC_CHAT, KC_AUTOCOMPLETE, KC_CONFIRM, KC_TRANSCRIPT, KC_HSEARCH, KC_TASK, KC_HELP,
    KC_SELECT, KC_THEME, KC_MSGSEL, KC_COUNT
};

enum {
    KA_NONE,                    /* no binding: the key is the editor's */
    KA_INTERRUPT, KA_EXIT, KA_REDRAW, KA_TODOS, KA_TRANSCRIPT,
    KA_HIST_SEARCH, KA_HIST_PREV, KA_HIST_NEXT,
    KA_CANCEL, KA_CLEAR_INPUT, KA_CLEAR_SCREEN, KA_KILL_AGENTS, KA_CYCLE_MODE, KA_MODEL_PICKER, KA_FAST_MODE,
    KA_THINKING, KA_SUBMIT, KA_QUEUE_SUBMIT, KA_SEND_NOW, KA_NEWLINE, KA_UNDO, KA_EXT_EDITOR, KA_STASH,
    KA_IMAGE_PASTE,
    KA_AC_ACCEPT, KA_AC_DISMISS, KA_AC_PREV, KA_AC_NEXT,
    KA_YES, KA_NO, KA_PREV, KA_NEXT, KA_NEXT_FIELD, KA_PREV_FIELD, KA_TOGGLE, KA_CONFIRM_CYCLE, KA_PERM_DEBUG,
    KA_TR_SHOW_ALL, KA_TR_EXIT,
    KA_HS_NEXT, KA_HS_ACCEPT, KA_HS_CANCEL, KA_HS_EXECUTE, KA_HS_SCOPE,
    KA_TASK_BG, KA_HELP_DISMISS,
    KA_SEL_NEXT, KA_SEL_PREV, KA_SEL_PGUP, KA_SEL_PGDN, KA_SEL_FIRST, KA_SEL_LAST, KA_SEL_ACCEPT, KA_SEL_CANCEL,
    KA_THEME_SYNTAX,
    KA_INERT,                   /* one of Claude Code's actions with nothing to do here (tabs, diff, ...) */
    KA_COUNT,
    KA_PENDING = 100,           /* the first keystroke of a chord: wait for the next */
    KA_CHORD_MISS               /* a chord's second keystroke bound to nothing: both dropped */
};

typedef struct kb_stroke {
    int k;                      /* K_CHAR (ch: a lower-case letter or another character), K_ENTER, K_TAB,
                                 * K_ESC, K_UP ... K_PGDN, K_BS, K_DEL; -1 never matches (cmd+, wheel) */
    unsigned long ch;
    int mods;                   /* KM_* */
} kb_stroke;

typedef struct kb_bind {
    int ctx;                    /* KC_* */
    int act;                    /* KA_*, -1 unbound (null) */
    int n;                      /* keystrokes: 1 or 2 */
    kb_stroke s[2];
} kb_bind;

typedef struct cl_keymap {
    kb_bind *b;                 /* the defaults, then the file's (a later one wins) */
    int n, cap, ndef;
    jw warn;                    /* the file's problems, a line each */
    int nwarn;
    /* a chord begun: its first keystroke, when */
    int pending;
    kb_stroke first;
    unsigned long first_ms;
    int expired;                /* the last chord ran out of time (the screen says so once) */
} cl_keymap;

/* the defaults: 0, -1 out of memory */
int km_init(cl_keymap *m);
/* back to the defaults alone (no file, or --safe-mode) */
void km_reset(cl_keymap *m);
void km_free(cl_keymap *m);
/* back to the defaults, then a keybindings.json's text over them: 0, -1
 * when it is no JSON object (the defaults stay; a warning says why) */
int km_load(cl_keymap *m, const char *json, long n);
/* the action for a key in the active contexts (most specific first):
 * KA_*, KA_NONE, KA_PENDING, KA_CHORD_MISS; now_ms times a chord */
int km_action(cl_keymap *m, const int *ctx, int nctx, const cl_key *k, unsigned long now_ms);
/* the defaults as a keybindings.json (what /keybindings writes) */
void km_defaults_json(jw *out);

#endif
