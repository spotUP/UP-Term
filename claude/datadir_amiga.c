/* datadir_amiga -- see datadir_amiga.h. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "datadir_amiga.h"
#include "util.h"
extern struct DosLibrary *DOSBase;

void dda_err(dd_amiga *s, const char *what)
{
    char f[120];
    LONG e = IoErr();
    cl_copy(s->err, what, sizeof(s->err));
    if (e && Fault(e, 0, (STRPTR)f, sizeof(f))) {
        cl_cat(s->err, ": ", sizeof(s->err));
        cl_cat(s->err, f, sizeof(s->err));
    }
}

int dda_pread(void *u, const char *path, long off, char *out, long cap, long *n)
{
    dd_amiga *s = (dd_amiga *)u;
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    long got;
    *n = 0;
    if (!f) {
        dda_err(s, path);
        return -1;
    }
    if (off && Seek(f, off, OFFSET_BEGINNING) < 0) {
        dda_err(s, path);
        Close(f);
        return -1;
    }
    got = Read(f, out, cap);
    Close(f);
    if (got < 0) {
        dda_err(s, path);
        return -1;
    }
    *n = got;
    return 0;
}

int dda_read(void *u, const char *path, long max, char **out, long *n)
{
    dd_amiga *s = (dd_amiga *)u;
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    struct FileInfoBlock *fib;
    long len = -1;
    char *b;
    if (!f) {
        dda_err(s, path);
        return -1;
    }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && ExamineFH(f, fib))
        len = fib->fib_Size;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (len < 0) {
        dda_err(s, path);
        Close(f);
        return -1;
    }
    if (len > max) {
        Close(f);
        return SYS_TOO_BIG;
    }
    b = (char *)CL_ALLOC((size_t)len + 1);
    if (!b || Read(f, b, len) != len) {
        dda_err(s, b ? path : "out of memory");
        if (b)
            CL_FREE(b);
        Close(f);
        return -1;
    }
    Close(f);
    b[len] = 0;
    *out = b;
    *n = len;
    return 0;
}

int dda_write(void *u, const char *path, const char *str, long n)
{
    dd_amiga *s = (dd_amiga *)u;
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    LONG w;
    if (!f) {
        dda_err(s, path);
        return -1;
    }
    w = n ? Write(f, (APTR)str, n) : 0;
    if (w != n) {
        dda_err(s, path);
        Close(f);
        return -1;
    }
    return Close(f) ? 0 : -1;
}

/* a DateStamp in seconds since 1978 */
long dda_seconds(const struct DateStamp *d)
{
    return d->ds_Days * 86400L + d->ds_Minute * 60L + d->ds_Tick / TICKS_PER_SECOND;
}

int dda_list(void *u, const char *path, cl_dir_fn fn, void *c)
{
    dd_amiga *s = (dd_amiga *)u;
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    int rc = -1;
    if (l && fib && Examine(l, fib) && fib->fib_DirEntryType > 0) {
        rc = 0;
        while (ExNext(l, fib)) {
            cl_dirent e;
            memset(&e, 0, sizeof(e));
            cl_copy(e.name, (const char *)fib->fib_FileName, sizeof(e.name));
            e.dir = fib->fib_DirEntryType > 0;
            e.size = fib->fib_Size;
            e.mtime = dda_seconds(&fib->fib_Date);
            if (fn(c, &e))
                break;
        }
    } else
        dda_err(s, path);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (l)
        UnLock(l);
    return rc;
}

int dda_kind(void *u, const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    int k = 0;
    (void)u;
    if (!l)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(l, fib))
        k = fib->fib_DirEntryType > 0 ? 2 : 1;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(l);
    return k;
}

int dda_quiet_kind(void *u, const char *path)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR win = me->pr_WindowPtr;
    int k;
    me->pr_WindowPtr = (APTR)-1;
    k = dda_kind(u, path);
    me->pr_WindowPtr = win;
    return k;
}

int dda_mkdir(void *u, const char *path)
{
    dd_amiga *s = (dd_amiga *)u;
    BPTR l;
    if (dda_kind(u, path) == 2)
        return 0;
    l = CreateDir((STRPTR)path);
    if (!l) {
        dda_err(s, path);
        return -1;
    }
    UnLock(l);
    return 0;
}

int dda_remove(void *u, const char *path)
{
    dd_amiga *s = (dd_amiga *)u;
    if (!DeleteFile((STRPTR)path)) {
        dda_err(s, path);
        return -1;
    }
    return 0;
}

int dda_rename(void *u, const char *from, const char *to)
{
    dd_amiga *s = (dd_amiga *)u;
    if (!Rename((STRPTR)from, (STRPTR)to)) {
        dda_err(s, from);
        return -1;
    }
    return 0;
}


void dda_init(dd_amiga *s, cl_sys *sys)
{
    memset(s, 0, sizeof(*s));
    sys->u = s;
    sys->read = dda_read;
    sys->write = dda_write;
    sys->list = dda_list;
    sys->kind = dda_kind;
    sys->quiet_kind = dda_quiet_kind;
    sys->mkdir = dda_mkdir;
    sys->remove = dda_remove;
    sys->rename = dda_rename;
    sys->pread = dda_pread;
}
