/* launchprobe -- what `upkg browse` Enter launches in tools/rig/upkg_browse_launch_rig.py.
 * Writes T:launch-sentinel ("token <argv[1]> stack <bytes>") and exits with code 7. Carries a
 * "$STACK: 262144" cookie: the rig asserts the stack it reports is at least that, under the
 * default Stack of the console that started upkg. */
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>

const char stack_cookie[] = "$STACK: 262144";

int main(int argc, char **argv)
{
    struct Task *me = FindTask(0);
    LONG stack = (LONG)((ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower);
    BPTR f;

    if (!stack_cookie[0])
        return 1; /* keeps the cookie linked in */
    f = Open("T:launch-sentinel", MODE_NEWFILE);
    if (!f)
        return 9;
    FPrintf(f, "token %s stack %ld\n", argc > 1 ? (LONG)argv[1] : (LONG)"none", stack);
    Close(f);
    return 7;
}
