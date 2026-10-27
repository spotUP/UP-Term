/* job_dos: the DOS side of the Amarchy job view (plan 2026-10-27-amarchy-job-view.md RJ.1). It runs one command
 * asynchronously through the UP-Term shell (vsh -c) with input NIL: and output to a T: file of this task; the
 * command line ends with `; echo @JOBRC $?`, so the job itself writes its return code into the file. A
 * timer.device tick (the handler/pty_handler.c timer_start/timer_stop pattern) wakes the caller; jb_poll reads the
 * bytes written since the last call into a job_core jc. One job at a time. Linked by amarchy-menu and amarchy-gui. */
#ifndef JOB_DOS_H
#define JOB_DOS_H

#include <exec/types.h>
#include "job_core.h"

/* Start cmd (a plain command line without double quotes, as for vsh -c) and reset j. prog (may be 0) is the
 * program whose "$STACK: n" cookie sets the child's stack (RULES.md rule 6, default floor 65536); title (may be 0)
 * is kept for jb_title. 1 started, 0 when a job is running, no vsh was found or it did not start. */
int jb_start(jc *j, const char *cmd, const char *prog, const char *title);
/* The signal mask of the timer tick: add it to the Wait(); 0 when no job runs. */
ULONG jb_sigmask(void);
/* Take the tick (re-arm it) and feed the new output into the jc given to jb_start. Safe to call at any time. */
void jb_poll(jc *j);
/* Ask the running job to stop (Ctrl-C to the child) and mark the jc cancelled. */
void jb_cancel(jc *j);
/* Stop the tick and delete the output file; the job should have ended or been cancelled. */
void jb_end(void);
/* The title given to jb_start, "" when none. */
const char *jb_title(void);

#endif
