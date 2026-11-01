/* Reboot: write every volume's buffers out, then restart the Amiga.
 *
 * The Install's last page offers it (dist/Install.installer, P_END): the
 * handlers, XCON: and the assigns it set up start with the next boot. A
 * reset while a disk is being written leaves the volume to be validated,
 * or worse, so before ColdReboot() every mounted volume's handler gets
 * ACTION_FLUSH (FFS writes its dirty blocks out and answers when they are
 * on the disk), then two seconds pass for the drives to settle. The ports
 * are taken under the DosList lock and the packets sent after it is let
 * go (a handler may need the list while it flushes).
 *
 * The packets go out all at once (SendPkt) and the answers are awaited
 * for at most FLUSH_TICKS. A handler that never answers ACTION_FLUSH (a
 * network or FAT handler may not) must not stop the reboot: the first
 * version waited on each with DoPkt and hung there for good. A volume
 * that has not answered in time is named, and the reboot goes ahead.
 *
 * `Reboot DRYRUN` sends the same packets, names each volume with its
 * answer (or none) and returns without rebooting: 0 when all answered,
 * 5 when one did not. Its packets still out stay allocated and its reply
 * port is left behind with PA_IGNORE, so a late answer lands in memory
 * that is still there instead of signalling a task that is gone.
 *
 * OS 3.2 has a C:Reboot of its own; the tools part installs this one
 * only where there is none (or one of ours), by the "(UP-Term)" in the
 * version string. `Reboot ?` (or -h, --help) prints what it does and
 * reboots nothing. Otherwise, without DRYRUN, it does not return. */
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../tty/qmark.h"

static const char ver[] = "$VER: Reboot 1.1 (27.10.2026) (UP-Term)";

/* vbcc's startup runs main on a stack of this size whatever the starter gives
 * it; the cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6). */
long __stack = 8192;
const char reboot_stack_cookie[] = "$STACK: 8192";

#define MAXVOL 32
#define FLUSH_TICKS (3 * TICKS_PER_SECOND)

struct vol {
    struct MsgPort *port;
    struct DosPacket *pkt;
    char name[32];
    int answered;
};

static struct vol vols[MAXVOL];

/* The volume's name from its BSTR, cut to fit. */
static void vol_name(struct DosList *dl, char *out)
{
    UBYTE *b = (UBYTE *)BADDR(dl->dol_Name);
    int len = b ? b[0] : 0, k;
    if (len > 30)
        len = 30;
    for (k = 0; k < len; k++)
        out[k] = b[k + 1];
    out[len] = 0;
}

int main(void)
{
    static const char tmpl[] = "DRYRUN/S";
    LONG args[1] = { 0 };
    struct RDArgs *rd;
    struct MsgPort *reply;
    struct DosList *dl;
    struct Message *msg;
    int n = 0, pending = 0, i, j, t;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)"Reboot: writes every volume's buffers out, then restarts the Amiga\n"
                       "Reboot DRYRUN: writes them out and names each volume's answer, no restart\n");
        return 0;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"Reboot");
        return 20;
    }
    reply = CreateMsgPort();
    dl = LockDosList(LDF_VOLUMES | LDF_READ);
    while (n < MAXVOL && (dl = NextDosEntry(dl, LDF_VOLUMES))) {
        if (!dl->dol_Task)
            continue;
        for (j = 0; j < n && vols[j].port != dl->dol_Task; j++)
            ;
        if (j == n) {
            vols[n].port = dl->dol_Task;
            vol_name(dl, vols[n].name);
            n++;
        }
    }
    UnLockDosList(LDF_VOLUMES | LDF_READ);
    for (i = 0; reply && i < n; i++) {
        vols[i].pkt = (struct DosPacket *)AllocDosObject(DOS_STDPKT, 0);
        if (!vols[i].pkt)
            continue;
        vols[i].pkt->dp_Type = ACTION_FLUSH;
        SendPkt(vols[i].pkt, vols[i].port, reply);
        pending++;
    }
    for (t = 0; pending; t++) {
        while (pending && (msg = GetMsg(reply))) {
            for (i = 0; i < n; i++)
                if (vols[i].pkt && msg == vols[i].pkt->dp_Link) {
                    vols[i].answered = 1;
                    pending--;
                }
        }
        if (!pending || t >= FLUSH_TICKS)
            break;
        Delay(1);
    }
    for (i = 0; i < n; i++) {
        if (vols[i].answered && !args[0])
            continue;
        Printf((STRPTR)"Reboot: %s: %s\n", (LONG)vols[i].name,
               (LONG)(vols[i].answered ? "flushed"
                      : vols[i].pkt ? "no answer in 3 s" : "no memory for the packet"));
    }
    if (args[0]) {
        if (pending) {
            reply->mp_Flags = PA_IGNORE;
        } else {
            for (i = 0; i < n; i++)
                if (vols[i].pkt)
                    FreeDosObject(DOS_STDPKT, vols[i].pkt);
            DeleteMsgPort(reply);
        }
        FreeArgs(rd);
        return pending ? 5 : 0;
    }
    Delay(2 * TICKS_PER_SECOND);
    ColdReboot();
    return (int)ver[0];
}
