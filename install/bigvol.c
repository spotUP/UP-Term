/* UPBigVol: the local disk with the most free room, for the Install's drawer page.
 *
 * The Installer script cannot list volumes. Install (dist/Install.installer) runs this first; it sets the global
 * variable ENV:UPTBIGVOL to "<Volume>:UP-Term", or deletes the variable when no volume qualifies (the script then
 * offers SYS:UP-Term). The choice itself is install/bigvol.h (bv_choose, host-tested); the walk over the volumes
 * is install/hardvol.c, shared with amarchy-index, plus the device test: a network share's (smbfs) and RAM:'s
 * device node has no FileSysStartupMsg, so only volumes of a disk of this machine are looked at.
 * `UPBigVol LIST` also prints every volume it looked at and why it was or was not taken. `UPBigVol ?` prints what
 * it does. */
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "bigvol.h"
#include "hardvol.h"
#include "../tty/qmark.h"

static const char ver[] = "$VER: UPBigVol 1.0 (24.10.2026) (UP-Term)";

#define MAXVOL 16
#define VAR "UPTBIGVOL"

int main(void)
{
    struct bv_vol v[MAXVOL];
    struct DosList *head, *dl = 0;
    struct InfoData *id;
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    BPTR syslock;
    int n = 0, i, best, list;
    char out[48];
    const char *args = (const char *)GetArgStr();
    if (qmark_line(args)) {
        PutStr((STRPTR) "UPBigVol: sets ENV:" VAR " to <Volume>:UP-Term for the local disk with the most free room (UPBigVol LIST shows each volume)\n");
        return 0;
    }
    list = args && (args[0] == 'L' || args[0] == 'l');
    DeleteVar((STRPTR)VAR, GVF_GLOBAL_ONLY);
    id = (struct InfoData *)AllocMem(sizeof *id, MEMF_PUBLIC | MEMF_CLEAR);
    if (!id)
        return 20;
    me->pr_WindowPtr = (APTR)-1L; /* no "Please insert volume" requester for a disk that went away */
    syslock = Lock((STRPTR) "SYS:", ACCESS_READ);
    head = LockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
    while (n < MAXVOL && (dl = NextDosEntry(dl ? dl : head, LDF_VOLUMES))) {
        const unsigned char *nm = (const unsigned char *)BADDR(dl->dol_Name);
        int len = nm ? nm[0] : 0;
        if (len <= 0 || len >= (int)sizeof v[0].name || !hv_disk(dl, head, 1))
            continue;
        memcpy(v[n].name, nm + 1, len);
        v[n].name[len] = 0;
        v[n].dostype = dl->dol_misc.dol_volume.dol_DiskType;
        v[n].writable = v[n].is_sys = 0;
        v[n].free_kb = 0;
        n++;
    }
    UnLockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
    for (i = 0; i < n; i++) {
        char path[40];
        BPTR l;
        strcpy(path, v[i].name);
        strcat(path, ":");
        l = Lock((STRPTR)path, ACCESS_READ);
        if (!l)
            continue;
        if (Info(l, id)) {
            v[i].dostype = id->id_DiskType;
            v[i].writable = id->id_DiskState == ID_VALIDATED;
            v[i].free_kb = bv_free_kb((unsigned long)id->id_NumBlocks, (unsigned long)id->id_NumBlocksUsed, (unsigned long)id->id_BytesPerBlock);
        }
        v[i].is_sys = syslock && SameLock(l, syslock) == LOCK_SAME;
        UnLock(l);
    }
    if (syslock)
        UnLock(syslock);
    me->pr_WindowPtr = oldwin;
    FreeMem(id, sizeof *id);
    best = bv_choose(v, n);
    if (list)
        for (i = 0; i < n; i++)
            Printf((STRPTR) "%s%s: type %08lx %s %ld KB free%s\n", i == best ? "* " : "  ", v[i].name, v[i].dostype, v[i].writable ? "rw" : "ro", v[i].free_kb, v[i].is_sys ? " (SYS:)" : "");
    if (best >= 0) {
        strcpy(out, v[best].name);
        strcat(out, ":UP-Term");
        SetVar((STRPTR)VAR, (STRPTR)out, -1, GVF_GLOBAL_ONLY);
        if (list)
            Printf((STRPTR) "ENV:" VAR " = %s\n", out);
    } else if (list)
        PutStr((STRPTR) "no volume qualifies\n");
    return 0;
}
