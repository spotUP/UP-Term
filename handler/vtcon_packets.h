/* Private packets of the vtcon console (XCON:) for programs running in
 * it. Another console replies DOSFALSE / ERROR_ACTION_NOT_KNOWN, so a
 * program may send them anywhere. */
#ifndef VTCON_PACKETS_H
#define VTCON_PACKETS_H

/* The words a shell knows, for Tab completion and for colouring the
 * command word (vsh sends them before a prompt when they have changed).
 *   dp_Arg1  fh_Arg1 of a handle on the window
 *   dp_Arg2  VTCON_WORDS_COMMANDS or VTCON_WORDS_VARIABLES
 *   dp_Arg3  the names, each ending in NUL (APTR)
 *   dp_Arg4  their length in bytes (the console keeps VTCON_WORDS_MAX)
 * The console keeps a copy while the sending process lives. */
#define ACTION_VTCON_WORDS 0x7654
#define VTCON_WORDS_COMMANDS 1   /* builtins, functions, aliases */
#define VTCON_WORDS_VARIABLES 2  /* variable names */
#define VTCON_WORDS_MAX 2048

/* The Unix terminal: a program's termios and the window size, for the
 * line discipline the console runs (tty/ldisc.c). A console that does not
 * know them refuses (DOSFALSE, ERROR_ACTION_NOT_KNOWN): ixemul then keeps
 * its SetMode mapping.
 *   ACTION_VTCON_TCGETA  Arg1 fh_Arg1, Arg2 struct termios * to fill
 *   ACTION_VTCON_TCSETA  Arg1 fh_Arg1, Arg2 const struct termios *,
 *                        Arg3 TCSANOW / TCSADRAIN / TCSAFLUSH. The first one
 *                        puts the window in termios mode for the sender;
 *                        an ACTION_SCREEN_MODE, or the sender's end, ends it.
 *   ACTION_VTCON_GWINSZ  Arg1 fh_Arg1, Arg2 struct winsize * to fill
 *   ACTION_VTCON_SWINSZ  Arg1 fh_Arg1, Arg2 const struct winsize * (a pty
 *                        master's; a window's size is the window's)
 * struct termios and struct winsize are ixemul 48.2's (sys/termios.h,
 * sys/ttycom.h): vt_termios and vt_winsize in tty/ldisc.h. */
#define ACTION_VTCON_TCGETA 0x7655
#define ACTION_VTCON_TCSETA 0x7656
#define ACTION_VTCON_GWINSZ 0x7657
#define ACTION_VTCON_SWINSZ 0x7658

#endif
