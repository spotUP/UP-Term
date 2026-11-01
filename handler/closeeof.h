/* handler/closeeof.h: the end a cooked read sees after the close gadget.
 *
 * Invariant: once the close gadget hung a (non-AUTO, line-mode) window up,
 * every later read with no typed bytes left gets the end at once, as every
 * read of a hung-up tty does (tty/ldisc.c: ld->hup answers LD_RD_ZERO each
 * time). The end is not a one-shot event. A one-shot flag let the first
 * reader after the close (vsh's prompt read, just after mg left termios
 * mode) spend it; vsh then read once more, that read waited forever, and the
 * window and its process stayed until a second click (ledger
 * MG-CLOSE-WINDOW-STAYS, 2026-11-01).
 *
 * eof is the one-shot end (end of a typed line with Ctrl-\ or a window that
 * could not open): one read sees it, then input goes on. A raw window gets a
 * break or IECLASS_CLOSEWINDOW instead; an AUTO window opens again on the
 * next read, so neither keeps the end.
 *
 * Header-only: the handler and the host test (tests/test_closeeof.c) share it. */
#ifndef CLOSEEOF_H
#define CLOSEEOF_H

/* The close gadget ended this window for its readers: every read sees the end. */
static int closeeof_sticky(int closing, int raw, int auto_open)
{
    return closing && !raw && !auto_open;
}

/* A cooked read with no bytes buffered: 1 answers it with the end (and
 * spends a one-shot *eof), 0 leaves it waiting for input. */
static int closeeof_read(int *eof, int closing, int raw, int auto_open)
{
    if (*eof) {
        *eof = 0;
        return 1;
    }
    return closeeof_sticky(closing, raw, auto_open);
}

#endif
