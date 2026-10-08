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

/* Job control for a shell (vsh S8): reads from a stopped or background
 * job's process wait, and the console serves the next reader -- as a Unix
 * tty sleeps a reader that is not in the foreground. A read the job sent
 * before it was stopped stays queued, not answered with the shell's line.
 *   ACTION_VTCON_HOLD  Arg1 fh_Arg1, Arg2 the process (struct Task *),
 *                      Arg3 1 hold / 0 release. Holds of processes that
 *                      are gone are dropped. */
#define ACTION_VTCON_HOLD 0x7659

/* FIONREAD: dp_Res1 the bytes a Read would get now (a complete line in
 * canonical mode, everything queued otherwise; a PTY: master, the slave's
 * output). ixemul asked WaitForChar, which says 1 at most: libevent then
 * read one byte per turn and a terminal's answer came apart. dp_Arg1 is
 * the handle's fh_Arg1. */
#define ACTION_VTCON_NREAD 0x765A

/* A slash command (handler/slash.c; ledger C1) run in the window, as typed
 * at its prompt: C:UPTerm sends them for scripts.
 *   dp_Arg1  fh_Arg1 of a handle on the window
 *   dp_Arg2  the command line, NUL-terminated, starting with "/" (APTR)
 *   dp_Arg3  a buffer for the answer (APTR), dp_Arg4 its size
 *   dp_Res1  0 not a command, 1 done, 2 refused (the answer says why) */
#define ACTION_VTCON_COMMAND 0x765B

/* A caught signal interrupts a read (Unix: the read returns EINTR and the
 * handler runs; less 321's SIGWINCH handler longjmps out of its read, W47).
 * ixemul sends this while its ACTION_READ waits here: the read, if still
 * queued, is answered now with dp_Res1 -1, dp_Res2 ERROR_BREAK and no
 * bytes taken.
 *   dp_Arg1  fh_Arg1, dp_Arg2 the waiting read (struct DosPacket *)
 *   dp_Res1  DOSTRUE the read was given back, DOSFALSE it was not queued
 *            (already answered: its reply carries the bytes) */
#define ACTION_VTCON_INTR 0x765C

/* The window's line history, for the shell's `history` builtin (V88). The console owns the list (the line
 * editor's, loaded from ENVARC:vtcon.history when the window opens, each entered line appended to that file);
 * the shell holds no copy. These operations change the list in memory only, never the file. A console that
 * does not know the packet answers dp_Res1 DOSFALSE, dp_Res2 ERROR_ACTION_NOT_KNOWN and the shell keeps a list
 * of its own from then on.
 *   dp_Arg1  fh_Arg1
 *   dp_Arg2  VTCON_HIST_COUNT   dp_Res1 the number of lines
 *            VTCON_HIST_GET     dp_Arg3 index (0 = oldest), dp_Arg4 buffer, dp_Arg5 its size: dp_Res1 the
 *                               line's length (it is NUL-terminated), -1 when there is no such line
 *            VTCON_HIST_ADD     dp_Arg4 the NUL-terminated line: dp_Res1 DOSTRUE
 *            VTCON_HIST_DEL     dp_Arg3 index: dp_Res1 DOSTRUE, DOSFALSE when there is none
 *            VTCON_HIST_CLEAR   dp_Res1 DOSTRUE
 *            VTCON_HIST_CONFIG  see below */
#define ACTION_VTCON_HISTORY 0x765D
#define VTCON_HIST_COUNT 0
#define VTCON_HIST_GET   1
#define VTCON_HIST_ADD   2
#define VTCON_HIST_DEL   3
#define VTCON_HIST_CLEAR 4
/*            VTCON_HIST_CONFIG  dp_Arg4 a NUL-terminated "HISTSIZE\nHISTFILESIZE\nHISTCONTROL" (the numbers may
 *                               be empty = the default 100 lines; the control words are the shell's, of which
 *                               ignorespace, ignoreboth and erasedups matter here). The list keeps at most
 *                               HISTSIZE lines (up to 1000), is read from the file again when it grew, and
 *                               the file keeps HISTFILESIZE lines when it is trimmed at the next window. */
#define VTCON_HIST_CONFIG 5

/* The environment the window tells its programs (W46: /term, /colors and
 * the profile's term and colors): vsh asks at its start and before a
 * prompt and exports what changed, so the next command it starts sees it.
 *   dp_Arg1  fh_Arg1
 *   dp_Arg2  a buffer (APTR), dp_Arg3 its size (VTCON_ENV_MAX is enough)
 *   dp_Res1  DOSTRUE; the buffer holds NAME=VALUE entries, each ending in
 *            NUL, then one more NUL; an empty VALUE means "unset NAME". An
 *            empty list: the window tells nothing. */
#define ACTION_VTCON_ENV 0x765E
#define VTCON_ENV_MAX 128

/* ACTION_VTCON_TCGETA answers dp_Res2 1 when the console is in termios
 * mode (a program set it), 0 when it describes the Amiga mode in termios
 * terms: a shell that suspends a job keeps the job's settings only then
 * (vsh S8). PTY: is always in termios mode. */

/* In termios mode the line discipline's signal keys go to the program as
 * break signals, which ixemul (patched, on a vtcon console) turns into
 * Unix signals for the foreground process group:
 *   VINTR ^C  -> SIGBREAKF_CTRL_C -> SIGINT
 *   VQUIT ^\  -> SIGBREAKF_CTRL_E -> SIGQUIT
 *   VSUSP ^Z  -> SIGBREAKF_CTRL_F -> SIGTSTP */

#endif
