/* Start icons as a Workbench double-click does (plan 2026-10-23-amarchy-app-index.md, X5). The Amiga
 * part is wblaunch.c, shared by wbrun (tests/amiga/wbrun.c) and Amarchy's index rows (leaf.c). */
#ifndef AMARCHY_WBLAUNCH_H
#define AMARCHY_WBLAUNCH_H

#include <exec/ports.h>
#include <workbench/startup.h>

/* Start names[0..n) as one Workbench launch: a project (WBPROJECT) starts its default tool with the
 * project; otherwise names[0] is the tool and the others are the icons shift-selected with it. Each
 * name is a path without .info. The program replies its WBStartup message when it ends.
 *  - reply 0: the call waits for that reply and frees everything itself.
 *  - reply a port that lives as long as the program may (Amarchy's): the call returns at once; each
 *    reply that arrives at the port is the launch's record, to be given to wbl_free. A port deleted
 *    while a launch is out is a crash when the program ends.
 * 0 on success, else -1 with the reason in err (at most errsz bytes). */
int wbl_start(const char *const *names, int n, struct MsgPort *reply, char *err, int errsz);

/* Free a launch's message, segment and locks: msg is what GetMsg returned from the reply port. */
void wbl_free(struct WBStartup *msg);

/* An index row's tool or project: workbench.library's OpenWorkbenchObject (V44 or newer) first, the
 * WBStartup message when that library is missing or answers FALSE (Directory Opus in place of
 * Workbench does, thoughts/shared/research/2026-10-17_wb-icon-launch.md). The message is sent by the
 * running Amarchy (port LF_LAUNCH_PORT), which owns the launch until the program ends; without
 * Amarchy this process starts it and waits for the program (reply 0). 0 or -1 as above. */
int wbl_open(const char *path, char *err, int errsz);

#endif
