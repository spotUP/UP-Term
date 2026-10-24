/* install/hardvol.h: is a volume on a disk of this machine. Shared by amarchy-index (which volumes to scan)
 * and UPBigVol (where the Install offers the drawer). */
#ifndef HARDVOL_H
#define HARDVOL_H
#include <dos/dosextens.h>

/* vol: a DosList volume entry; devs: the list head locked with LDF_DEVICES. 1 when a disk is in, and its handler
 * is not RAM, a CD file system or trackdisk. need_device: also 0 unless the handler's device node has a
 * FileSysStartupMsg that names an exec device (a network share's and RAM:'s have none). */
int hv_disk(struct DosList *vol, struct DosList *devs, int need_device);
#endif
