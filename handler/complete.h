/* Tab filename completion, scanned by a worker process (see complete.c). */
#ifndef COMPLETE_H
#define COMPLETE_H
#include <exec/ports.h>
#include <dos/dosextens.h>

#define COMPLETE_MAX 256

struct complete_req {
    struct Message msg;
    struct Process *opener;       /* whose current directory counts */
    char word[COMPLETE_MAX];      /* in: the word before the cursor */
    char common[COMPLETE_MAX];    /* the longest name all matches start with */
    char add[COMPLETE_MAX];       /* out: what to type after the word */
    int matches;                  /* out: how many names matched */
    int is_dir;
};

/* Start the scan; the request comes back on `reply`. 0 if no worker. */
int complete_start(struct complete_req *q, struct MsgPort *reply, struct Process *opener);

#endif
