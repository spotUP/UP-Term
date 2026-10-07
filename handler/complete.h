/* Completion and command lookup for the cooked line, done by a worker
 * process (see complete.c): the handler makes no DOS calls itself. */
#ifndef COMPLETE_H
#define COMPLETE_H
#include <exec/ports.h>
#include <dos/dosextens.h>
#include <intuition/screens.h>

#define COMPLETE_MAX 256
#define COMPLETE_NAMES 8192   /* the matching names, NUL-separated (a whole C: in KingCON's window) */
#define COMPLETE_EXTRA 2048   /* the shell's words handed to a completion */

enum complete_mode {
    COMPLETE_FILES = 0,       /* the word is a path: names in its directory */
    COMPLETE_COMMANDS = 1,    /* the first word: C:, the Shell's path, residents, current dir */
    CHECK_COMMAND = 2,        /* does the first word name a command? (for colouring) */
    HISTORY_LOAD = 3,         /* data <- the saved history (newest last) */
    HISTORY_APPEND = 4,       /* word -> one more line of saved history */
    COMPLETE_VARS = 5,        /* $NAME / ${NAME: the shell's variable names (extra) */
    COMPLETE_DEVICES = 6,     /* devices, volumes and assigns (KingCON's Shift+Tab) */
    COMPLETE_ASL = 7,         /* an ASL file requester on `screen`: the chosen path in add
                               * (KingCON's Tab on an empty word); matches 0: cancelled */
    COMPLETE_FONT = 8,        /* an ASL font requester (fixed width) on `screen`: the font's
                               * name in add, its size in font_size; matches 0: cancelled */
    COMPLETE_THEME = 9,       /* an ASL file requester on the themes drawer
                               * (prefs_dos_theme_drawer), on `screen`, or with word set
                               * (/theme NAME, a name or a path) that theme: the file read
                               * into data, its path in add; matches 0: none */
    CONFIG_SAVE = 10,         /* data (data_len bytes) written as the profile file, ENV: and
                               * ENVARC: (prefs_dos_save); matches 1 when both are in place,
                               * font_size the result code */
    COMPLETE_THEMES = 11      /* /theme's list: the themes in the themes drawer, sorted, in
                               * names (matches of them); the drawer in add ("" none) */
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
    int cold;                     /* in: read a command directory the cache cannot answer
                                   * for (the refine after a warm-up); 0: never wait */
    int partial;                  /* out: COMMANDS answered without every directory's
                                   * current names (a warm-up refines it; add then
                                   * carries no suffix) */
    unsigned long warm_gen;       /* out: complete_warm_gen() when the lookup began */
    struct Screen *screen;        /* in: COMPLETE_ASL's / _FONT's / _THEME's screen */
    int font_size;                /* COMPLETE_FONT: in the current size, out the chosen one */
    char word[COMPLETE_MAX];      /* in: the word before the cursor */
    char common[COMPLETE_MAX];    /* the longest name all matches start with */
    char add[COMPLETE_MAX];       /* out: what to type after the word */
    int matches;                  /* out: how many names matched (CHECK: 1 = found) */
    int is_dir;
    char *names;                  /* out: the matches, NUL-separated, for the menu
                                   * (COMPLETE_NAMES; 0 in a request made without lists) */
    int names_len;
    char *extra;                  /* in: the shell's words (COMMANDS: its commands, VARS: its
                                   * variables), NUL-separated; extra_len 0: none
                                   * (COMPLETE_EXTRA; 0 without lists). COMPLETE_THEME(S):
                                   * the theme file the window has ("" none: its drawer is
                                   * looked in first), NUL, the handler's own file (its
                                   * DeviceNode's dn_Handler, "" not known: the themes
                                   * drawer beside it when the kit's is missing), NUL --
                                   * see theme_dir */
    long extra_len;
    char *data;                   /* HISTORY_LOAD: 0 in; out the file, allocated by the worker
                                   * at its size (data_max at most), the caller FreeVecs it.
                                   * COMPLETE_THEME, CONFIG_SAVE: the caller's, data_max bytes */
    long data_max, data_len;
    int hlcat;                /* CONFIG_SAVE: the highlight-cat switch prefs_dos_save writes beside the file */
};

/* A request, cleared. lists: with room for the names and the shell's
 * words (COMPLETE_NAMES + COMPLETE_EXTRA, in the same block), which only
 * completions use; a command check or the history is 800 bytes without
 * them, 11 KB with (research/2026-10-04_window-memory.md). FreeVec frees
 * it. 0 when there is no memory. */
struct complete_req *complete_req_new(int lists);

#include "brk.h" /* task_alive */

/* The command-directory cache every window shares (KingCON's "Reset
 * cache": every directory is read again at its next use; "Purge cache":
 * the cached names are freed). Exec calls only: safe in the handler. */
void complete_cache_reset(void);
void complete_cache_purge(void);

/* W22: the warm-up -- a low-priority process reads C: (every directory of
 * a multi-assign) and opener's path into the command cache, only what
 * changed since the cache file was written, then writes the file. One at a
 * time (0 when it could not start). */
int complete_warm(struct Process *opener);

/* Warm-ups finished so far. */
unsigned long complete_warm_gen(void);

/* t gets sig when a warm-up ends (one is started for opener when none
 * runs). 1 instead when one has ended since gen (ask again now). */
int complete_warm_wait(struct Task *t, ULONG sig, unsigned long gen, struct Process *opener);

/* t waits no more (its window closes). */
void complete_warm_forget(struct Task *t);

/* Start the scan; the request comes back on `reply`. 0 if no worker. */
int complete_start(struct complete_req *q, struct MsgPort *reply, struct Process *opener);

#endif
