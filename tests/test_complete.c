/* handler/complete_core: the rules of the completion worker's command
 * lookup, fed what a host fake of AmigaDOS reports. */
#include <stdlib.h>
#include "harness.h"
#include "../handler/complete_core.h"

/* protection as fib_Protection: R W E D set = not allowed */
#define P_NOEXEC CC_FIBF_EXECUTE

/* H8.2: Alt+Tab lists only files with the execute or the script bit,
 * never directories (KingCON-handler.asm lbC007E08) */
static void command_list_skips_directories_and_files_without_e_or_s(void)
{
    CHECK_INT(cc_is_command(-3, 0), 1);                         /* rwed file */
    CHECK_INT(cc_is_command(-3, P_NOEXEC), 0);                  /* rw-d: data */
    CHECK_INT(cc_is_command(-3, P_NOEXEC | CC_FIBF_SCRIPT), 1); /* s, no e */
    CHECK_INT(cc_is_command(-3, CC_FIBF_SCRIPT), 1);
    CHECK_INT(cc_is_command(2, 0), 0);                          /* a directory */
    CHECK_INT(cc_is_command(4, 0), 0);                          /* a soft link to one */
}

/* H8.4: residents offered are seg_UC >= 0 or CMD_INTERNAL (-2) only, as
 * KingCON's resident walk; CMD_SYSTEM (-1) segments are not commands */
static void command_list_skips_system_and_disabled_residents(void)
{
    CHECK_INT(cc_resident_listed(0), 1);      /* resident, not in use */
    CHECK_INT(cc_resident_listed(3), 1);      /* in use three times */
    CHECK_INT(cc_resident_listed(-2), 1);     /* CMD_INTERNAL: Alias, CD, ... */
    CHECK_INT(cc_resident_listed(-1), 0);     /* CMD_SYSTEM */
    CHECK_INT(cc_resident_listed(-999), 0);   /* CMD_DISABLED */
}

/* A fake AmigaDOS for the command-directory walk: a lock is
 * 100 * serial + directory, so two locks on one directory differ but
 * same() knows them; C: is assigned to c_dirs, the path is p_dirs. */
typedef struct fake_dos {
    const int *c_dirs, *p_dirs;
    int c_at, p_at, serial;
    int c_ended, outstanding;  /* c_next locks not yet dropped */
    int stop_at;               /* visit() of this directory ends the walk */
    int visited[16], nvisited;
} fake_dos;

static long fk_c_next(void *u)
{
    fake_dos *f = (fake_dos *)u;
    if (!f->c_dirs[f->c_at])
        return 0;
    f->outstanding++;
    return 100L * ++f->serial + f->c_dirs[f->c_at++];
}
static void fk_c_end(void *u) { ((fake_dos *)u)->c_ended++; }
static long fk_p_next(void *u)
{
    fake_dos *f = (fake_dos *)u;
    if (!f->p_dirs[f->p_at])
        return 0;
    return 100L * ++f->serial + f->p_dirs[f->p_at++];
}
static fake_dos *fk; /* drop() has no u: the walk under test */
static int fk_same(long a, long b) { return a % 100 == b % 100; }
static void fk_drop(long lock) { (void)lock; fk->outstanding--; }
static int fk_visit(void *u, long lock)
{
    fake_dos *f = (fake_dos *)u;
    int d = (int)(lock % 100);
    f->visited[f->nvisited++] = d;
    return d == f->stop_at;
}
static const cc_dirs_os fake_os = { fk_c_next, fk_c_end, fk_p_next, fk_same, fk_drop, fk_visit };

static const char *visits(const fake_dos *f)
{
    static char b[64];
    int i, n = 0;
    for (i = 0; i < f->nvisited; i++) { /* directories 1..9 */
        if (i)
            b[n++] = ' ';
        b[n++] = (char)('0' + f->visited[i]);
    }
    b[n] = 0;
    return b;
}

/* H8.3: Alt+Tab searches every directory of a multi-assigned C: (KingCON's
 * GetDeviceProc loop), not only the first; a directory met again (the
 * path naming one of C:'s) is searched once */
static void command_list_searches_every_directory_of_a_multi_assigned_c(void)
{
    static const int c_dirs[] = { 1, 2, 0 }, p_dirs[] = { 3, 1, 2, 4, 0 };
    fake_dos f;
    fk = &f;
    memset(&f, 0, sizeof(f));
    f.c_dirs = c_dirs;
    f.p_dirs = p_dirs;
    CHECK_INT(cc_walk_command_dirs(&fake_os, &f), 0);
    CHECK_STR(visits(&f), "1 2 3 4");
    CHECK_INT(f.c_ended, 1);
    CHECK_INT(f.outstanding, 0);           /* every C: lock dropped */

    memset(&f, 0, sizeof(f));              /* a lookup that finds it in C:'s second */
    f.c_dirs = c_dirs;
    f.p_dirs = p_dirs;
    f.stop_at = 2;
    CHECK_INT(cc_walk_command_dirs(&fake_os, &f), 1);
    CHECK_STR(visits(&f), "1 2");
    CHECK_INT(f.c_ended, 1);
    CHECK_INT(f.outstanding, 0);
}

/* ---- W22: the command-name cache, over a fake file system ---------------
 * Directory d (1..9) is lock % 100 in the walk above; its names and date are
 * here, every read of it is counted, the cache file is a buffer, and every
 * allocation is counted so the tests see what RAM holds. */

#define FS_DIRS 10

typedef struct fake_sys {
    fake_dos d;                        /* first: the walk's callbacks take it */
    const char *names[FS_DIRS];        /* "a b c": the commands of directory d */
    long date[FS_DIRS];                /* its date (days) */
    int reads[FS_DIRS];                /* scans of it */
    char file[CC_CACHE_FILE_MAX];
    long file_len;                     /* -1: no file */
    int saves, save_fails;
    long held;                         /* bytes allocated, not freed */
    long alloc_fail_over;              /* allocations above this fail (0: none) */
    int locked;
    char got[512];                     /* the names emitted, space-separated */
} fake_sys;

static fake_sys *fs; /* the system under test (the cache's calls have no u) */

static void fs_lock(void) { CHECK_INT(fs->locked++, 0); }
static void fs_unlock(void) { CHECK_INT(--fs->locked, 0); }
static void *fs_alloc(long size)
{
    if (fs->alloc_fail_over && size > fs->alloc_fail_over)
        return 0;
    fs->held += size;
    return malloc((size_t)size);
}
static void fs_free(void *p, long size)
{
    fs->held -= size;
    free(p);
}
static int fs_stat(void *u, long lock, char *name, int max, cc_date *date)
{
    fake_sys *s = (fake_sys *)u;
    int d = (int)(lock % 100);
    CHECK(max >= 9);
    strcpy(name, "DH0:Dir0");
    name[7] = (char)('0' + d);
    date->days = s->date[d];
    date->minute = 7;
    date->tick = 0;
    return 1;
}
static int fs_scan(void *u, long lock, cc_name_fn add, void *x)
{
    fake_sys *s = (fake_sys *)u;
    int d = (int)(lock % 100);
    const char *p = s->names[d];
    char w[64];
    s->reads[d]++;
    while (p && *p) {
        int n = 0;
        while (*p && *p != ' ')
            w[n++] = *p++;
        w[n] = 0;
        while (*p == ' ')
            p++;
        if (!add(x, w))
            return 0;
    }
    return 1;
}
static long fs_load(char *buf, long max)
{
    if (fs->file_len < 0 || fs->file_len > max)
        return -1;
    memcpy(buf, fs->file, (size_t)fs->file_len);
    return fs->file_len;
}
static int fs_save(const char *buf, long len)
{
    if (fs->save_fails)
        return 0;
    memcpy(fs->file, buf, (size_t)len);
    fs->file_len = len;
    fs->saves++;
    return 1;
}
static const cc_cache_os fake_cache_os = { fs_lock, fs_unlock, fs_alloc, fs_free, fs_stat, fs_scan, fs_load, fs_save };

static int fs_emit(void *x, const char *name)
{
    fake_sys *s = (fake_sys *)x;
    if (s->got[0])
        strcat(s->got, " ");
    strcat(s->got, name);
    return 1;
}

/* C: is directories 1 and 2, the path 3 */
static const int fs_c[] = { 1, 2, 0 }, fs_p[] = { 3, 0 };

static void fs_init(fake_sys *s)
{
    memset(s, 0, sizeof(*s));
    s->d.c_dirs = fs_c;
    s->d.p_dirs = fs_p;
    s->names[1] = "Assign Copy Dir";
    s->names[2] = "Ed List";
    s->names[3] = "MultiView";
    s->date[1] = s->date[2] = s->date[3] = 100;
    s->file_len = -1;
    fk = &s->d;
    fs = s;
}

/* a new walk over the same directories (the walk's state is per request) */
static void fs_rewalk(fake_sys *s)
{
    s->d.c_at = s->d.p_at = s->d.serial = 0;
    s->d.c_ended = s->d.outstanding = 0;
    s->d.nvisited = 0;
    s->got[0] = 0;
}

/* a Tab: the command names (cold 0, as the worker asks first); 1 = partial */
static int fs_tab(cc_cache *c, fake_sys *s, int cold)
{
    int partial;
    fs_rewalk(s);
    partial = cc_command_names(c, &fake_cache_os, &fake_os, s, cold, fs_emit, s);
    cc_cache_end(c, &fake_cache_os);
    CHECK_INT(s->d.outstanding, 0);
    return partial;
}

static void fs_warm(cc_cache *c, fake_sys *s)
{
    fs_rewalk(s);
    cc_cache_warm(c, &fake_cache_os, &fake_os, s);
}

static int reads(const fake_sys *s)
{
    return s->reads[1] + s->reads[2] + s->reads[3];
}

static void cc_free_all(cc_cache *c, fake_sys *s)
{
    cc_cache_release(c, &fake_cache_os);
    CHECK_INT(s->held, 0); /* nothing leaks */
}

/* W22 (c): a Tab never waits for a scan: on a cold cache it answers at once,
 * from nothing, partial; it reads no directory itself */
static void first_tab_never_scans_a_cold_directory(void)
{
    fake_sys s;
    cc_cache c;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    CHECK_INT(fs_tab(&c, &s, 0), 1);
    CHECK_INT(reads(&s), 0);
    CHECK_STR(s.got, "");
    cc_free_all(&c, &s);
}

/* W22 reachability: the Tab's path (cc_command_names, as complete.c's
 * worker calls it for COMPLETE_COMMANDS) answers in full once the warm-up
 * ran -- the sentinel proves the warm-up ran and read each directory once,
 * and the Tab after it read none */
static void tab_after_the_warm_up_is_answered_from_the_cache(void)
{
    fake_sys s;
    cc_cache c;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    CHECK_INT(fs_tab(&c, &s, 0), 1);        /* cold: partial */
    fs_warm(&c, &s);
    CHECK_INT(c.warmups, 1);                /* the sentinel */
    CHECK_INT(c.scans, 3);
    CHECK_INT(reads(&s), 3);
    CHECK_INT(fs_tab(&c, &s, 0), 0);        /* asked again: complete */
    CHECK_STR(s.got, "Assign Copy Dir Ed List MultiView");
    CHECK_INT(reads(&s), 3);                /* from RAM: no read */
    cc_free_all(&c, &s);
}

/* W22 (b): the file -- written after the warm-up, read back whole by a new
 * cache (a reboot), which then answers with no read at all */
static void cache_file_round_trips_through_a_reboot(void)
{
    fake_sys s;
    cc_cache c, after;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    CHECK_INT(s.saves, 1);
    CHECK_INT(cc_cache_parse(s.file, s.file_len, 0, 0), 3);
    cc_free_all(&c, &s);

    cc_cache_init(&after, CC_CACHE_CAP, CC_CACHE_CAP); /* the reboot */
    CHECK_INT(fs_tab(&after, &s, 0), 0);
    CHECK_STR(s.got, "Assign Copy Dir Ed List MultiView");
    CHECK_INT(reads(&s), 3);                /* still only the warm-up's */
    CHECK_INT(s.saves, 1);                  /* nothing new to write */
    cc_free_all(&after, &s);
}

/* W22 (b): a file cut short, with a byte changed, or of another version is
 * not read (the directories are read again, nothing crashes) */
static void cache_file_corrupt_or_old_is_ignored(void)
{
    fake_sys s;
    cc_cache c;
    char good[CC_CACHE_FILE_MAX];
    long len;
    int k;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    cc_free_all(&c, &s);
    len = s.file_len;
    memcpy(good, s.file, (size_t)len);
    for (k = 0; k < 4; k++) {
        memcpy(s.file, good, (size_t)len);
        s.file_len = len;
        if (k == 0)
            s.file_len = len - 5;           /* cut short */
        else if (k == 1)
            s.file[CC_CACHE_HEAD + CC_CACHE_EHEAD + 3] ^= 0x20; /* a name's byte */
        else if (k == 2)
            s.file[7] = 0;                  /* version 0: an older one */
        else
            memcpy(s.file, "XXXX", 4);
        CHECK_INT(cc_cache_parse(s.file, s.file_len, 0, 0), -1);
        cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
        CHECK_INT(fs_tab(&c, &s, 0), 1);    /* nothing believed */
        CHECK_STR(s.got, "");
        cc_free_all(&c, &s);
    }
    /* a version-0 file is replaced by the next warm-up */
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    CHECK_INT(cc_cache_parse(s.file, s.file_len, 0, 0), 3);
    cc_free_all(&c, &s);
}

/* W22 (b): the delta -- only a directory whose date changed is read again,
 * in RAM and across a reboot; until then a Tab gets its old names, partial */
static void only_changed_directories_are_read_again(void)
{
    fake_sys s;
    cc_cache c;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    s.names[2] = "Ed List Lha";             /* something installed in C:'s second */
    s.date[2] = 101;
    CHECK_INT(fs_tab(&c, &s, 0), 1);        /* partial: Dir2 changed */
    CHECK_STR(s.got, "Assign Copy Dir Ed List MultiView"); /* its old names meanwhile */
    fs_warm(&c, &s);
    CHECK_INT(s.reads[1], 1);
    CHECK_INT(s.reads[2], 2);               /* only Dir2 again */
    CHECK_INT(s.reads[3], 1);
    CHECK_INT(fs_tab(&c, &s, 0), 0);
    CHECK_STR(s.got, "Assign Copy Dir Ed List Lha MultiView");
    cc_free_all(&c, &s);

    s.names[3] = "MultiView Installer";     /* changed while switched off */
    s.date[3] = 102;
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    CHECK_INT(s.reads[1], 1);
    CHECK_INT(s.reads[2], 2);
    CHECK_INT(s.reads[3], 2);               /* only Dir3 */
    CHECK_INT(fs_tab(&c, &s, 0), 0);
    CHECK_STR(s.got, "Assign Copy Dir Ed List Lha MultiView Installer");
    cc_free_all(&c, &s);
}

/* W22 (d): RAM never holds more than the cap; the least recently used
 * directory goes first; one bigger than the cap is answered, not kept */
static void cache_stays_under_its_cap_least_recently_used_first(void)
{
    fake_sys s;
    cc_cache c;
    long cap;
    fs_init(&s);
    /* the entries: "DH0:Dir1\0Assign\0Copy\0Dir\0" 25 bytes, Dir2 17, Dir3 19 */
    cap = 25 + 19;
    cc_cache_init(&c, cap, cap);
    s.save_fails = 1;                       /* no file: RAM alone */
    fs_warm(&c, &s);
    CHECK(c.bytes <= cap);
    CHECK_INT(s.held, c.bytes);
    CHECK_INT(c.bytes, 17 + 19);            /* Dir1, the oldest, went for Dir3 */
    s.got[0] = 0;
    fs_rewalk(&s);
    CHECK_INT(cc_cache_dir(&c, &fake_cache_os, &s, 2, 0, fs_emit, &s), CC_FRESH);
    CHECK_INT(cc_cache_dir(&c, &fake_cache_os, &s, 1, 1, fs_emit, &s), CC_FRESH); /* read */
    CHECK(c.bytes <= cap);
    CHECK_INT(c.bytes, 25 + 17);            /* Dir3, used least lately, went */
    CHECK_INT(cc_cache_dir(&c, &fake_cache_os, &s, 3, 0, fs_emit, &s), CC_MISSING);

    s.names[3] = "AVeryLongCommandName AnotherVeryLongOne YetAnotherOne";
    s.date[3] = 200;
    s.got[0] = 0;
    CHECK_INT(cc_cache_dir(&c, &fake_cache_os, &s, 3, 1, fs_emit, &s), CC_FRESH);
    CHECK_STR(s.got, "AVeryLongCommandName AnotherVeryLongOne YetAnotherOne");
    CHECK(c.bytes <= cap);                  /* answered, never kept */
    CHECK_INT(c.bytes, 25 + 17);
    CHECK_INT(s.held, c.bytes);
    cc_free_all(&c, &s);
}

/* W22 (d): under memory pressure everything goes (not while a lookup
 * holds the lock); the next Tab is answered from the file, with no read */
static void low_memory_releases_every_cached_name(void)
{
    fake_sys s;
    cc_cache c;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    CHECK(c.bytes > 0);
    c.in_use = 1;                           /* a lookup runs: it keeps them */
    CHECK_INT(cc_cache_release(&c, &fake_cache_os), 0);
    CHECK(c.bytes > 0);
    c.in_use = 0;
    CHECK_INT(cc_cache_release(&c, &fake_cache_os), 25 + 17 + 19);
    CHECK_INT(c.bytes, 0);
    CHECK_INT(s.held, 0);
    CHECK_INT(fs_tab(&c, &s, 0), 0);
    CHECK_STR(s.got, "Assign Copy Dir Ed List MultiView");
    CHECK_INT(reads(&s), 3);
    cc_free_all(&c, &s);
}

/* W22 (d): without fast RAM (keep 0) nothing stays in RAM between
 * requests: the file is the cache, read at each Tab (no directory read) */
static void chip_only_machine_keeps_the_cache_on_disk(void)
{
    fake_sys s;
    cc_cache c;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, 0);
    fs_warm(&c, &s);
    CHECK_INT(c.bytes, 0);
    CHECK_INT(s.held, 0);
    CHECK_INT(fs_tab(&c, &s, 0), 0);
    CHECK_STR(s.got, "Assign Copy Dir Ed List MultiView");
    CHECK_INT(c.bytes, 0);
    CHECK_INT(s.held, 0);
    CHECK_INT(reads(&s), 3);
    /* the file not written (a write-protected disk): RAM keeps it */
    s.names[1] = "Assign Copy Dir Lha";
    s.date[1] = 103;
    s.save_fails = 1;
    fs_warm(&c, &s);
    CHECK(c.bytes > 0);
    CHECK_INT(fs_tab(&c, &s, 0), 0);
    CHECK_STR(s.got, "Assign Copy Dir Lha Ed List MultiView");
    cc_free_all(&c, &s);
}

/* KingCON's Reset cache: every directory read again, the file not believed */
static void reset_cache_reads_every_directory_again(void)
{
    fake_sys s;
    cc_cache c;
    fs_init(&s);
    cc_cache_init(&c, CC_CACHE_CAP, CC_CACHE_CAP);
    fs_warm(&c, &s);
    cc_cache_reset(&c, &fake_cache_os);
    CHECK_INT(fs_tab(&c, &s, 0), 1);
    cc_free_all(&c, &s);                    /* even with RAM empty ... */
    fs_warm(&c, &s);
    CHECK_INT(reads(&s), 6);                /* ... the file's dates are not believed */
    CHECK_INT(fs_tab(&c, &s, 0), 0);
    cc_free_all(&c, &s);
}

void suite_complete(void)
{
    first_tab_never_scans_a_cold_directory();
    tab_after_the_warm_up_is_answered_from_the_cache();
    cache_file_round_trips_through_a_reboot();
    cache_file_corrupt_or_old_is_ignored();
    only_changed_directories_are_read_again();
    cache_stays_under_its_cap_least_recently_used_first();
    low_memory_releases_every_cached_name();
    chip_only_machine_keeps_the_cache_on_disk();
    reset_cache_reads_every_directory_again();
    command_list_searches_every_directory_of_a_multi_assigned_c();
    command_list_skips_directories_and_files_without_e_or_s();
    command_list_skips_system_and_disabled_residents();
}
