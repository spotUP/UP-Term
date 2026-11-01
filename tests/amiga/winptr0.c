/* winptr0 <command line> -- runs the command line with a process whose pr_WindowPtr is 0, as a user's own
 * process has it (DOS opens its requesters on the default public screen). The rig's commands come from wasabi's
 * runner, whose pr_WindowPtr is -1 (wasabi wasabid.c:836) and every child inherits that, so a case that must
 * SEE a DOS requester (upkg_browse_extra_rig.py B5, opkg patch 0026) starts its window through this tool. */
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    const char *args = (const char *)GetArgStr();
    if (args[0] == '-' && args[1] == 'p') {
        /* `winptr0 -p`: print this process's own pr_WindowPtr and exit. Typed in a window started through winptr0
         * it prints 00000000 (inherited); from the rig's agent commands it prints ffffffff. */
        Printf((STRPTR)"pr_WindowPtr=%08lx\n", (LONG)me->pr_WindowPtr);
        return 0;
    }
    me->pr_WindowPtr = NULL;
    /* the raw argument string, quotes and all: the command line as typed */
    return (int)SystemTags((STRPTR)args, TAG_DONE);
}
