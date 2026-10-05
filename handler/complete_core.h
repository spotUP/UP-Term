/* The portable rules of complete.c's command lookup (host-tested in
 * tests/test_complete.c); complete.c feeds them what AmigaDOS reports.
 * The rules follow KingCON by David Larsson as
 * thoughts/shared/research/2026-10-02_kingcon-completion.md records it;
 * no code of KingCON is used. */
#ifndef COMPLETE_CORE_H
#define COMPLETE_CORE_H

/* dos/dos.h's protection bits, the ones these rules read (complete.c
 * checks they agree). R W E D are set when the action is NOT allowed. */
#define CC_FIBF_EXECUTE 2L
#define CC_FIBF_SCRIPT  64L

/* Is a directory entry a command? A file (entry_type < 0, as
 * fib_DirEntryType) whose protection allows execution or carries the
 * script bit; never a directory. */
int cc_is_command(long entry_type, unsigned long protection);

/* dos/dosextens.h's seg_UC for the Shell's own commands (complete.c
 * checks it agrees) */
#define CC_CMD_INTERNAL (-2L)

/* Is a resident list entry a command to offer? seg_UC >= 0 (a resident
 * program, its use count) or CMD_INTERNAL (the Shell's own); not
 * CMD_SYSTEM (-1, the system's segments) nor CMD_DISABLED. */
int cc_resident_listed(long seg_uc);

/* The directories a command is looked for in: every directory C: is
 * assigned to (a multi-assign has several), then the Shell's path. One
 * already searched -- the same directory, whatever it is called -- is
 * skipped, as KingCON does with SameLock. Directories are locks (BPTR on
 * the Amiga); the OS side is behind these calls (u passed to those that
 * keep state): */
typedef struct cc_dirs_os {
    long (*c_next)(void *u);           /* the next directory of C: (the first, at
                                        * first), a lock of the walk's own that
                                        * it gives to drop(); 0: no more */
    void (*c_end)(void *u);            /* the C: walk is over (ended or cut short) */
    long (*p_next)(void *u);           /* the next directory of the path, a lock
                                        * the Shell owns; 0: no more */
    int  (*same)(long a, long b);      /* the same directory? */
    void (*drop)(long lock);           /* a c_next lock is done with */
    int  (*visit)(void *u, long lock); /* search it; nonzero ends the walk */
} cc_dirs_os;

#define CC_DIRS_MAX 32 /* directories remembered for the same() check */

/* Walk them; returns what the visit that ended the walk returned, or 0. */
int cc_walk_command_dirs(const cc_dirs_os *os, void *u);

/* ---- W22: the command-name cache ------------------------------------------
 *
 * What a command directory holds (the names cc_is_command takes), kept per
 * directory and keyed by its canonical name (NameFromLock): one packed
 * buffer each -- the directory's name, NUL, then its command names, a NUL
 * after each -- with the directory's date. A directory whose date is still
 * the cached one is answered from the buffer; only a changed directory is
 * read again (the delta). The cache is also a file (cc_cache_os load/save,
 * the format cc_cache_pack writes), so a reboot pays no scan.
 *
 * RAM: every buffer counts toward `bytes`, which never exceeds `cap`; the
 * least recently used directory goes first. Between requests at most `keep`
 * bytes stay (0 on a machine without fast RAM: the file is the cache there,
 * read again at the next request). cc_cache_release frees everything (the
 * low-memory handler, KingCON's Purge cache).
 *
 * A Tab never waits for a scan: with cold 0, a directory not cached (or
 * changed) is answered from what is cached and the answer is partial; the
 * warm-up (cold 1, a low-priority process) reads it and the Tab is asked
 * again. Measured 2026-10-04 on the rig's disks, read as files (build/rig):
 * the owner's 3.1 disk, C: and its Startup-sequence path: 295 commands,
 * 2721 bytes packed; the 3.2 install: 75 commands, 579 bytes. */

#define CC_CACHE_DIRS    32                /* directories cached at most */
#define CC_CACHE_CAP     16384L            /* bytes of packed buffers in RAM at most:
                                            * 6x the measured 3.1 set */
#define CC_CACHE_NAMELEN 256               /* a directory's name, its NUL included */
#define CC_CACHE_HEAD    16                /* the file: "UPCC", version, count, payload */
#define CC_CACHE_EHEAD   16                /* an entry: days, minute, tick, size */
#define CC_CACHE_FILE_MAX (CC_CACHE_HEAD + CC_CACHE_DIRS * (long)CC_CACHE_EHEAD + CC_CACHE_CAP + 4)
#define CC_CACHE_VERSION 1L

typedef struct cc_date {
    long days, minute, tick;               /* struct DateStamp */
} cc_date;

typedef struct cc_centry {
    char *buf;                             /* name NUL names...; 0: a free slot */
    long size;                             /* bytes of buf */
    long nlen;                             /* the name's bytes, its NUL included */
    cc_date date;
    unsigned long used;                    /* the clock at its last use */
} cc_centry;

typedef struct cc_cache {
    cc_centry e[CC_CACHE_DIRS];
    long bytes;                            /* the sizes summed: never above cap */
    long cap, keep;                        /* see above */
    unsigned long clock;
    int loaded;                            /* the file's entries were offered since
                                            * the last eviction */
    int dirty;                             /* RAM has what the file has not */
    int file_stale;                        /* Reset cache: the file is neither read
                                            * nor merged until the next save */
    int in_use;                            /* a lookup holds the lock: no release */
    long scans;                            /* directories read (warm-up or cold) */
    long warmups;                          /* cc_cache_warm runs */
} cc_cache;

/* a name to someone: nonzero to go on */
typedef int (*cc_name_fn)(void *x, const char *name);

/* u: the request's own (the walk's state, its FileInfoBlock) */
typedef struct cc_cache_os {
    void (*lock)(void);                    /* the cache's semaphore */
    void (*unlock)(void);
    void *(*alloc)(long size);             /* 0: no memory (or below the floor) */
    void (*free)(void *p, long size);
    /* the directory's canonical name (max bytes with its NUL) and date; 0: unknown */
    int (*stat)(void *u, long lock, char *name, int max, cc_date *date);
    /* its command names, each to add (which may stop it); 0: not read */
    int (*scan)(void *u, long lock, cc_name_fn add, void *x);
    long (*load)(char *buf, long max);     /* the file into buf: its bytes, or -1 */
    int (*save)(const char *buf, long len); /* nonzero: written whole */
} cc_cache_os;

/* what cc_cache_dir answered from */
#define CC_FRESH   0   /* the names as the directory is now */
#define CC_STALE   1   /* the names of an older date (partial) */
#define CC_MISSING 2   /* none (partial) */
#define CC_NOSTAT  3   /* the directory has no name or date: read directly */

void cc_cache_init(cc_cache *c, long cap, long keep);

/* One directory's command names to emit (0: none wanted). cold: read it
 * now when the cache cannot answer for its current date. */
int cc_cache_dir(cc_cache *c, const cc_cache_os *os, void *u, long lock, int cold,
                 cc_name_fn emit, void *x);

/* The command names of every command directory (cc_walk_command_dirs over
 * dirs; u is passed to both interfaces); 1 when the answer is partial (a
 * directory's names were not current). The Tab's path. */
int cc_command_names(cc_cache *c, const cc_cache_os *os, const cc_dirs_os *dirs, void *u,
                     int cold, cc_name_fn emit, void *x);

/* The warm-up: every command directory read when changed, then
 * cc_cache_end. */
void cc_cache_warm(cc_cache *c, const cc_cache_os *os, const cc_dirs_os *dirs, void *u);

/* A request is over: the file written when RAM has news, then RAM cut to
 * `keep` (an entry the file lacks stays). */
void cc_cache_end(cc_cache *c, const cc_cache_os *os);

/* Every directory read again at its next use (KingCON's Reset cache). */
void cc_cache_reset(cc_cache *c, const cc_cache_os *os);

/* Free every buffer, unless a lookup holds the lock (in_use). The caller
 * holds the lock, or runs where no lookup can. Returns the bytes freed. */
long cc_cache_release(cc_cache *c, const cc_cache_os *os);

/* The file: RAM's entries (most recently used first), then old's (a file
 * read before; 0 for none) for directories RAM lacks, while CC_CACHE_DIRS
 * and CC_CACHE_CAP allow. Returns its bytes, 0 when max is too small. */
long cc_cache_pack(const cc_cache *c, const char *old, long oldlen, char *out, long max);

/* Each entry of a cache file to each(x, date, buffer, size) (nonzero to go
 * on). Returns the entry count, or -1 when the file is not one this version
 * wrote whole (another version, cut short, a bad checksum, a bad entry);
 * each is then not called at all. */
int cc_cache_parse(const char *buf, long len,
                   int (*each)(void *x, const cc_date *d, const char *ent, long size), void *x);

#endif
