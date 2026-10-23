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
    ((struct Process *)FindTask(NULL))->pr_WindowPtr = NULL;
    /* the raw argument string, quotes and all: the command line as typed */
    return (int)SystemTags((STRPTR)GetArgStr(), TAG_DONE);
}
