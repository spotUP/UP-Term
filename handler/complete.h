/* Completion and command lookup for the cooked line, done by a worker
 * process (see complete.c): the handler makes no DOS calls itself. */
#ifndef COMPLETE_H
#define COMPLETE_H
#include <exec/ports.h>
#include <dos/dosextens.h>
#include <intuition/screens.h>

#define COMPLETE_MAX 256
#define COMPLETE_NAMES 8192   /* the matching names, NUL-separated (a whole C: in KingCON's window) */

enum complete_mode {
    COMPLETE_FILES = 0,       /* the word is a path: names in its directory */
    COMPLETE_COMMANDS = 1,    /* the first word: C:, the Shell's path, residents, current dir */
    CHECK_COMMAND = 2,        /* does the first word name a command? (for colouring) */
    HISTORY_LOAD = 3,         /* data <- the saved history (newest last) */
    HISTORY_APPEND = 4,       /* word -> one more line of saved history */
    COMPLETE_VARS = 5,        /* $NAME / ${NAME: the shell's variable names (extra) */
    COMPLETE_DEVICES = 6,     /* devices, volumes and assigns (KingCON's Shift+Tab) */
    COMPLETE_ASL = 7          /* an ASL file requester on `screen`: the chosen path in add
                               * (KingCON's Tab on an empty word); matches 0: cancelled */
};

#define HISTORY_FILE "ENVARC:vtcon.history"
#define HISTORY_KEEP 100      /* lines kept when the file is trimmed */

struct complete_req {
    struct Message msg;
    struct Process *opener;       /* whose current directory and path count */
    int mode;                     /* enum complete_mode */
    int kingcon;                  /* in: KingCON's rules (research/2026-10-02_kingcon-completion.md):
                                   * names carry their suffix (dir "/", file " ", device ":"),
                                   * sorted files before dirs and volumes before assigns before
                                   * devices, then by name; a word with wildcards is a pattern;
                                   * no .info (unless show_info); files that find nothing fall
                                   * back to devices */
    int show_info;                /* in: KingCON lists .info files too */
    int no_cache;                 /* in: scan command directories afresh (KingCON's menu
                                   * "Enable cache" off, or kingcon-cache = off) */
    struct Screen *screen;        /* in: COMPLETE_ASL's screen */
    char word[COMPLETE_MAX];      /* in: the word before the cursor */
    char common[COMPLETE_MAX];    /* the longest name all matches start with */
    char add[COMPLETE_MAX];       /* out: what to type after the word */
    int matches;                  /* out: how many names matched (CHECK: 1 = found) */
    int is_dir;
    char names[COMPLETE_NAMES];   /* out: the matches, NUL-separated, for the menu */
    int names_len;
    char extra[2048];             /* in: the shell's words (COMMANDS: its commands, VARS: its
                                   * variables), NUL-separated; extra_len 0: none */
    long extra_len;
    char *data;                   /* HISTORY_LOAD: buffer to fill, data_max bytes */
    long data_max, data_len;
};

#include "brk.h" /* task_alive */

/* The command-directory cache every window shares (KingCON's "Reset
 * cache": every directory is read again at its next use; "Purge cache":
 * the cached names are freed). Exec calls only: safe in the handler. */
void complete_cache_reset(void);
void complete_cache_purge(void);

/* Start the scan; the request comes back on `reply`. 0 if no worker. */
int complete_start(struct complete_req *q, struct MsgPort *reply, struct Process *opener);

#endif
