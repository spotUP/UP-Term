/* UPBigVol: the local disk with the most free room, for the Install's drawer page.
 *
 * The Installer script cannot list volumes. Install (dist/Install.installer) runs this first; it sets the global
 * variable ENV:UPTBIGVOL to "<Volume>:UP-Term", or deletes the variable when no volume qualifies (the script then
 * offers SYS:UP-Term). The choice itself is install/bigvol.h (bv_choose, host-tested); the walk over the volumes
 * is install/hardvol.c, shared with amarchy-index, plus the device test: a network share's (smbfs) and RAM:'s
 * device node has no FileSysStartupMsg, so only volumes of a disk of this machine are looked at.
 * `UPBigVol LIST` also prints every volume it looked at and why it was or was not taken. `UPBigVol ?` prints what
 * it does.
 * `UPBigVol ROOM <path> <KB>... [<path> <KB>...]` is the room check of a Shell install (dist/install.dos, no STEP):
 * each path's volume is locked, groups on one volume (SameLock) count together (bv_parse, bv_merge), and a volume
 * with less free room than its parts need prints bv_room_text ("<path> has N KB free;", newline, "the parts you chose need about M KB.") and
 * returns 5 (WARN); it also sets ENV:UPTROOM to that text and ENV:UPTROOMAT to the path, which the Installer
 * (dist/Install.installer) reads, and deletes both first on every run. A volume it cannot lock or Info, or words it cannot read, cannot tell: it returns 0 and the
 * install goes on, as the Installer's own check does when getdiskspace answers -1. */
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

/* vbcc's startup (vc.lib) runs main on a stack of ___stack bytes, whatever the Installer's run gives it; the
 * cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6). The volume list and Info need little. */
long __stack = 8192;
const char bigvol_stack_cookie[] = "$STACK: 8192";

#define MAXVOL 16
#define MAXGROUP 8
#define VAR "UPTBIGVOL"
/* ROOM's refusal text, and in ROOMVAR "AT" the path of the disk it names: the Installer reads both */
#define ROOMVAR "UPTROOM"

/* bv_merge's test: ctx is the groups' volume locks */
static int same_volume(int i, int j, void *ctx)
{
    BPTR *lk = (BPTR *)ctx;
    LONG r;
    if (!lk[i] || !lk[j])
        return 0;
    r = SameLock(lk[i], lk[j]);
    return r == LOCK_SAME || r == LOCK_SAME_VOLUME;
}

/* ROOM: 5 when a volume is too small for its parts, else 0 (room, or it cannot tell). */
static int room(char **words, int n, struct InfoData *id)
{
    struct bv_group g[MAXGROUP];
    BPTR glock[MAXGROUP];
    int ng = bv_parse(words, n, g, MAXGROUP), i, rc = 0;
    /* a refusal of an earlier run must not answer for this one */
    DeleteVar((STRPTR)ROOMVAR, GVF_GLOBAL_ONLY);
    DeleteVar((STRPTR)ROOMVAR "AT", GVF_GLOBAL_ONLY);
    if (ng < 0) {
        PutStr((STRPTR) "UPBigVol: ROOM takes <path> <KB>... groups; room not checked\n");
        return 0;
    }
    for (i = 0; i < ng; i++) {
        char vol[40];
        glock[i] = bv_volume(g[i].path, vol, sizeof vol) ? Lock((STRPTR)vol, ACCESS_READ) : 0;
    }
    bv_merge(g, ng, same_volume, glock);
    for (i = 0; i < ng && !rc; i++) {
        unsigned long free_kb;
        if (g[i].same_as >= 0 || !glock[i] || !Info(glock[i], id))
            continue;
        free_kb = bv_free_kb((unsigned long)id->id_NumBlocks, (unsigned long)id->id_NumBlocksUsed, (unsigned long)id->id_BytesPerBlock);
        if (bv_short(free_kb, g[i].kb)) {
            char text[160];
            bv_room_text(text, sizeof text, g[i].path, free_kb, g[i].kb);
            Printf((STRPTR) "%s\n", text);
            SetVar((STRPTR)ROOMVAR, (STRPTR)text, -1, GVF_GLOBAL_ONLY);
            SetVar((STRPTR)ROOMVAR "AT", (STRPTR)g[i].path, -1, GVF_GLOBAL_ONLY);
            rc = 5;
        }
    }
    for (i = 0; i < ng; i++)
        if (glock[i])
            UnLock(glock[i]);
    return rc;
}

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
    LONG opt[3] = {0, 0, 0};
    struct RDArgs *rd;
    if (qmark_line(args)) {
        PutStr((STRPTR) "UPBigVol: sets ENV:" VAR " to <Volume>:UP-Term for the local disk with the most free room (UPBigVol LIST shows each volume);\n"
                        "UPBigVol ROOM <path> <KB>... warns (5) when a disk has less free room than the KB after its paths\n");
        return 0;
    }
    rd = ReadArgs((STRPTR) "LIST/S,ROOM/S,WORDS/M", opt, 0);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR) "UPBigVol");
        return 20;
    }
    list = opt[0] != 0;
    id = (struct InfoData *)AllocMem(sizeof *id, MEMF_PUBLIC | MEMF_CLEAR);
    if (!id) {
        FreeArgs(rd);
        return 20;
    }
    me->pr_WindowPtr = (APTR)-1L; /* no "Please insert volume" requester for a disk that went away */
    if (opt[1]) {
        char **w = (char **)opt[2];
        int nw = 0, rc;
        while (w && w[nw])
            nw++;
        rc = room(w, nw, id);
        me->pr_WindowPtr = oldwin;
        FreeMem(id, sizeof *id);
        FreeArgs(rd);
        return rc;
    }
    FreeArgs(rd);
    DeleteVar((STRPTR)VAR, GVF_GLOBAL_ONLY);
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
            Printf((STRPTR) "%s%s: type %08lx %s %lu KB free%s\n", i == best ? "* " : "  ", v[i].name, v[i].dostype, v[i].writable ? "rw" : "ro", v[i].free_kb, v[i].is_sys ? " (SYS:)" : "");
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
