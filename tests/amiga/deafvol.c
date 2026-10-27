/* Stand-in for a handler that never answers a packet (a hung network or FAT handler): adds a
 * volume DEAF: whose dol_Task is a message port nobody serves. Reboot must name it and go on.
 * Ctrl-C removes the entry and exits. */
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

long __stack = 8192;
const char stackcookie[] = "$STACK:8192\n";

int main(void)
{
    struct MsgPort *port = CreateMsgPort();
    struct DosList *dl;
    if (!port)
        return 20;
    dl = MakeDosEntry((STRPTR)"DEAF", DLT_VOLUME);
    if (!dl) {
        DeleteMsgPort(port);
        return 20;
    }
    dl->dol_Task = port;
    LockDosList(LDF_VOLUMES | LDF_WRITE);
    AddDosEntry(dl);
    UnLockDosList(LDF_VOLUMES | LDF_WRITE);
    PutStr((STRPTR)"deaf volume up\n");
    Flush(Output());
    Wait(SIGBREAKF_CTRL_C);
    LockDosList(LDF_VOLUMES | LDF_WRITE);
    RemDosEntry(dl);
    UnLockDosList(LDF_VOLUMES | LDF_WRITE);
    FreeDosEntry(dl);
    DeleteMsgPort(port);
    return 0;
}
