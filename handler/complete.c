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
#include <exec/interrupts.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <libraries/asl.h>
#include <string.h>
#include "complete.h"
#include "complete_core.h"

#if CC_FIBF_EXECUTE != FIBF_EXECUTE || CC_FIBF_SCRIPT != FIBF_SCRIPT
#error "the protection bits of complete_core.h disagree with dos/dos.h"
#endif
#if CC_CMD_INTERNAL != CMD_INTERNAL
#error "CC_CMD_INTERNAL of complete_core.h disagrees with dos/dosextens.h"
#endif
#include "../prefs/prefs_dos.h"

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
    if (!q->names)
        return; /* a request made without lists: no completion is asked of it */
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

/* Names in directory `lock` starting with `prefix`; commands: only what
 * cc_is_command takes (files with e or s, no directories), no .info. */
/* W21: ticks (1/50 s) since the epoch of DateStamp, for the ghost read's budget */
static long now_ticks(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return ds.ds_Minute * 3000L + ds.ds_Tick;
}

/* W21: the volume or assign a path starts with is in the DOS list (a name
 * that is not would be asked for in a requester, or waited for); a path
 * without a volume is the current directory's. */
static int ghost_volume_known(const char *dirpart)
{
    char name[COMPLETE_MAX];
    int n = 0;
    struct DosList *dl;
    while (dirpart[n] && dirpart[n] != ':' && dirpart[n] != '/')
        n++;
    if (dirpart[n] != ':')
        return 1;
    if (n == 0 || n >= (int)sizeof(name))
        return 0;
    memcpy(name, dirpart, n);
    name[n] = 0;
    dl = LockDosList(LDF_ALL | LDF_READ);
    dl = FindDosEntry(dl, (STRPTR)name, LDF_ALL);
    UnLockDosList(LDF_ALL | LDF_READ);
    return dl != 0;
}

static void scan_dir(struct complete_req *q, BPTR lock, const char *prefix, int commands)
{
    long t0 = q->ghost ? now_ticks() : 0;
    int seen = 0;
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
            if (q->ghost && (++seen > GHOST_ENTRIES || now_ticks() - t0 > GHOST_TICKS))
                break; /* a big or slow directory: what was read is the ghost's */
            int info = n > 5 && same_name(name + n - 5, ".info");
            if (commands && (info || !cc_is_command(fib->fib_DirEntryType, fib->fib_Protection)))
                continue;
            if (q->kingcon && info && !q->show_info)
                continue;
            if (wild ? MatchPatternNoCase((STRPTR)pat, (STRPTR)name) : has_prefix(name, prefix))
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
        if (!cc_resident_listed(seg->seg_UC))
            continue; /* the system's segments, disabled entries */
        memcpy(name, seg->seg_Name + 1, n);
        name[n] = 0;
        if (has_prefix(name, prefix))
            add_name(q, name, 0);
    }
    Permit();
}

/* The command directories (cc_walk_command_dirs): C:'s through
 * GetDeviceProc, every directory of a multi-assign, then the Shell's
 * path, CLI cli_CommandDir, a list of {next, lock}. */
typedef struct cmd_walk {
    struct complete_req *q;
    const char *prefix;          /* scan: the names starting with it */
    int find;                    /* or find: is q->word in one */
    struct DevProc *dp;          /* where the C: walk stands */
    int c_done;
    BPTR *path;                  /* the path's next node */
    struct FileInfoBlock *fib;   /* the command cache's Examines (W22), 0: none */
} cmd_walk;

static long cw_c_next(void *u)
{
    cmd_walk *w = (cmd_walk *)u;
    BPTR l;
    while (!w->c_done) {
        w->dp = GetDeviceProc((STRPTR)"C:", w->dp);
        if (!w->dp) {
            w->c_done = 1;
            break;
        }
        if (!(w->dp->dvp_Flags & DVPF_ASSIGN))
            w->c_done = 1; /* not an assign: one directory, no next */
        l = w->dp->dvp_Lock ? DupLock(w->dp->dvp_Lock) : Lock((STRPTR)"C:", ACCESS_READ);
        if (l)
            return (long)l;
    }
    return 0;
}

static void cw_c_end(void *u)
{
    cmd_walk *w = (cmd_walk *)u;
    if (w->dp)
        FreeDeviceProc(w->dp);
    w->dp = 0;
}

static long cw_p_next(void *u)
{
    cmd_walk *w = (cmd_walk *)u;
    while (w->path) {
        BPTR *node = w->path;
        w->path = (BPTR *)BADDR(node[0]);
        if (node[1])
            return (long)node[1];
    }
    return 0;
}

static int cw_same(long a, long b)
{
    return SameLock((BPTR)a, (BPTR)b) == LOCK_SAME;
}

static void cw_drop(long lock)
{
    UnLock((BPTR)lock);
}

/* the walk's own visit: KingCON's cache off (scan each directory), or
 * CHECK_COMMAND's find (the cached walk visits through complete_core) */
static int cw_visit(void *u, long lock)
{
    cmd_walk *w = (cmd_walk *)u;
    BPTR old, l;
    if (!w->find) {
        scan_dir(w->q, (BPTR)lock, w->prefix, 1);
        return 0;
    }
    old = CurrentDir((BPTR)lock);
    l = Lock((STRPTR)w->q->word, ACCESS_READ);
    CurrentDir(old);
    if (l)
        UnLock(l);
    return l != 0;
}

static const cc_dirs_os cmd_dirs_os = { cw_c_next, cw_c_end, cw_p_next, cw_same, cw_drop, cw_visit };

/* C: and opener's path (0: C: alone). Under Forbid when opener may end. */
static void walk_init(cmd_walk *w, struct complete_req *q, struct Process *opener,
                      const char *prefix, int find)
{
    w->q = q;
    w->prefix = prefix;
    w->find = find;
    w->dp = 0;
    w->c_done = 0;
    w->path = 0;
    w->fib = 0;
    if (opener && opener->pr_CLI)
        w->path = (BPTR *)BADDR(((struct CommandLineInterface *)BADDR(opener->pr_CLI))->cli_CommandDir);
}

static int walk_command_dirs(struct complete_req *q, const char *prefix, int find)
{
    cmd_walk w;
    walk_init(&w, q, q->opener, prefix, find);
    return cc_walk_command_dirs(&cmd_dirs_os, &w);
}

/* ---- W22: the command-name cache (complete_core.h), its AmigaDOS side ----
 *
 * One cache for every window (the handler's code and data are shared): the
 * Tab's worker reads it (never a directory, cold 0), the warm-up process
 * fills it at low priority, when a window opens and when a Tab found a
 * directory changed or not cached.
 *
 * The file is ENVARC:up-term/commands.cache: it has to outlive a reboot,
 * and ENV: is RAM: on 3.1 and 3.2 alike. On 3.2 RAM:ENV is a link to
 * ENVARC: (MakeLink in its Startup-sequence) that costs no RAM; 3.1's
 * Startup-sequence copies ENVARC: into RAM:ENV at boot, so there the file
 * costs its size in RAM once (about 3 KB for the owner's 3.1 disk's C: and
 * path, CC_CACHE_FILE_MAX at most), as the history file next to it does.
 *
 * RAM: buffers come from fast RAM when the machine has any (a MemHeader
 * with MEMF_FAST in exec's list: MEMF_FAST, never chip -- without fast RAM
 * to spare nothing is cached rather than chip taken); a machine without
 * fast RAM uses MEMF_ANY (chip) during a request only, keep 0, and reads
 * the file at the next one (the owner, 2026-10-04: the on-disk copy is
 * preferred on chip-only machines). No allocation leaves less than
 * CACHE_FLOOR free, and exec's low-memory handler (V39+) frees every
 * cached name when an allocation anywhere fails. */

#define CACHE_DIR   "ENVARC:up-term"
#define CACHE_FILE  "ENVARC:up-term/commands.cache"
#define CACHE_FLOOR 131072L       /* free RAM the cache never takes: a window,
                                   * its font and a Shell need about that */

static struct SignalSemaphore cache_sem;
static int cache_ready, cache_lowmem_added;
static cc_cache cache;
static ULONG cache_memf;          /* MEMF_FAST, or MEMF_ANY on a chip-only machine */
static struct Interrupt cache_lowmem_irq;

static void co_lock(void)
{
    ObtainSemaphore(&cache_sem);
}

static void co_unlock(void)
{
    ReleaseSemaphore(&cache_sem);
}

static void *co_alloc(long size)
{
    if ((long)AvailMem(cache_memf) < CACHE_FLOOR + size)
        return 0;
    return AllocMem(size, cache_memf);
}

static void co_free(void *p, long size)
{
    FreeMem(p, size);
}

static int co_stat(void *u, long lock, char *name, int max, cc_date *d)
{
    struct FileInfoBlock *fib = ((cmd_walk *)u)->fib;
    if (!fib || !NameFromLock((BPTR)lock, (STRPTR)name, max) || !Examine((BPTR)lock, fib))
        return 0;
    d->days = fib->fib_Date.ds_Days;
    d->minute = fib->fib_Date.ds_Minute;
    d->tick = fib->fib_Date.ds_Tick;
    return 1;
}

/* a directory's commands: files with e or s (cc_is_command), no .info */
static int co_scan(void *u, long lock, cc_name_fn add, void *x)
{
    struct FileInfoBlock *fib = ((cmd_walk *)u)->fib;
    if (!fib || !Examine((BPTR)lock, fib))
        return 0;
    while (ExNext((BPTR)lock, fib)) {
        const char *name = (const char *)fib->fib_FileName;
        int n = (int)strlen(name);
        if ((n > 5 && same_name(name + n - 5, ".info")) ||
            !cc_is_command(fib->fib_DirEntryType, fib->fib_Protection))
            continue;
        if (!add(x, name))
            return 0;
    }
    return IoErr() == ERROR_NO_MORE_ENTRIES; /* else cut short: not the directory's list */
}

static long co_load(char *buf, long max)
{
    BPTR f = Open((STRPTR)CACHE_FILE, MODE_OLDFILE);
    long n;
    if (!f)
        return -1;
    n = Read(f, buf, max);
    Close(f);
    return n;
}

static int co_save(const char *buf, long len)
{
    BPTR f, l = Lock((STRPTR)CACHE_DIR, ACCESS_READ);
    long n;
    if (!l)
        l = CreateDir((STRPTR)CACHE_DIR);
    if (!l)
        return 0;
    UnLock(l);
    f = Open((STRPTR)CACHE_FILE, MODE_NEWFILE);
    if (!f)
        return 0;
    n = Write(f, (APTR)buf, len);
    Close(f);
    if (n != len) {
        DeleteFile((STRPTR)CACHE_FILE); /* cut short: gone rather than half */
        return 0;
    }
    return 1;
}

static const cc_cache_os cache_os = { co_lock, co_unlock, co_alloc, co_free, co_stat, co_scan, co_load, co_save };

/* exec's low-memory handler (is_Data the cache): runs in the allocating
 * task under Forbid, so it only tries the lock; a lookup holding it keeps
 * the names (in_use) */
static ULONG cache_lowmem(__reg("a1") APTR data)
{
    long freed;
    if (!AttemptSemaphore(&cache_sem))
        return MEM_DID_NOTHING;
    freed = cc_cache_release((cc_cache *)data, &cache_os);
    ReleaseSemaphore(&cache_sem);
    return freed ? MEM_TRY_AGAIN : MEM_DID_NOTHING;
}

/* the semaphore, the RAM type and the low-memory handler, at first use */
static void cache_init(void)
{
    int add = 0;
    Forbid();
    if (!cache_ready) {
        struct MemHeader *mh;
        InitSemaphore(&cache_sem);
        cache_memf = MEMF_ANY;
        for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
             mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
            if (mh->mh_Attributes & MEMF_FAST)
                cache_memf = MEMF_FAST;
        cc_cache_init(&cache, CC_CACHE_CAP, cache_memf == MEMF_FAST ? CC_CACHE_CAP : 0);
        cache_ready = 1;
        if (SysBase->LibNode.lib_Version >= 39 && !cache_lowmem_added) {
            cache_lowmem_irq.is_Node.ln_Type = NT_INTERRUPT;
            cache_lowmem_irq.is_Node.ln_Pri = -10; /* after the system's own */
            cache_lowmem_irq.is_Node.ln_Name = (char *)"UP-Term command cache";
            cache_lowmem_irq.is_Data = (APTR)&cache;
            cache_lowmem_irq.is_Code = (void (*)())cache_lowmem;
            cache_lowmem_added = add = 1;
        }
    }
    Permit();
    /* never removed: the handler's code stays loaded (dn_SegList) for the
     * boot, as the cache does */
    if (add)
        AddMemHandler(&cache_lowmem_irq);
}

void complete_cache_reset(void)
{
    cache_init();
    cc_cache_reset(&cache, &cache_os);
}

void complete_cache_purge(void)
{
    cache_init();
    ObtainSemaphore(&cache_sem);
    cc_cache_release(&cache, &cache_os);
    ReleaseSemaphore(&cache_sem);
}

/* warm-ups that finished, and who waits for the next one */
static unsigned long warm_gen;
static int warm_running;
#define WARM_WAITERS 16
static struct {
    struct Task *task;
    ULONG sig;
} warm_waiters[WARM_WAITERS];

struct warm_msg {
    struct Message msg;
    struct Process *opener;
};

/* a Tab's names that start with the word (the cached walk's emit) */
static int cw_emit(void *x, const char *name)
{
    cmd_walk *w = (cmd_walk *)x;
    if (has_prefix(name, w->prefix))
        add_name(w->q, name, 0);
    return 1;
}

/* COMPLETE_COMMANDS' directories: from the cache (q->partial when one was
 * not current), or each read afresh (KingCON's cache off) */
static void command_names(struct complete_req *q, const char *prefix)
{
    cmd_walk w;
    if (q->no_cache) {
        walk_command_dirs(q, prefix, 0);
        return;
    }
    cache_init();
    Forbid();
    q->warm_gen = warm_gen; /* a warm-up ending after this refines the answer */
    Permit();
    walk_init(&w, q, q->opener, prefix, 0);
    w.fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    q->partial = cc_command_names(&cache, &cache_os, &cmd_dirs_os, &w, q->cold, cw_emit, &w);
    cc_cache_end(&cache, &cache_os);
    if (w.fib)
        FreeDosObject(DOS_FIB, w.fib);
}

/* The warm-up's body: its own process, at low priority. */
static void warm_worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct warm_msg *m;
    struct Process *opener;
    cmd_walk w;
    int i;
    WaitPort(&me->pr_MsgPort);
    m = (struct warm_msg *)GetMsg(&me->pr_MsgPort);
    opener = m->opener;
    FreeVec(m);
    me->pr_WindowPtr = (APTR)-1; /* no requesters (a path naming a volume not in a drive) */
    Forbid();
    if (!opener || !task_alive(&opener->pr_Task) || opener->pr_Task.tc_Node.ln_Type != NT_PROCESS)
        opener = 0; /* gone: C: alone */
    walk_init(&w, 0, opener, 0, 0);
    Permit();
    w.fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (w.fib) {
        cc_cache_warm(&cache, &cache_os, &cmd_dirs_os, &w);
        FreeDosObject(DOS_FIB, w.fib);
    }
    Forbid(); /* the waiters woken and our end, before anything can go */
    warm_running = 0;
    warm_gen++;
    for (i = 0; i < WARM_WAITERS; i++)
        if (warm_waiters[i].task) {
            Signal(warm_waiters[i].task, warm_waiters[i].sig);
            warm_waiters[i].task = 0;
        }
}

int complete_warm(struct Process *opener)
{
    struct warm_msg *m;
    struct Process *p;
    cache_init();
    Forbid();
    if (warm_running) {
        Permit();
        return 1; /* one at a time: it wakes every waiter */
    }
    warm_running = 1;
    Permit();
    m = (struct warm_msg *)AllocVec(sizeof(*m), MEMF_PUBLIC | MEMF_CLEAR);
    if (m) {
        m->msg.mn_Length = sizeof(*m);
        m->opener = opener;
        p = CreateNewProcTags(NP_Entry, (ULONG)warm_worker, NP_Name, (ULONG)"UP-Term command cache",
                              NP_Priority, -1, NP_StackSize, 12000, NP_Input, 0, NP_Output, 0,
                              NP_CloseInput, FALSE, NP_CloseOutput, FALSE, NP_ConsoleTask, 0, TAG_DONE);
        if (p) {
            PutMsg(&p->pr_MsgPort, &m->msg);
            return 1;
        }
        FreeVec(m);
    }
    Forbid();
    warm_running = 0;
    Permit();
    return 0;
}

unsigned long complete_warm_gen(void)
{
    return warm_gen;
}

int complete_warm_wait(struct Task *t, ULONG sig, unsigned long gen, struct Process *opener)
{
    int i, done;
    Forbid();
    done = warm_gen != gen;
    if (!done) {
        for (i = 0; i < WARM_WAITERS && warm_waiters[i].task && warm_waiters[i].task != t; i++)
            ;
        if (i < WARM_WAITERS) {
            warm_waiters[i].task = t;
            warm_waiters[i].sig = sig;
        } else {
            done = 1; /* no room to wait: ask again now */
        }
    }
    Permit();
    if (!done && !complete_warm(opener)) {
        complete_warm_forget(t); /* no warm-up to wait for: ask again now */
        done = 1;
    }
    return done;
}

void complete_warm_forget(struct Task *t)
{
    int i;
    Forbid();
    for (i = 0; i < WARM_WAITERS; i++)
        if (warm_waiters[i].task == t)
            warm_waiters[i].task = 0;
    Permit();
}

/* CHECK_COMMAND: resident, a path to a file, or a file in the current
 * directory, C: or the path. */
static int command_exists(struct complete_req *q)
{
    BPTR lock;
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
    return walk_command_dirs(q, 0, 1);
}

/* HISTORY_LOAD: the file's last HISTORY_KEEP lines into q->data (and the
 * file trimmed to them once it has grown past twice that). The buffer is
 * made here at the file's size (data_max at most, and data_max when the
 * size is not known): the window held 51 KB for a history of a few lines
 * while it opened. */
static void history_load(struct complete_req *q)
{
    BPTR f = Open((STRPTR)HISTORY_FILE, MODE_OLDFILE);
    struct FileInfoBlock *fib;
    long n, i, lines = 0, from = 0, size = q->data_max, keepn = q->keep > 0 ? q->keep : HISTORY_KEEP;
    q->data_len = 0;
    q->data = 0;
    if (!f)
        return;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib) {
        if (ExamineFH(f, fib) && fib->fib_Size > 0 && fib->fib_Size + 1 < size)
            size = fib->fib_Size + 1;
        FreeDosObject(DOS_FIB, fib);
    }
    q->data = (char *)AllocVec(size, MEMF_ANY);
    if (!q->data) {
        Close(f);
        return;
    }
    n = Read(f, q->data, size - 1);
    Close(f);
    if (n <= 0)
        return;
    for (i = n - 1; i >= 0; i--)
        if (q->data[i] == '\n' && ++lines > keepn) {
            from = i + 1;
            break;
        }
    memmove(q->data, q->data + from, n - from);
    q->data_len = n - from;
    if (from && lines > keepn) {
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

/* COMPLETE_ASL: KingCON's Tab on an empty word -- a file requester in the
 * current directory; the chosen path in add, a space after a file. */
static void asl_pick(struct complete_req *q)
{
    struct Library *AslBase = OpenLibrary((STRPTR)"asl.library", 37);
    struct FileRequester *fr;
    char drawer[COMPLETE_MAX];
    BPTR cd = CurrentDir(0);
    CurrentDir(cd);
    drawer[0] = 0;
    if (cd)
        NameFromLock(cd, (STRPTR)drawer, sizeof(drawer));
    if (!AslBase)
        return;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
            ASLFR_Screen, (ULONG)q->screen, ASLFR_TitleText, (ULONG)"Select filename",
            ASLFR_InitialDrawer, (ULONG)drawer, ASLFR_RejectIcons, (ULONG)!q->show_info,
            TAG_DONE);
    if (fr && AslRequest(fr, 0)) {
        int n;
        strncpy(q->add, (const char *)fr->fr_Drawer, COMPLETE_MAX - 2);
        q->add[COMPLETE_MAX - 2] = 0;
        AddPart((STRPTR)q->add, fr->fr_File, COMPLETE_MAX - 2);
        n = (int)strlen(q->add);
        if (n && q->add[n - 1] != '/' && q->add[n - 1] != ':')
            strcpy(q->add + n, " ");
        q->matches = q->add[0] != 0;
    }
    if (fr)
        FreeAslRequest(fr);
    CloseLibrary(AslBase);
}

/* COMPLETE_FONT: the Settings menu's Font... -- fixed-width fonts only, the
 * current one (q->word, q->font_size) preselected. */
static void font_pick(struct complete_req *q)
{
    struct Library *AslBase = OpenLibrary((STRPTR)"asl.library", 37);
    struct FontRequester *fr;
    if (!AslBase)
        return;
    fr = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest,
            ASLFO_Screen, (ULONG)q->screen, ASLFO_TitleText, (ULONG)"UP-Term font",
            ASLFO_FixedWidthOnly, TRUE, ASLFO_InitialName, (ULONG)q->word,
            ASLFO_InitialSize, (ULONG)(q->font_size ? q->font_size : 8), TAG_DONE);
    if (fr && AslRequest(fr, 0)) {
        strncpy(q->add, (const char *)fr->fo_Attr.ta_Name, COMPLETE_MAX - 1);
        q->add[COMPLETE_MAX - 1] = 0;
        q->font_size = fr->fo_Attr.ta_YSize;
        q->matches = 1;
    }
    if (fr)
        FreeAslRequest(fr);
    CloseLibrary(AslBase);
}

/* theme file -> q->data, its path in q->add; 0 when it cannot be read */
static int theme_read(struct complete_req *q, const char *path)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    long n;
    if (!f)
        return 0;
    n = Read(f, q->data, q->data_max - 1);
    Close(f);
    if (n <= 0)
        return 0;
    q->data_len = n;
    q->data[n] = 0;
    q->matches = 1;
    strncpy(q->add, path, COMPLETE_MAX - 1);
    q->add[COMPLETE_MAX - 1] = 0;
    return 1;
}

/* The themes drawer for q (prefs_dos_theme_drawer): the window's theme's
 * drawer, the kit's, its ENV: copy, or beside the handler's own file. */
static void theme_dir(struct complete_req *q, char *dir, int cap)
{
    char home[COMPLETE_MAX];
    const char *theme = "";
    home[0] = 0;
    if (q->extra && q->extra_len > 0) {
        theme = q->extra;
        strncpy(home, theme + strlen(theme) + 1, sizeof(home) - 1);
        home[sizeof(home) - 1] = 0;
        *PathPart((STRPTR)home) = 0;
    }
    prefs_dos_theme_drawer(theme, home, dir, cap);
}

/* COMPLETE_THEME: Settings > Theme... -- a theme file from the themes
 * drawer (prefs_dos_theme_drawer: W30, the requester opened wherever ASL
 * liked when the kit's drawer was missing), read into q->data
 * (q->data_max bytes at most). With q->word set (a typed "/theme NAME",
 * or an entry of /theme's list) that theme, no requester. */
static void theme_pick(struct complete_req *q)
{
    struct Library *AslBase;
    struct FileRequester *fr;
    char dir[COMPLETE_MAX], path[COMPLETE_MAX];
    q->data_len = 0;
    theme_dir(q, dir, sizeof(dir));
    if (q->word[0]) {
        prefs_theme_file(dir, q->word, path, sizeof(path));
        theme_read(q, path);
        return;
    }
    AslBase = OpenLibrary((STRPTR)"asl.library", 37);
    if (!AslBase)
        return;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
            ASLFR_Screen, (ULONG)q->screen, ASLFR_TitleText, (ULONG)"UP-Term theme",
            ASLFR_InitialDrawer, (ULONG)dir, ASLFR_InitialPattern,
            (ULONG)"#?.conf", ASLFR_DoPatterns, TRUE, ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (fr && AslRequest(fr, 0) && fr->fr_File[0]) {
        strncpy(path, (const char *)fr->fr_Drawer, sizeof(path) - 2);
        path[sizeof(path) - 2] = 0;
        AddPart((STRPTR)path, fr->fr_File, sizeof(path) - 2);
        theme_read(q, path);
    }
    if (fr)
        FreeAslRequest(fr);
    CloseLibrary(AslBase);
}

/* COMPLETE_THEMES: /theme's list -- the theme names in the themes drawer
 * (the requester's), sorted (prefs_theme_add), into q->names. */
static void theme_list(struct complete_req *q)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    int len = 0;
    q->names_len = 0;
    theme_dir(q, q->add, COMPLETE_MAX);
    if (!q->add[0] || !q->names || !(lock = Lock((STRPTR)q->add, SHARED_LOCK)))
        return;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib) && fib->fib_DirEntryType > 0)
        while (ExNext(lock, fib))
            if (fib->fib_DirEntryType < 0 &&
                prefs_theme_add(q->names, &len, COMPLETE_NAMES, (const char *)fib->fib_FileName) > 0)
                q->matches++;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    q->names_len = len;
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
    q->partial = 0;

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

    if (q->mode == COMPLETE_ASL) {
        asl_pick(q);
    } else if (q->mode == COMPLETE_FONT) {
        font_pick(q);
    } else if (q->mode == COMPLETE_THEME) {
        theme_pick(q);
    } else if (q->mode == COMPLETE_THEMES) {
        theme_list(q);
    } else if (q->mode == CONFIG_SAVE) {
        int failed;
        q->font_size = prefs_dos_save(q->data, q->data_len, 1, &failed, q->hlcat);
        q->matches = q->font_size == PREFS_INSTALL_OK;
    } else if (q->mode == CHECK_COMMAND) {
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
        lock = q->mode == COMPLETE_DEVICES || (q->ghost && !ghost_volume_known(dirpart))
                   ? 0
                   : Lock((STRPTR)dirpart, ACCESS_READ);
        if (lock) {
            /* KingCON's Alt+Tab: commands only, in the word's directory too
             * (unix keeps directories there: a directory's name is a cd) */
            scan_dir(q, lock, prefix, q->kingcon && q->mode == COMPLETE_COMMANDS);
            UnLock(lock);
        }
        if (q->mode == COMPLETE_COMMANDS && split < 0) {
            command_names(q, prefix);
            scan_residents(q, prefix);
            scan_extra(q, prefix);
        }
        /* KingCON: file names that find nothing are device names */
        if (q->mode == COMPLETE_DEVICES || (q->kingcon && q->mode == COMPLETE_FILES && !q->matches && !q->ghost)) {
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
            if (q->matches == 1 && !q->partial)
                strcat(q->add, q->is_dir ? "/" : " "); /* partial: the word may go on */
        }
    }
    if (dir) {
        CurrentDir(old);
        UnLock(dir);
    }
    Forbid(); /* the reply and our end, before the handler can free anything */
    ReplyMsg(&q->msg);
}

struct complete_req *complete_req_new(int lists)
{
    struct complete_req *q = (struct complete_req *)AllocVec(
        sizeof(struct complete_req) + (lists ? COMPLETE_NAMES + COMPLETE_EXTRA : 0), MEMF_CLEAR);
    if (q && lists) {
        q->names = (char *)(q + 1);
        q->extra = q->names + COMPLETE_NAMES;
    }
    return q;
}

int complete_start(struct complete_req *q, struct MsgPort *reply, struct Process *opener)
{
    struct Process *w;
    q->msg.mn_ReplyPort = reply;
    q->msg.mn_Length = sizeof(*q);
    q->opener = opener;
    w = CreateNewProcTags(NP_Entry, (ULONG)worker, NP_Name, (ULONG)"vtcon completion",
                          NP_StackSize, 16000, NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE,
                          NP_CloseOutput, FALSE, NP_ConsoleTask, 0, TAG_DONE);
    if (!w)
        return 0;
    PutMsg(&w->pr_MsgPort, &q->msg);
    return 1;
}
