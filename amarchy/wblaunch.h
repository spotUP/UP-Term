/* Start icons as a Workbench double-click does (plan 2026-10-23-amarchy-app-index.md, X5). The Amiga
 * part is wblaunch.c, shared by wbrun (tests/amiga/wbrun.c) and Amarchy's index rows (leaf.c). */
#ifndef AMARCHY_WBLAUNCH_H
#define AMARCHY_WBLAUNCH_H

/* Start names[0..n) as one Workbench launch: a project (WBPROJECT) starts its default tool with the
 * project; otherwise names[0] is the tool and the others are the icons shift-selected with it. Each
 * name is a path without .info. With wait the call returns after the program replied its WBStartup
 * message; without it the program runs on its own (its message has no reply port, so the call may
 * return, and the process end, before the program does). 0 on success, else -1 with the reason in
 * err (at most errsz bytes). */
int wbl_start(const char *const *names, int n, int wait, char *err, int errsz);

/* An index row's tool or project: workbench.library's OpenWorkbenchObject (V44 or newer) first, the
 * WBStartup message of wbl_start when that library is missing or answers FALSE (Directory Opus in
 * place of Workbench does, thoughts/shared/research/2026-10-17_wb-icon-launch.md). 0 or -1 as above. */
int wbl_open(const char *path, char *err, int errsz);

#endif
