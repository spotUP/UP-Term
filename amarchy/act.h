/* Amarchy actions (plan 2026-10-22-amarchy-commodity.md section 3). Amiga only. */
#ifndef AMARCHY_ACT_H
#define AMARCHY_ACT_H

/* run:<command line> and rexx:<script>: SystemTags asynchronously, input and
 * output NIL:, a 32 KB stack, the commodity's command path cloned so the child
 * finds C: and outlives us. 1 when started. */
int act_run(const char *cmd);
int act_rexx(const char *arg);
/* open:uterm: a new UP-Term window (the README's NewShell XCON: line). */
int act_open_uterm(void);

#endif
