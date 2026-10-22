/* Amarchy commodity: broker and hotkey objects (plan 2026-10-22-amarchy-commodity.md section 2). */
#ifndef AMARCHY_CX_H
#define AMARCHY_CX_H

#include <exec/types.h>
#include "../config/keymap.h"

enum { CXE_NONE = 0, CXE_KEY, CXE_ENABLE, CXE_DISABLE, CXE_KILL, CXE_UNIQUE, CXE_APPEAR, CXE_DISAPPEAR };

/* Register the broker "Amarchy". 0 when it is already running (the running one
 * is told: CXE_UNIQUE) or on failure. */
int  cx_open(int priority);
void cx_close(void);
ULONG cx_sigmask(void);
/* Replace the hotkey set by the keymap's commodity-side rows (scope C or B; a
 * terminal-only row is also filtered so a plain console gets it, see cx.c). */
int  cx_install(const keymap *km);
/* Stop reading the installed keymap (before it is released by a reload); cx_install resumes. */
void cx_detach(void);
/* The next event: CXE_*; *row is the row index for CXE_KEY. CXE_NONE when drained. */
int  cx_next(int *row);
/* Exchange Disable / Enable state, 1 = the hotkeys are active. */
int  cx_enabled(void);
const km_bind *cx_row(int row);
int  cx_nrows(void);
#endif
