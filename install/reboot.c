/* Reboot: write every volume's buffers out, then restart the Amiga.
 *
 * The Install's last page offers it (dist/Install.installer, P_END): the
 * handlers, XCON: and the assigns it set up start with the next boot. A
 * reset while a disk is being written leaves the volume to be validated,
 * or worse, so before ColdReboot() every mounted volume's handler gets
 * ACTION_FLUSH (FFS writes its dirty blocks out and answers when they are
 * on the disk), then two seconds pass for the drives to settle. The ports
 * are taken under the DosList lock and the packets sent after it is let
 * go (a handler may need the list while it flushes). OS 3.2 has a
 * C:Reboot of its own; the tools part installs this one only where there
 * is none (or one of ours), by the "(UP-Term)" in the version string.
 * `Reboot ?` (or -h, --help) prints what it does and reboots nothing.
 * Otherwise it does not return. */
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../tty/qmark.h"

static const char ver[] = "$VER: Reboot 1.0 (23.10.2026) (UP-Term)";

#define MAXVOL 32

int main(void)
{
    struct MsgPort *port[MAXVOL];
    struct DosList *dl;
    int n = 0, i, j;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)"Reboot: writes every volume's buffers out, then restarts the Amiga\n");
        return 0;
    }
    dl = LockDosList(LDF_VOLUMES | LDF_READ);
    while (n < MAXVOL && (dl = NextDosEntry(dl, LDF_VOLUMES))) {
        if (!dl->dol_Task)
            continue;
        for (j = 0; j < n && port[j] != dl->dol_Task; j++)
            ;
        if (j == n)
            port[n++] = dl->dol_Task;
    }
    UnLockDosList(LDF_VOLUMES | LDF_READ);
    for (i = 0; i < n; i++)
        DoPkt(port[i], ACTION_FLUSH, 0, 0, 0, 0, 0);
    Delay(2 * TICKS_PER_SECOND);
    ColdReboot();
    return (int)ver[0];
}
