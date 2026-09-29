/* Completion and command lookup for the cooked line, done by a worker
 * process (see complete.c): the handler makes no DOS calls itself. */
#ifndef COMPLETE_H
#define COMPLETE_H
#include <exec/ports.h>
#include <dos/dosextens.h>

#define COMPLETE_MAX 256
#define COMPLETE_NAMES 2048   /* the matching names, NUL-separated */

enum complete_mode {
    COMPLETE_FILES = 0,       /* the word is a path: names in its directory */
    COMPLETE_COMMANDS = 1,    /* the first word: C:, the Shell's path, residents, current dir */
    CHECK_COMMAND = 2         /* does the first word name a command? (for colouring) */
};

struct complete_req {
    struct Message msg;
    struct Process *opener;       /* whose current directory and path count */
    int mode;                     /* enum complete_mode */
    char word[COMPLETE_MAX];      /* in: the word before the cursor */
    char common[COMPLETE_MAX];    /* the longest name all matches start with */
    char add[COMPLETE_MAX];       /* out: what to type after the word */
    int matches;                  /* out: how many names matched (CHECK: 1 = found) */
    int is_dir;
    char names[COMPLETE_NAMES];   /* out: the matches, NUL-separated, for the menu */
    int names_len;
};

/* Start the scan; the request comes back on `reply`. 0 if no worker. */
int complete_start(struct complete_req *q, struct MsgPort *reply, struct Process *opener);

#endif
