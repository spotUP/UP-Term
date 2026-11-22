/* gthemes_scan_dos: see gthemes_scan_dos.h. */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "gthemes_scan_dos.h"

static void gt_dir(void *ctx, const char *dir, gt_emit_fn emit, void *ectx)
{
    BPTR lock = Lock((STRPTR)dir, ACCESS_READ);
    struct FileInfoBlock *fib;
    (void)ctx;
    if (!lock)
        return;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib) {
        if (Examine(lock, fib))
            while (ExNext(lock, fib))
                if (fib->fib_DirEntryType < 0)      /* a file, not a drawer */
                    emit(ectx, (const char *)fib->fib_FileName);
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
}

void gt_dos_scan(gt_list *l)
{
    gt_scan(gt_dir, 0, l);
}
