/* The console line editor's functions under their readline names (V92): lineedit.c runs them by
 * number, vsh's `bind` and its .inputrc name them. A key is a control byte (0x01-0x1F, 0x7F) or
 * LE_META + a character (Meta/Alt with it, "\M-x" or "\ex" in readline's notation). */
#ifndef LE_FNS_H
#define LE_FNS_H

#define LE_META 0x80

/* number, readline name; in bash's `bind -l` order (alphabetical); 0 is no function (the key unbound) */
#define LE_FNS(X)                                                                                          \
    X(1, "backward-char") X(2, "backward-delete-char") X(3, "backward-kill-word") X(4, "backward-word")     \
    X(5, "beginning-of-line") X(6, "clear-screen") X(7, "delete-char") X(8, "end-of-line")                  \
    X(9, "forward-char") X(10, "forward-word") X(11, "kill-line") X(12, "kill-whole-line")                  \
    X(13, "kill-word") X(14, "next-history") X(15, "previous-history") X(16, "reverse-search-history")      \
    X(17, "undo") X(18, "unix-line-discard") X(19, "unix-word-rubout")
#define LE_NFNS 19

/* the keys the editor has without a binding (emacs mode): key, function */
#define LE_EMACS_KEYS(X)                                                                                   \
    X(0x01, 5) X(0x05, 8) X(0x0B, 11) X(0x0C, 6) X(0x12, 16) X(0x15, 18) X(0x17, 19) X(0x18, 12)           \
    X(0x1A, 17) X(0x1F, 17) X(LE_META + 'b', 4) X(LE_META + 'd', 13) X(LE_META + 'f', 10)

#define LE_BINDS 32 /* bindings a shell may set */

#endif
