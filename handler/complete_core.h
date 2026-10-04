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

#endif
