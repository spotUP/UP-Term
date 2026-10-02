/* Completion and command lookup for the cooked line.
 *
 * The work runs in a worker process, never in the handler: a DOS call
 * made by the handler waits for its reply on the handler's own packet
 * port and would take a queued packet for it (see the handler's notes).
 * The worker works in the directory of the process that opened the window
 * (the Shell), with its command path, answers on a private port the
 * handler waits on, and ends.
 *
 * The KingCON rules (q->kingcon: device names, its order and suffixes,
 * wildcard words) follow the behaviour of KingCON by David Larsson, as
 * thoughts/shared/research/2026-10-02_kingcon-completion.md records it; no
 * code of KingCON is used. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
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

/* KingCON's kinds, the order its list shows them in (higher first) */
#define KIND_DEVICE 1
#define KIND_ASSIGN 2
#define KIND_VOLUME 3
#define KIND_DIR    1
#define KIND_FILE   2

/* One matching name: shortens the shared prefix, joins the menu list
 * (without repeats: a command in C: and in the path is one entry). Under
 * KingCON's rules an entry is stored as <kind><name><suffix> until
 * finish_names sorts the list and drops the kind byte. */
/* the stored entry k names `name` (KingCON entries: kind byte, name, suffix) */
static int entry_is(const struct complete_req *q, int k, const char *name)
{
    const char *e = q->names + k;
    int i;
    if (!q->kingcon)
        return same_name(e, name);
    for (e++, i = 0; name[i] && e[i] && lower((unsigned char)e[i]) == lower((unsigned char)name[i]); i++)
        ;
    return !name[i] && (!e[i] || !e[i + 1]); /* the name, then at most its suffix */
}

static void add_entry(struct complete_req *q, const char *name, int is_dir, int kind, char suffix)
{
    int n, k = 0;
    while (k < q->names_len) {
        if (entry_is(q, k, name))
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
    n = (int)strlen(name);
    if (!q->kingcon) {
        if (q->names_len + n + 1 < COMPLETE_NAMES) {
            memcpy(q->names + q->names_len, name, n + 1);
            q->names_len += n + 1;
        }
        return;
    }
    if (q->names_len + n + 3 < COMPLETE_NAMES) {
        char *e = q->names + q->names_len;
        e[0] = (char)('0' + kind);
        memcpy(e + 1, name, n);
        e[n + 1] = suffix;
        e[n + 2] = 0;
        q->names_len += n + 3;
    }
}

static void add_name(struct complete_req *q, const char *name, int is_dir)
{
    add_entry(q, name, is_dir, is_dir ? KIND_DIR : KIND_FILE, is_dir ? '/' : ' ');
}

/* KingCON's order: kind (higher first), then the name, case-insensitively;
 * then the kind byte goes. An insertion sort over offsets, as KingCON's
 * own: the lists are a directory's worth. */
static int name_cmp(const char *a, const char *b)
{
    if (*a != *b)
        return *b - *a;
    for (a++, b++; *a && lower((unsigned char)*a) == lower((unsigned char)*b); a++, b++)
        ;
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static void finish_names(struct complete_req *q)
{
    static const int max = 1024;
    char *sorted;
    int *at, n = 0, k = 0, i, j, o = 0;
    if (!q->kingcon || !q->names_len)
        return;
    at = (int *)AllocVec(max * sizeof(int), MEMF_ANY);
    sorted = (char *)AllocVec(COMPLETE_NAMES, MEMF_ANY);
    if (at && sorted) {
        while (k < q->names_len && n < max) {
            int x = k;
            for (i = n; i > 0 && name_cmp(q->names + at[i - 1], q->names + x) > 0; i--)
                at[i] = at[i - 1];
            at[i] = x;
            n++;
            k += (int)strlen(q->names + k) + 1;
        }
        for (i = 0; i < n; i++) {
            const char *e = q->names + at[i] + 1;
            j = (int)strlen(e) + 1;
            memcpy(sorted + o, e, j);
            o += j;
        }
        CopyMem(sorted, q->names, o);
        q->names_len = o;
    }
    if (at)
        FreeVec(at);
    if (sorted)
        FreeVec(sorted);
}

/* Devices, volumes and assigns starting with prefix, ":" appended. */
static void scan_devices(struct complete_req *q, const char *prefix)
{
    struct DosList *dl = LockDosList(LDF_DEVICES | LDF_VOLUMES | LDF_ASSIGNS | LDF_READ);
    char name[32];
    struct DosList *d = dl;
    while ((d = NextDosEntry(d, LDF_DEVICES | LDF_VOLUMES | LDF_ASSIGNS)) != 0) {
        const UBYTE *b = (const UBYTE *)BADDR(d->dol_Name);
        int n = b ? b[0] : 0, kind;
        if (n < 1 || n > 30)
            continue;
        memcpy(name, b + 1, n);
        name[n] = 0;
        if (!has_prefix(name, prefix))
            continue;
        kind = d->dol_Type == DLT_VOLUME ? KIND_VOLUME : d->dol_Type == DLT_DEVICE ? KIND_DEVICE
                                                                                  : KIND_ASSIGN;
        add_entry(q, name, 0, kind, ':');
    }
    UnLockDosList(LDF_DEVICES | LDF_VOLUMES | LDF_ASSIGNS | LDF_READ);
}

/* Names in directory `lock` starting with `prefix`; commands only: files
 * (a command is a file) and, for the path search, no directories. */
static void scan_dir(struct complete_req *q, BPTR lock, const char *prefix, int commands)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    char pat[COMPLETE_MAX * 2 + 2];
    int wild = 0;
    if (!fib)
        return;
    /* KingCON: a word with wildcards matches whole names (no implicit #?) */
    if (q->kingcon && prefix[0])
        wild = ParsePatternNoCase((STRPTR)prefix, (STRPTR)pat, sizeof(pat)) == 1;
    if (Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            int is_dir = fib->fib_DirEntryType > 0;
            int n = (int)strlen(name);
            int info = n > 5 && same_name(name + n - 5, ".info");
            if (commands && (is_dir || info))
                continue;
            if (q->kingcon && info)
                continue;
            if (wild ? MatchPatternNoCase((STRPTR)pat, (STRPTR)name) : has_prefix(name, prefix))
                add_name(q, name, is_dir);
        }
    }
    FreeDosObject(DOS_FIB, fib);
}

/* The command names of each directory on the command path, kept for
 * every window (the handler's code is shared, so is this): a Tab used to
 * scan C: file by file, ~3 s on the rig. A directory is scanned again
 * only when its date changes (FFS dates a directory when something in it
 * is made, deleted or renamed); a Tab costs one Examine per directory. */
typedef struct dir_cache {
    char name[256];               /* NameFromLock of the directory */
    struct DateStamp date;
    char *names;                  /* NUL-separated, AllocVec'd */
    long len;
    struct dir_cache *next;
} dir_cache;

static struct SignalSemaphore cache_sem;
static int cache_ready;
static dir_cache *caches;

static void cache_names(dir_cache *d, BPTR lock, struct FileInfoBlock *fib)
{
    long cap = 1024, len = 0;
    char *names = (char *)AllocVec(cap, MEMF_ANY);
    if (names && Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            long n = (long)strlen(name);
            if (fib->fib_DirEntryType > 0 || (n > 5 && same_name(name + n - 5, ".info")))
                continue;
            if (len + n + 1 > cap) {
                char *more = (char *)AllocVec(cap * 2, MEMF_ANY);
                if (!more)
                    break;
                CopyMem(names, more, len);
                FreeVec(names);
                names = more;
                cap *= 2;
            }
            CopyMem((APTR)name, names + len, n + 1);
            len += n + 1;
        }
    }
    if (d->names)
        FreeVec(d->names);
    d->names = names;
    d->len = names ? len : 0;
}

/* The commands in directory `lock` starting with prefix, from the cache. */
static void scan_commands(struct complete_req *q, BPTR lock, const char *prefix)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    char name[256];
    dir_cache *d;
    long k;
    if (!fib)
        return;
    if (!NameFromLock(lock, (STRPTR)name, sizeof(name)) || !Examine(lock, fib)) {
        FreeDosObject(DOS_FIB, fib);
        scan_dir(q, lock, prefix, 1);
        return;
    }
    Forbid();
    if (!cache_ready) {
        InitSemaphore(&cache_sem);
        cache_ready = 1;
    }
    Permit();
    ObtainSemaphore(&cache_sem);
    for (d = caches; d && !same_name(d->name, name); d = d->next)
        ;
    if (!d && (d = (dir_cache *)AllocVec(sizeof(dir_cache), MEMF_CLEAR)) != 0) {
        strcpy(d->name, name);
        d->next = caches;
        caches = d;
        d->date.ds_Days = -1; /* never scanned */
    }
    if (d) {
        if (CompareDates(&d->date, &fib->fib_Date) != 0 || !d->names) {
            d->date = fib->fib_Date;
            cache_names(d, lock, fib);
        }
        for (k = 0; k < d->len; k += (long)strlen(d->names + k) + 1)
            if (has_prefix(d->names + k, prefix))
                add_name(q, d->names + k, 0);
    }
    ReleaseSemaphore(&cache_sem);
    FreeDosObject(DOS_FIB, fib);
    if (!d)
        scan_dir(q, lock, prefix, 1);
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
        scan_commands(q, c, prefix);
        UnLock(c);
    }
    if (!q->opener || !q->opener->pr_CLI)
        return;
    cli = (struct CommandLineInterface *)BADDR(q->opener->pr_CLI);
    for (node = (BPTR *)BADDR(cli->cli_CommandDir); node; node = (BPTR *)BADDR(node[0]))
        if (node[1])
            scan_commands(q, node[1], prefix);
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

/* The shell's words (q->extra) that start with prefix; the shell's names
 * are case-sensitive, unlike AmigaDOS's. */
static void scan_extra(struct complete_req *q, const char *prefix)
{
    long k = 0;
    size_t l = strlen(prefix);
    while (k < q->extra_len) {
        const char *w = q->extra + k;
        if (!strncmp(w, prefix, l))
            add_name(q, w, 0);
        k += (long)strlen(w) + 1;
    }
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
    /* no requesters: the lookups run as the user types (a first word
     * "KEY:x" asked for volume KEY in a dialog on the rig) */
    me->pr_WindowPtr = (APTR)-1;
    q->matches = 0;
    q->add[0] = 0;
    q->common[0] = 0;
    q->is_dir = 0;
    q->names_len = 0;

    /* the opener's current directory (read without its cooperation, as
     * console-side completion must; it is the Shell's, still alive) */
    {
        BPTR cd = 0;
        Forbid(); /* the opener may have ended since the request was made */
        if (task_alive(&q->opener->pr_Task) && q->opener->pr_Task.tc_Node.ln_Type == NT_PROCESS)
            cd = q->opener->pr_CurrentDir;
        Permit();
        if (cd)
            dir = DupLock(cd);
    }
    if (dir)
        old = CurrentDir(dir);

    if (q->mode == CHECK_COMMAND) {
        q->matches = command_exists(q);
    } else if (q->mode == HISTORY_LOAD) {
        history_load(q);
    } else if (q->mode == HISTORY_APPEND) {
        history_append(q);
    } else if (q->mode == COMPLETE_VARS) {
        /* $NAME or ${NAME: the part after the $ or ${ */
        int brace = q->word[1] == '{';
        const char *pre = q->word + (brace ? 2 : 1);
        scan_extra(q, pre);
        if (q->matches) {
            strcpy(q->add, q->common + strlen(pre));
            if (q->matches == 1 && brace)
                strcat(q->add, "}");
        }
    } else {
        for (i = 0; q->word[i]; i++)
            if (q->word[i] == '/' || q->word[i] == ':')
                split = i;
        memcpy(dirpart, q->word, split + 1);
        dirpart[split + 1] = 0;
        strcpy(prefix, q->word + split + 1);
        lock = q->mode == COMPLETE_DEVICES ? 0 : Lock((STRPTR)dirpart, ACCESS_READ);
        if (lock) {
            scan_dir(q, lock, prefix, 0);
            UnLock(lock);
        }
        if (q->mode == COMPLETE_COMMANDS && split < 0) {
            scan_path(q, prefix);
            scan_residents(q, prefix);
            scan_extra(q, prefix);
        }
        /* KingCON: file names that find nothing are device names */
        if (q->mode == COMPLETE_DEVICES || (q->kingcon && q->mode == COMPLETE_FILES && !q->matches)) {
            q->matches = 0;
            q->names_len = 0;
            q->common[0] = 0;
            q->mode = COMPLETE_DEVICES; /* the handler titles its window by this */
            scan_devices(q, q->word);
            strcpy(prefix, q->word);
        }
        finish_names(q);
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
