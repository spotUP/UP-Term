/* autoprobe [DEV] -- H5.2 of the console.device plan: what a console
 * handler's AUTO/CLOSE window does on the close gadget, on the next write,
 * and around ACTION_DISK_INFO / ACTION_UNDISK_INFO (513; V47's con-handler
 * notes: "An ACTION_DISK_INFO disabled the auto-ability of the console
 * completely ... Now the console supports ACTION_UNDISK_INFO"). DEV is CON
 * (default) or XCON: the same steps against the ROM and against vtcon.
 * The probe prints "PHASE name" and waits for RAM:autoprobe.go to hold that
 * name; tools/rig/autoprobe_rig.py looks for the window from the host
 * (amiagent's window list), clicks its close gadget where a step says so,
 * and writes the file. Output also in RAM:autoprobe.log. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "probeout.h"

#ifndef ACTION_UNDISK_INFO
#define ACTION_UNDISK_INFO 513
#endif

static void phase(const char *name)
{
    char buf[40];
    int t;
    po_line("PHASE %s\n", name);
    for (t = 0; t < 60 * 10; t++) {
        BPTR f = Open((STRPTR)"RAM:autoprobe.go", MODE_OLDFILE);
        if (f) {
            LONG n = Read(f, buf, sizeof(buf) - 1);
            Close(f);
            buf[n > 0 ? n : 0] = 0;
            if (n > 0 && buf[n - 1] == '\n')
                buf[n - 1] = 0;
            if (!strcmp(buf, name))
                return;
        }
        Delay(5);
    }
    po_line("FAIL the rig never answered phase %s\n", name);
}

int main(int argc, char **argv)
{
    char spec[80];
    BPTR fh;
    struct MsgPort *port;
    static struct InfoData id;
    LONG r;
    sprintf(spec, "%s:0/20/300/100/autoprobe/AUTO/CLOSE", argc > 1 ? argv[1] : "CON");
    po_start("RAM:autoprobe.log");
    po_line("RESULT spec %s\n", spec);
    fh = Open((STRPTR)spec, MODE_NEWFILE);
    if (!po_check(fh != 0, "Open", 0))
        return po_end();
    port = ((struct FileHandle *)BADDR(fh))->fh_Type;
    phase("opened");                   /* rig: is there a window? (AUTO: no) */
    Write(fh, "one\n", 4);
    phase("wrote");                    /* rig: window? then click its close gadget */
    phase("closed");                   /* rig: window after the click? */
    Write(fh, "two\n", 4);
    phase("wrote-again");              /* rig: reopened? */
    memset(&id, 0, sizeof(id));
    r = DoPkt(port, ACTION_DISK_INFO, MKBADDR(&id), 0, 0, 0, 0);
    po_line("RESULT DISK_INFO res1 %ld window %08lx inuse %08lx\n", r, (ULONG)id.id_VolumeNode,
            (ULONG)id.id_InUse);
    phase("diskinfo");                 /* rig: click close */
    phase("diskinfo-closed");          /* rig: window after the click? */
    Write(fh, "three\n", 6);
    r = DoPkt(port, ACTION_UNDISK_INFO, 0, 0, 0, 0, 0);
    po_line("RESULT UNDISK_INFO res1 %ld res2 %ld\n", r, IoErr());
    phase("undisk");                   /* rig: click close */
    phase("undisk-closed");            /* rig: window after the click? */
    Write(fh, "four\n", 5);
    phase("wrote-last");               /* rig: reopened? */
    Close(fh);
    phase("closed-file");              /* rig: window after Close? */
    return po_end();
}
