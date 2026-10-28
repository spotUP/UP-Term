/* WBPath ADD|REMOVE <drawer>: edit the Workbench process's command path live.
 *
 * "Path X ADD" changes only the Shell it runs in. Every Shell vsh starts from Workbench (UP-Term windows)
 * and every Amarchy launch copies the Workbench process's cli_CommandDir, so a drawer an installer adds
 * with Path alone is missing there until the next boot (UHC:ExtBin after up-uhc: "Pipe" unknown). WBPath
 * puts the drawer on that path itself (dist/addons/uhc/install.dos) and takes it off again before the
 * drawer is deleted (uninstall-body.dos: the node holds a lock that blocks the delete).
 *
 *   ADD     lock the drawer; when a node already holds the same object (SameLock), do nothing; else append
 *   REMOVE  unlink and free every node that is the same object as the drawer
 *
 * Shells already open keep their own path (they copied it): run "Path <drawer> ADD" there once.
 * Return codes: 0 done (or nothing to do), 5 no Workbench process ("[WARN]" line; an install still
 * succeeds), 10 the drawer cannot be locked (ADD), 20 usage or no memory.
 * The node layout and the loops are config/cmdpath.h's, shared with vsh and Amarchy. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#define CMDPATH_DOS
#include "../config/cmdpath.h"

/* vbcc's startup runs main on a stack of this size whatever the starter gives; the cookie is what
 * AmigaOS 3.2 and launchers read from the file (config/segstack.h) */
long __stack = 8192;
const char wbpath_stack_cookie[] = "$STACK: 8192";

/* ASCII case-insensitive equality (no utility.library to open for it) */
static int same_word(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if ((*a | 0x20) != (*b | 0x20))
            return 0;
    return !*a && !*b;
}

static int usage(void)
{
    Printf("usage: WBPath ADD|REMOVE <drawer>\n");
    return 20;
}

int main(int argc, char **argv)
{
    int add, rc = 0;
    BPTR lock, *field, *tail;
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin;
    if (argc != 3)
        return usage();
    if (same_word(argv[1], "ADD"))
        add = 1;
    else if (same_word(argv[1], "REMOVE"))
        add = 0;
    else
        return usage();
    oldwin = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;            /* no requester when the volume is gone */
    lock = Lock((STRPTR)argv[2], SHARED_LOCK);
    me->pr_WindowPtr = oldwin;
    if (!lock) {
        if (add) {
            Printf("[ERROR] WBPath: cannot lock %s\n", argv[2]);
            return 10;
        }
        Printf("[INFO] WBPath: %s does not exist; nothing to remove\n", argv[2]);
        return 0;
    }
    Forbid();
    field = cmdpath_field(cmdpath_workbench());
    if (!field) {
        Permit();
        UnLock(lock);
        Printf("[WARN] WBPath: no Workbench process; its path was not changed\n");
        return 5;
    }
    if (add) {
        if (cmdpath_has(*field, lock)) {
            Permit();
            UnLock(lock);
            Printf("[INFO] WBPath: %s is already on the Workbench path\n", argv[2]);
            return 0;
        }
        tail = cmdpath_tail(field);
        if (!cmdpath_append(&tail, lock))
            rc = 20;
        Permit();
        if (rc) {
            UnLock(lock);
            Printf("[ERROR] WBPath: out of memory\n");
        } else
            Printf("[OK] WBPath: %s added to the Workbench path\n", argv[2]);
        return rc;
    }
    {
        int n = cmdpath_remove(field, lock);
        Permit();
        UnLock(lock);
        Printf("[OK] WBPath: %ld node(s) for %s removed from the Workbench path\n", (LONG)n, argv[2]);
    }
    return 0;
}
