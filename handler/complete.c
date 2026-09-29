/* Tab filename completion for the cooked line.
 *
 * The directory scan runs in a worker process, never in the handler: a
 * DOS call made by the handler waits for its reply on the handler's own
 * packet port and would take a queued packet for it (see the handler's
 * notes). The worker scans in the directory of the process that opened
 * the window (the Shell), sends the answer back to a private port the
 * handler waits on, and ends. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "complete.h"

static int lower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
        return c + 32; /* Latin-1 capitals */
    return c;
}

static int has_prefix(const char *s, const char *p)
{
    for (; *p; s++, p++)
        if (lower((unsigned char)*s) != lower((unsigned char)*p))
            return 0;
    return 1;
}

/* The worker's body: runs as its own process. */
static void worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct complete_req *q;
    BPTR dir = 0, old = 0, lock;
    struct FileInfoBlock *fib;
    char dirpart[COMPLETE_MAX], prefix[COMPLETE_MAX];
    int split = -1, i, n;

    WaitPort(&me->pr_MsgPort); /* the request, before any DOS call */
    q = (struct complete_req *)GetMsg(&me->pr_MsgPort);
    q->matches = 0;
    q->add[0] = 0;
    q->is_dir = 0;
    for (i = 0; q->word[i]; i++)
        if (q->word[i] == '/' || q->word[i] == ':')
            split = i;
    memcpy(dirpart, q->word, split + 1);
    dirpart[split + 1] = 0;
    strcpy(prefix, q->word + split + 1);

    /* the opener's current directory (read without its cooperation, as
     * console-side completion must; it is the Shell's, still alive) */
    if (q->opener && q->opener->pr_Task.tc_Node.ln_Type == NT_PROCESS && q->opener->pr_CurrentDir)
        dir = DupLock(q->opener->pr_CurrentDir);
    if (dir)
        old = CurrentDir(dir);
    lock = Lock((STRPTR)dirpart, ACCESS_READ);
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (lock && fib && Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            if (!has_prefix(name, prefix))
                continue;
            if (!q->matches) {
                strcpy(q->common, name);
                q->is_dir = fib->fib_DirEntryType > 0;
            } else {
                /* keep the part every match shares */
                for (n = 0; q->common[n] && lower((unsigned char)q->common[n]) ==
                                            lower((unsigned char)name[n]); n++)
                    ;
                q->common[n] = 0;
            }
            q->matches++;
        }
    }
    if (q->matches) {
        n = (int)strlen(prefix);
        strcpy(q->add, q->common + n);
        if (q->matches == 1)
            strcat(q->add, q->is_dir ? "/" : " ");
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (lock)
        UnLock(lock);
    if (dir) {
        CurrentDir(old);
        UnLock(dir);
    }
    Forbid(); /* the reply and our end, before the handler can free anything */
    ReplyMsg(&q->msg);
}

int complete_start(struct complete_req *q, struct MsgPort *reply, struct Process *opener)
{
    struct Process *w;
    q->msg.mn_ReplyPort = reply;
    q->msg.mn_Length = sizeof(*q);
    q->opener = opener;
    w = CreateNewProcTags(NP_Entry, (ULONG)worker, NP_Name, (ULONG)"vtcon completion",
                          NP_StackSize, 6000, NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE,
                          NP_CloseOutput, FALSE, NP_ConsoleTask, 0, TAG_DONE);
    if (!w)
        return 0;
    PutMsg(&w->pr_MsgPort, &q->msg);
    return 1;
}
