/* handler/complete_core: the rules of the completion worker's command
 * lookup, fed what a host fake of AmigaDOS reports. */
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

void suite_complete(void)
{
    command_list_searches_every_directory_of_a_multi_assigned_c();
    command_list_skips_directories_and_files_without_e_or_s();
    command_list_skips_system_and_disabled_residents();
}
