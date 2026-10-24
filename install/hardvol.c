/* install/hardvol.c: see hardvol.h. The walk was amarchy/index.c's hard_disk(). */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/dos.h>
#include "hardvol.h"

#define ID_CD01 0x43443031L
#define ID_CDFS 0x43444653L

static int ieqn(const char *a, const char *b, int n)
{
    for (; n > 0; n--, a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (y >= 'A' && y <= 'Z')
            y += 32;
        if (x != y)
            return 0;
        if (!x)
            break;
    }
    return 1;
}

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
        int named = 0;
        if (d->dol_Task != vol->dol_Task || !nm)
            continue;
        if (nm[0] == 3 && nm[1] == 'D' && nm[2] == 'F' && nm[3] >= '0' && nm[3] <= '9')
            return 0;
        if (nm[0] == 3 && (nm[1] == 'R' || nm[1] == 'r') && (nm[2] == 'A' || nm[2] == 'a') && (nm[3] == 'M' || nm[3] == 'm'))
            return 0;
        if (nm[0] == 3 && nm[1] == 'C' && nm[2] == 'D' && nm[3] >= '0' && nm[3] <= '9')
            return 0;
        {
            /* the startup message names the exec device; trackdisk is a floppy whatever the unit is called */
            ULONG st = (ULONG)d->dol_misc.dol_handler.dol_Startup;
            struct FileSysStartupMsg *fs = (struct FileSysStartupMsg *)BADDR(st);
            if (st > 0x400 && fs->fssm_Device) {
                const unsigned char *dn = (const unsigned char *)BADDR(fs->fssm_Device);
                named = dn[0] > 0;
                if (dn[0] >= 9 && ieqn((const char *)dn + 1, "trackdisk", 9))
                    return 0;
            }
        }
        if (need_device && !named)
            return 0;
        return 1;
    }
    return 0;
}
