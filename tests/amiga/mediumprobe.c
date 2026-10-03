/* mediumprobe -- DP5 of the console.device plan
 * (thoughts/shared/plans/2026-09-30-console-device.md): the bytes the V47
 * con-handler's medium mode (SetMode(fh, 2)) hands a reader. The probe opens
 * its own CON: window, sets mode 2, prints READY and then hex-dumps every
 * Read() while the rig (tools/rig/mediumprobe_rig.py) types "ab", TAB,
 * Shift+TAB, Up, Down, "hello" and RETURN, then "q" and RETURN. A read is
 * one line: "READ n: hex... | text". The probe ends on a read holding "q",
 * or after 60 s with no input. NOWAIT: plain blocking Read() instead of
 * WaitForChar first; REPLY: answer each CSI report by rewriting the input
 * line (as the V47 Shell must); SECOND: write those replies through a
 * second handle on the window; BEL: the reply is one BEL, as the V47
 * Shell's on no match; OWN: the window is the process's console task
 * (pr_ConsoleTask) for the whole run. Closes matrix Q9 (research section 6).
 * Output also in RAM:mediumprobe.log. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "probeout.h"

extern struct DosLibrary *DOSBase;

int main(int argc, char **argv)
{
    int i2, nowait = 0, reply = 0, second = 0, bel = 0, own = 0, force = 0;
    APTR oldcons = 0;
    BPTR out = 0;
    BPTR fh;
    char buf[64], hex[3 * 64 + 1], text[64 + 1];
    LONG n, i, idle = 0;
    for (i2 = 1; i2 < argc; i2++) {
        nowait |= !strcmp(argv[i2], "NOWAIT");
        reply |= !strcmp(argv[i2], "REPLY");
        force |= !strcmp(argv[i2], "FORCE");
        reply |= force;
        second |= !strcmp(argv[i2], "SECOND");
        bel |= !strcmp(argv[i2], "BEL");
        own |= !strcmp(argv[i2], "OWN");
    }
    po_start("RAM:mediumprobe.log");
    po_line("RESULT options nowait %d reply %d\n", nowait, reply);
    po_line("RESULT dos.library %d.%d\n", DOSBase->dl_lib.lib_Version, DOSBase->dl_lib.lib_Revision);
    fh = Open((STRPTR)"CON:0/20/500/150/mediumprobe/CLOSE", MODE_NEWFILE);
    if (!po_check(fh != 0, "CON: window", 0))
        return po_end();
    /* OWN: the window is this process's console for the whole run, as a
     * Shell's window is the Shell's */
    if (own) {
        struct Process *me = (struct Process *)FindTask(0);
        oldcons = me->pr_ConsoleTask;
        me->pr_ConsoleTask = ((struct FileHandle *)BADDR(fh))->fh_Type;
    }
    po_line("RESULT SetMode(fh, 2) returned %ld\n", SetMode(fh, 2));
    /* SECOND: replies go through a second handle on the same window, as a
     * Shell's Output() is separate from its Input() */
    if (second) {
        struct Process *me = (struct Process *)FindTask(0);
        APTR old = me->pr_ConsoleTask;
        me->pr_ConsoleTask = ((struct FileHandle *)BADDR(fh))->fh_Type;
        out = Open((STRPTR)"*", MODE_NEWFILE);
        me->pr_ConsoleTask = old;
        po_check(out != 0, "second handle on the window", 0);
    }
    Write(fh, "medium mode: type\n", 18);
    po_line("READY\n");
    while (idle < 60) {
        if (!nowait && !WaitForChar(fh, 1000000)) {
            idle++;
            continue;
        }
        idle = 0;
        n = Read(fh, buf, sizeof(buf));
        if (n <= 0) {
            po_line("READ %ld\n", n);
            break;
        }
        for (i = 0; i < n; i++) {
            sprintf(hex + 3 * i, "%02x ", (unsigned char)buf[i]);
            text[i] = (buf[i] >= 32 && buf[i] < 127) ? buf[i] : '.';
        }
        text[n] = 0;
        po_line("READ %ld: %s| %s\n", n, hex, text);
        if (memchr(buf, 'q', (size_t)n))
            break;
        /* REPLY: answer a report as a shell would, by rewriting the
         * input line (CR, erase to end of line, the text after the report) */
        if (reply && (unsigned char)buf[0] == 0x9b) {
            char *e = memchr(buf, 'U', (size_t)n);
            BPTR w = out ? out : fh;
            if (force) {
                /* as the V47 Shell does (measured on 3.2 through XCON:'s
                 * packet log): ACTION_FORCE (2001), Arg2 0x02 + the line,
                 * Arg3 its length -- 0x02 replaces the line being edited */
                static char fb[256];
                struct FileHandle *h = (struct FileHandle *)BADDR(fh);
                LONG len = e ? (LONG)(buf + n - e - 1) : 0;
                if (len > 250)
                    len = 250;
                fb[0] = 0x02;
                if (len > 0)
                    memcpy(fb + 1, e + 1, (size_t)len);
                po_line("FORCE %ld\n", DoPkt(h->fh_Type, 2001, h->fh_Arg1, (LONG)fb, len + 1, 0, 0));
            } else if (bel) {
                Write(w, "\x07", 1);    /* what the V47 Shell answers on no match (medshell) */
            } else {
                Write(w, "\r\x9bK", 3);
                if (e && e + 1 < buf + n)
                    Write(w, e + 1, (LONG)(buf + n - e - 1));
            }
            po_line("REPLIED\n");
        }
    }
    po_check(idle < 60, "the rig's typing reached the window", "no input for 60 s");
    if (out)
        Close(out);
    if (own)
        ((struct Process *)FindTask(0))->pr_ConsoleTask = oldcons;
    SetMode(fh, 0);
    Close(fh);
    return po_end();
}
