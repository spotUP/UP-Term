/* stackprobe -- prints the stack it was given (P8: vsh's stack rules).
 * Built twice: plain, and with STACK_COOKIE so its file carries
 * "$STACK: 50000" (vsh then gives it at least that), and with
 * STACK_COOKIE_BIG ("$STACK: 262144", stackprobe256k: ixemul's execve then
 * gives it at least that, tools/rig/stackext_rig.py). */
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>

#if defined(STACK_COOKIE_BIG)
#define STACK_COOKIE
const char stack_cookie[] = "$STACK: 262144";
#elif defined(STACK_COOKIE)
const char stack_cookie[] = "$STACK: 50000";
#endif

int main(void)
{
    struct Task *me = FindTask(0);
    struct CommandLineInterface *cli = Cli();
#ifdef STACK_COOKIE
    if (!stack_cookie[0])
        return 1; /* keeps the cookie linked in */
#endif
    Printf("stack %ld (cli %ld)\n", (LONG)((ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower),
           cli ? (LONG)cli->cli_DefaultStack * 4 : 0L);
    return 0;
}
