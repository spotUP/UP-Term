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

#endif
