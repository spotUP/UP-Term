/* ixnwait -- an ixemul program blocked in a native exec Wait() that a
 * kill(pid, SIGINT) from another ixemul program must end
 * (tools/rig/nwait_rig.py; RJ.7: curl in bsdsocket WaitSelect outlived
 * upkg's SIGINT, because ixemul's setrun signalled only its own waits).
 *
 * Catches SIGINT, writes its pid to RAM:ixnwait.pid, then waits on
 * SIGBREAKF_CTRL_C and a signal bit no one sends, as curl's
 * Curl_amiga_select does. Appends "woke <mask> ints <n>" to
 * RAM:ixnwait.out when the Wait returns. */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>

/* every Amiga program declares its stack (RULES.md) */
long __stack = 16384;
const char stackcookie[] = "$STACK:16384\n";

static volatile int ints;
static void on_int(int sig) { (void)sig; ints++; }

int main(void)
{
    struct sigaction sa;
    BYTE bit = AllocSignal(-1);
    ULONG got;
    FILE *f;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_int;
    sigaction(SIGINT, &sa, 0);
    f = fopen("/RAM/ixnwait.pid", "w");
    if (f) { fprintf(f, "%ld\n", (long)getpid()); fclose(f); }
    got = Wait(SIGBREAKF_CTRL_C | (bit >= 0 ? 1UL << bit : 0));
    f = fopen("/RAM/ixnwait.out", "a");
    if (f) { fprintf(f, "woke %08lx ints %d\n", (unsigned long)got, ints); fclose(f); }
    if (bit >= 0) FreeSignal(bit);
    return 0;
}
