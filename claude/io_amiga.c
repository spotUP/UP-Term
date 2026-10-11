/* see io_amiga.h; the functions as they were in claude/main_amiga.c */
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "io_amiga.h"
#include "util.h"

/* the Shell puts the program's argument line at the front of a redirected Input()
 * (Program <file: the first line read is "arg arg", not the file's): the first read
 * drops it when it is the argument line, so a redirected file is read from its start */
static int first_read = 1;

static long trimmed(const char *s, char *out, long cap)
{
    long n = (long)strlen(s);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, s, (size_t)n);
    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ' || out[n - 1] == '\t'))
        n--;
    out[n] = 0;
    return n;
}

static long c_read(void *u, char *buf, long cap)
{
    long n;
    (void)u;
    if (!FGets(Input(), (STRPTR)buf, cap))
        return -1;
    if (first_read) {
        char a[512], b[512];
        first_read = 0;
        if (!IsInteractive(Input()) && trimmed((const char *)GetArgStr(), a, sizeof(a)) &&
            trimmed(buf, b, sizeof(b)) && !strcmp(a, b) && !FGets(Input(), (STRPTR)buf, cap))
            return -1;
    }
    n = (long)strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = 0;
    return n;
}

static void c_write(void *u, const char *s, long n)
{
    (void)u;
    Write(Output(), (APTR)s, n);
}

static int c_brk(void *u)
{
    (void)u;
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}

static int c_sleep(void *u, long ms)
{
    long ticks = ms / 20;
    while (ticks > 0) {
        long t = ticks > 5 ? 5 : ticks;
        if (c_brk(u))
            return 1;
        Delay(t);
        ticks -= t;
    }
    return c_brk(u);
}

static unsigned long c_ms(void *u)
{
    struct DateStamp d;
    (void)u;
    DateStamp(&d);
    return (unsigned long)d.ds_Days * 86400000UL + (unsigned long)d.ds_Minute * 60000UL +
           (unsigned long)d.ds_Tick * 20UL;
}

/* ---- the screen of its own: raw keys, the window's size ---- */

static vt_termios saved;
static int saved_termios;       /* the console was in termios mode already (a PTY:) */
static int raw_on;

static struct FileHandle *console(BPTR h)
{
    struct FileHandle *fh;
    if (!h || !IsInteractive(h))
        return 0;
    fh = (struct FileHandle *)BADDR(h);
    return fh->fh_Type ? fh : 0;
}

/* raw mode through UP-Term's line discipline (the termios path vsh and
 * tmux take); another console refuses the packet and the line mode stays */
static int c_raw(void *u, int on)
{
    struct FileHandle *fh = console(Input());
    (void)u;
    if (!fh)
        return -1;
    if (on) {
        vt_termios t;
        if (!DoPkt(fh->fh_Type, ACTION_VTCON_TCGETA, fh->fh_Arg1, (LONG)&saved, 0, 0, 0))
            return -1;
        saved_termios = IoErr() == 1;
        t = saved;
        ld_make_raw(&t);
        if (!DoPkt(fh->fh_Type, ACTION_VTCON_TCSETA, fh->fh_Arg1, (LONG)&t, LD_TCSANOW, 0, 0))
            return -1;
        raw_on = 1;
        return 0;
    }
    if (raw_on) {
        if (saved_termios)
            DoPkt(fh->fh_Type, ACTION_VTCON_TCSETA, fh->fh_Arg1, (LONG)&saved, LD_TCSANOW, 0, 0);
        else
            SetMode(Input(), 0);    /* ends the termios mode: the window's own line editing again */
        raw_on = 0;
    }
    return 0;
}

static long c_rd(void *u, char *buf, long cap, long ms)
{
    BPTR in = Input();
    LONG n;
    (void)u;
    if (!WaitForChar(in, ms > 0 ? ms * 1000 : 0))
        return 0;
    n = Read(in, buf, cap);
    return n > 0 ? n : -1;
}

static int c_size(void *u, int *cols, int *rows)
{
    struct FileHandle *fh = console(Output());
    vt_winsize ws;
    (void)u;
    if (!fh)
        fh = console(Input());
    if (!fh)
        return -1;
    ws.ws_col = ws.ws_row = 0;
    if (!DoPkt(fh->fh_Type, ACTION_VTCON_GWINSZ, fh->fh_Arg1, (LONG)&ws, 0, 0, 0) || !ws.ws_col || !ws.ws_row)
        return -1;
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

/* Ctrl+G (A4 1.12): the prompt's file in the user's editor -- ENV:EDITOR
 * (vsh's), else Ed -- in this window, raw mode off meanwhile */
static int c_edit(void *u, const char *path)
{
    char ed[200], cmd[512];
    LONG rc;
    (void)u;
    if (GetVar((STRPTR)"EDITOR", (STRPTR)ed, sizeof(ed), 0) <= 0)
        cl_copy(ed, "Ed", sizeof(ed));
    cl_copy(cmd, ed, sizeof(cmd));
    cl_cat(cmd, " \"", sizeof(cmd));
    cl_cat(cmd, path, sizeof(cmd));
    cl_cat(cmd, "\"", sizeof(cmd));
    rc = SystemTags((STRPTR)cmd, SYS_Input, Input(), SYS_Output, Output(), SYS_UserShell, TRUE, TAG_END);
    return rc == -1 ? -1 : 0;
}

void io_amiga_line(cl_io *io)
{
    io->read_line = c_read;
    io->write = c_write;
    io->brk = c_brk;
    io->sleep = c_sleep;
    io->ms = c_ms;
}

int io_amiga_screen(cl_io *io)
{
    if (!console(Input()) || !console(Output()))
        return 0;
    io->read = c_rd;
    io->size = c_size;
    io->raw = c_raw;
    io->edit = c_edit;
    return 1;
}

void io_amiga_end(void)
{
    c_raw(0, 0);
}

int io_amiga_brk(void *u)
{
    return c_brk(u);
}
