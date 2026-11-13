/* nobreak.c -- a Break (Ctrl-C) during file work must not end the program behind its own cleanup.
 *
 * The vbcc C library (vc.lib, unit _chkabort.c) is called by every buffered stdio read and
 * write (_fillbuf, _flushbuf, _putbuf). It does SetSignal(0, SIGBREAKF_CTRL_C) and, when a
 * Ctrl-C was pending, calls exit(20) from inside stdio. exit() never reaches the program's
 * own exit path, so the patches, semaphores, ports and commodity broker it holds stay armed
 * behind an unloaded segment, and the next start crashes (TE-APPLY-DESKTOP-FREEZE-32,
 * thoughts/shared/research/s61-evidence/restart-crash-chkabort.md).
 *
 * Linking this empty version in place of the library unit leaves the signal pending, so the
 * program's own main Wait takes SIGBREAKF_CTRL_C and runs its normal exit path. Link it only
 * into programs that handle Ctrl-C themselves AND hold system state (CHKABORT_PROGRAMS in the
 * Makefile; tests/test_nobreak.py fails when a built one still carries the library code).
 */

void _chkabort(void);

void _chkabort(void)
{
}
