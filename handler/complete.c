/* Completion and command lookup for the cooked line.
 *
 * The work runs in a worker process, never in the handler: a DOS call
 * made by the handler waits for its reply on the handler's own packet
 * port and would take a queued packet for it (see the handler's notes).
 * The worker works in the directory of the process that opened the window
 * (the Shell), with its command path, answers on a private port the
 * handler waits on, and ends. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
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

static int same_name(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (lower((unsigned char)*a) != lower((unsigned char)*b))
            return 0;
    return *a == *b;
}

/* One matching name: shortens the shared prefix, joins the menu list
 * (without repeats: a command in C: and in the path is one entry). */
static void add_name(struct complete_req *q, const char *name, int is_dir)
{
    int n, k = 0;
    while (k < q->names_len) {
        if (same_name(q->names + k, name))
            return;
        k += (int)strlen(q->names + k) + 1;
    }
    if (!q->matches) {
        strncpy(q->common, name, COMPLETE_MAX - 1);
        q->common[COMPLETE_MAX - 1] = 0;
        q->is_dir = is_dir;
    } else {
        for (n = 0; q->common[n] && lower((unsigned char)q->common[n]) == lower((unsigned char)name[n]); n++)
            ;
        q->common[n] = 0;
    }
    q->matches++;
    n = (int)strlen(name) + 1;
    if (q->names_len + n < COMPLETE_NAMES) {
        memcpy(q->names + q->names_len, name, n);
        q->names_len += n;
    }
}

/* Names in directory `lock` starting with `prefix`; commands only: files
 * (a command is a file) and, for the path search, no directories. */
static void scan_dir(struct complete_req *q, BPTR lock, const char *prefix, int commands)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (!fib)
        return;
    if (Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            int is_dir = fib->fib_DirEntryType > 0;
            int n = (int)strlen(name);
            if (commands && (is_dir || (n > 5 && same_name(name + n - 5, ".info"))))
                continue;
            if (has_prefix(name, prefix))
                add_name(q, name, is_dir);
        }
    }
    FreeDosObject(DOS_FIB, fib);
}

/* The resident list (the Shell's internal commands live there too): a
 * private DOS list, walked read-only under Forbid as Resident does. */
static void scan_residents(struct complete_req *q, const char *prefix)
{
    struct DosInfo *di = (struct DosInfo *)BADDR(((struct RootNode *)DOSBase->dl_Root)->rn_Info);
    struct Segment *seg;
    char name[64];
    int count = 0, i;
    Forbid();
    /* The NDK names the list di_ResList (= di_McName), but KS 3.1 keeps it
     * in di_NetHand (probed on the rig, 2026-09-29: ResList 0, NetHand the
     * Alias/Ask/CD... chain). Private layout: every entry is checked. */
    seg = (struct Segment *)BADDR(di->di_ResList ? di->di_ResList : (BPTR)di->di_NetHand);
    for (; seg && count < 256; seg = (struct Segment *)BADDR(seg->seg_Next), count++) {
        int n = seg->seg_Name[0];
        if (n < 1 || n > 31)
            break; /* not a resident list after all */
        for (i = 1; i <= n; i++)
            if (seg->seg_Name[i] < 0x20 || seg->seg_Name[i] >= 0x7F)
                break;
        if (i <= n)
            break;
        if (seg->seg_UC < 0 && seg->seg_UC != CMD_INTERNAL && seg->seg_UC != CMD_SYSTEM)
            continue; /* disabled entries */
        memcpy(name, seg->seg_Name + 1, n);
        name[n] = 0;
        if (has_prefix(name, prefix))
            add_name(q, name, 0);
    }
    Permit();
}

/* The Shell's command path: CLI cli_CommandDir, a list of {next, lock}. */
static void scan_path(struct complete_req *q, const char *prefix)
{
    struct CommandLineInterface *cli;
    BPTR *node;
    BPTR c = Lock((STRPTR)"C:", ACCESS_READ);
    if (c) {
        scan_dir(q, c, prefix, 1);
        UnLock(c);
    }
    if (!q->opener || !q->opener->pr_CLI)
        return;
    cli = (struct CommandLineInterface *)BADDR(q->opener->pr_CLI);
    for (node = (BPTR *)BADDR(cli->cli_CommandDir); node; node = (BPTR *)BADDR(node[0]))
        if (node[1])
            scan_dir(q, node[1], prefix, 1);
}

/* CHECK_COMMAND: resident, a path to a file, or a file in the current
 * directory, C: or the path. */
static int command_exists(struct complete_req *q)
{
    struct CommandLineInterface *cli;
    BPTR *node, lock, old;
    const char *w = q->word;
    int found = 0, i;
    for (i = 0; w[i]; i++)
        if (w[i] == '/' || w[i] == ':') {
            lock = Lock((STRPTR)w, ACCESS_READ);
            if (lock)
                UnLock(lock);
            return lock != 0;
        }
    Forbid();
    found = FindSegment((STRPTR)w, 0, FALSE) != 0 || FindSegment((STRPTR)w, 0, TRUE) != 0;
    Permit();
    if (found)
        return 1;
    lock = Lock((STRPTR)w, ACCESS_READ); /* the current directory */
    if (lock) {
        UnLock(lock);
        return 1;
    }
    lock = Lock((STRPTR)"C:", ACCESS_READ);
    if (lock) {
        old = CurrentDir(lock);
        found = (lock = Lock((STRPTR)w, ACCESS_READ)) != 0;
        if (lock)
            UnLock(lock);
        UnLock(CurrentDir(old));
        if (found)
            return 1;
    }
    if (!q->opener || !q->opener->pr_CLI)
        return 0;
    cli = (struct CommandLineInterface *)BADDR(q->opener->pr_CLI);
    for (node = (BPTR *)BADDR(cli->cli_CommandDir); node && !found; node = (BPTR *)BADDR(node[0])) {
        if (!node[1])
            continue;
        old = CurrentDir(node[1]);
        lock = Lock((STRPTR)w, ACCESS_READ);
        CurrentDir(old);
        if (lock) {
            UnLock(lock);
            found = 1;
        }
    }
    return found;
}

/* HISTORY_LOAD: the file's last HISTORY_KEEP lines into q->data (and the
 * file trimmed to them once it has grown past twice that). */
static void history_load(struct complete_req *q)
{
    BPTR f = Open((STRPTR)HISTORY_FILE, MODE_OLDFILE);
    long n, i, lines = 0, from = 0;
    q->data_len = 0;
    if (!f)
        return;
    n = Read(f, q->data, q->data_max - 1);
    Close(f);
    if (n <= 0)
        return;
    for (i = n - 1; i >= 0; i--)
        if (q->data[i] == '\n' && ++lines > HISTORY_KEEP) {
            from = i + 1;
            break;
        }
    memmove(q->data, q->data + from, n - from);
    q->data_len = n - from;
    if (from && lines > HISTORY_KEEP) {
        /* the file had more: keep only what was loaded */
        f = Open((STRPTR)HISTORY_FILE, MODE_NEWFILE);
        if (f) {
            Write(f, q->data, q->data_len);
            Close(f);
        }
    }
}

static void history_append(struct complete_req *q)
{
    BPTR f = Open((STRPTR)HISTORY_FILE, MODE_READWRITE);
    if (!f)
        return;
    Seek(f, 0, OFFSET_END);
    Write(f, q->word, (LONG)strlen(q->word));
    Write(f, "\n", 1);
    Close(f);
}

/* The worker's body: runs as its own process. */
static void worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct complete_req *q;
    BPTR dir = 0, old = 0, lock;
    char dirpart[COMPLETE_MAX], prefix[COMPLETE_MAX];
    int split = -1, i;

    WaitPort(&me->pr_MsgPort); /* the request, before any DOS call */
    q = (struct complete_req *)GetMsg(&me->pr_MsgPort);
    q->matches = 0;
    q->add[0] = 0;
    q->common[0] = 0;
    q->is_dir = 0;
    q->names_len = 0;

    /* the opener's current directory (read without its cooperation, as
     * console-side completion must; it is the Shell's, still alive) */
    if (q->opener && q->opener->pr_Task.tc_Node.ln_Type == NT_PROCESS && q->opener->pr_CurrentDir)
        dir = DupLock(q->opener->pr_CurrentDir);
    if (dir)
        old = CurrentDir(dir);

    if (q->mode == CHECK_COMMAND) {
        q->matches = command_exists(q);
    } else if (q->mode == HISTORY_LOAD) {
        history_load(q);
    } else if (q->mode == HISTORY_APPEND) {
        history_append(q);
    } else {
        for (i = 0; q->word[i]; i++)
            if (q->word[i] == '/' || q->word[i] == ':')
                split = i;
        memcpy(dirpart, q->word, split + 1);
        dirpart[split + 1] = 0;
        strcpy(prefix, q->word + split + 1);
        lock = Lock((STRPTR)dirpart, ACCESS_READ);
        if (lock) {
            scan_dir(q, lock, prefix, 0);
            UnLock(lock);
        }
        if (q->mode == COMPLETE_COMMANDS && split < 0) {
            scan_path(q, prefix);
            scan_residents(q, prefix);
        }
        if (q->matches) {
            strcpy(q->add, q->common + strlen(prefix));
            if (q->matches == 1)
                strcat(q->add, q->is_dir ? "/" : " ");
        }
    }
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
