/* install/hardvol.c: see hardvol.h. The walk was amarchy/index.c's hard_disk(). */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stddef.h>
#include "fssm.h"
#include "hardvol.h"

#define ID_CD01 0x43443031L
#define ID_CDFS 0x43444653L

typedef char fssm_layout[(offsetof(struct FileSysStartupMsg, fssm_Device) == FSSM_DEVICE_OFF &&
                          offsetof(struct FileSysStartupMsg, fssm_Environ) == FSSM_ENVIRON_OFF) ? 1 : -1];

/* real memory: RAM exec knows, or the Kickstart ROM (a startup message built by the ROM may name its device there) */
static int mem_ok(unsigned long a, unsigned long len)
{
    unsigned long e = a + len - 1;
    if (!len || e < a)
        return 0;
    if (a >= 0xF80000UL && e <= 0xFFFFFFUL)
        return 1;
    return TypeOfMem((APTR)a) != 0 && TypeOfMem((APTR)e) != 0;
}

static unsigned long mem_rl(unsigned long a)
{
    return *(const ULONG *)a;
}

static unsigned char mem_rb(unsigned long a)
{
    return *(const UBYTE *)a;
}

static const struct fssm_mem amiga_mem = { mem_ok, mem_rl, mem_rb };

int hv_disk(struct DosList *vol, struct DosList *devs, int need_device)
{
    struct DosList *d;
    if (!vol->dol_Task)
        return 0;
    if (vol->dol_misc.dol_volume.dol_DiskType == ID_CD01 || vol->dol_misc.dol_volume.dol_DiskType == ID_CDFS || vol->dol_misc.dol_volume.dol_DiskType == (ULONG)ID_NO_DISK_PRESENT ||
        vol->dol_misc.dol_volume.dol_DiskType == ID_UNREADABLE_DISK)
        return 0;
    for (d = NextDosEntry(devs, LDF_DEVICES); d; d = NextDosEntry(d, LDF_DEVICES)) {
        const unsigned char *nm = (const unsigned char *)BADDR(d->dol_Name);
        int kind;
        if (d->dol_Task != vol->dol_Task || !nm)
            continue;
        if (nm[0] == 3 && nm[1] == 'D' && nm[2] == 'F' && nm[3] >= '0' && nm[3] <= '9')
            return 0;
        if (nm[0] == 3 && (nm[1] == 'R' || nm[1] == 'r') && (nm[2] == 'A' || nm[2] == 'a') && (nm[3] == 'M' || nm[3] == 'm'))
            return 0;
        if (nm[0] == 3 && nm[1] == 'C' && nm[2] == 'D' && nm[3] >= '0' && nm[3] <= '9')
            return 0;
        /* the startup message names the exec device; trackdisk is a floppy whatever the unit is called */
        kind = fssm_kind(&amiga_mem, (ULONG)d->dol_misc.dol_handler.dol_Startup);
        if (kind == FSSM_FLOPPY)
            return 0;
        if (need_device && kind != FSSM_DEVICE)
            return 0;
        return 1;
    }
    return 0;
}
