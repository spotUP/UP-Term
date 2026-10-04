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

#endif
