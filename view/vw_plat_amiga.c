/* vw_plat for AmigaDOS: dos.library files and handles, the window's width
 * from UP-Term's ACTION_VTCON_GWINSZ (handler/vtcon_packets.h), from
 * ACTION_DISK_INFO's window on any other console, ^C from the task's
 * signals. No ixemul. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "vw_plat.h"

const int vw_fail_code = RETURN_ERROR;

struct vw_file {
    BPTR fh;
    int own;
};

static int failed;

vw_file *vw_open(const char *name)
{
    vw_file *f = (vw_file *)AllocVec(sizeof(vw_file), MEMF_ANY);
    if (!f)
        return 0;
    if (!name || !strcmp(name, "-")) {
        f->fh = Input();
        f->own = 0;
    } else {
        f->fh = Open((STRPTR)name, MODE_OLDFILE);
        f->own = 1;
    }
    if (!f->fh) {
        FreeVec(f);
        return 0;
    }
    return f;
}

long vw_read(vw_file *f, char *buf, long n)
{
    return Read(f->fh, buf, n);
}

void vw_close(vw_file *f)
{
    if (f->own)
        Close(f->fh);
    FreeVec(f);
}

void vw_write(void *u, const char *s, long n)
{
    (void)u;
    if (!failed && Write(Output(), (APTR)s, n) != n)
        failed = 1;
}

int vw_write_failed(void)
{
    return failed;
}

void vw_say(const char *s)
{
    BPTR out = Output();
    if (out)
        Write(out, (APTR)s, (LONG)strlen(s));
}

int vw_out_is_tty(void)
{
    BPTR out = Output();
    return out && IsInteractive(out);
}

/* the columns of the console window behind h, 0 when it is none */
static int columns_of(BPTR h)
{
    struct FileHandle *fh;
    vt_winsize ws;
    struct InfoData *id;
    int cols = 0;
    /* only a console: DISK_INFO to a file's handler names a volume */
    if (!h || !IsInteractive(h))
        return 0;
    fh = (struct FileHandle *)BADDR(h);
    if (!fh->fh_Type)
        return 0;
    ws.ws_col = 0;
    if (DoPkt(fh->fh_Type, ACTION_VTCON_GWINSZ, fh->fh_Arg1, (LONG)&ws, 0, 0, 0) && ws.ws_col > 0)
        return ws.ws_col;
    /* another console: its window, through the classic DISK_INFO */
    id = (struct InfoData *)AllocVec(sizeof(struct InfoData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!id)
        return 0;
    if (DoPkt(fh->fh_Type, ACTION_DISK_INFO, MKBADDR(id), 0, 0, 0, 0)) {
        struct Window *w = (struct Window *)id->id_VolumeNode;
        if (w && w->RPort && w->RPort->TxWidth > 0)
            cols = (w->Width - w->BorderLeft - w->BorderRight) / w->RPort->TxWidth;
    }
    FreeVec(id);
    return cols > 0 ? cols : 0;
}

/* standard output's window, else standard input's (mdv x | less: the
 * pipe has no width, the window the pager runs in has) */
int vw_columns(void)
{
    int c = columns_of(Output());
    return c ? c : columns_of(Input());
}

int vw_env(const char *name, char *buf, int n)
{
    return GetVar((STRPTR)name, (STRPTR)buf, n, 0) >= 0;
}

int vw_break(void)
{
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}
