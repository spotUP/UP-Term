/* sz / rz -- ZMODEM over the shell's own stream (ledger T4 G3): in a serial
 * login (upgetty: serial.device <-> PTY: <-> vsh) the far end's terminal
 * program receives what `sz file` sends and sends what `rz` receives, the
 * way lrzsz works on Unix. No ixemul: plain AmigaDOS, small enough for a ROM.
 *
 *   sz FILE/M/A           send the files
 *   rz [OVERWRITE/S]      receive into the current directory
 *
 * Built twice from this file (Makefile: -DZM_SZ, -DZM_RZ). The protocol is
 * zm/zmodem.c's; this file is the line (Input()/Output() in raw mode,
 * WaitForChar for the timeouts) and the files. */
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "zmodem.h"

#ifdef ZM_SZ
static const char vers[] = "$VER: sz 1.0 (2.10.2026) UP-Term";
#else
static const char vers[] = "$VER: rz 1.0 (2.10.2026) UP-Term";
#endif

/* ---- the line: the shell's stream ---------------------------------------- */

typedef struct aline {
    BPTR in, out;
    UBYTE buf[1024];
    LONG len, pos;
} aline;

static int a_getc(void *u, int timeout_ms)
{
    aline *l = (aline *)u;
    if (l->pos < l->len)
        return l->buf[l->pos++];
    /* WaitForChar takes microseconds; 0 asks without waiting */
    if (!WaitForChar(l->in, (LONG)timeout_ms * 1000))
        return ZM_TIMEOUT;
    l->len = Read(l->in, l->buf, sizeof(l->buf));
    l->pos = 0;
    if (l->len <= 0) {
        l->len = 0;
        return ZM_LINE_ERROR;
    }
    return l->buf[l->pos++];
}

static int a_write(void *u, const unsigned char *b, long n)
{
    aline *l = (aline *)u;
    return Write(l->out, (APTR)b, n) == n ? 0 : 1;
}

/* ---- the files ---------------------------------------------------------------- */

/* Amiga dates count from 1978-01-01, Unix's from 1970-01-01: 2922 days */
#define EPOCH_DIFF 252460800UL

static void *f_open_read(void *u, const char *name, long *size, zm_u32 *mtime)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    BPTR f = Open((STRPTR)name, MODE_OLDFILE);
    (void)u;
    if (f && fib && ExamineFH(f, fib)) {
        *size = fib->fib_Size;
        *mtime = (zm_u32)fib->fib_Date.ds_Days * 86400UL + (zm_u32)fib->fib_Date.ds_Minute * 60UL +
                 (zm_u32)fib->fib_Date.ds_Tick / 50UL + EPOCH_DIFF;
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    return f ? (void *)f : 0;
}

static long f_read(void *u, void *f, unsigned char *b, long n)
{
    (void)u;
    return Read((BPTR)f, b, n);
}

static int f_seek(void *u, void *f, long pos)
{
    (void)u;
    return Seek((BPTR)f, pos, OFFSET_BEGINNING) < 0 && IoErr() != 0;
}

static int overwrite;

/* the sender's name, its own part only: a path from the far end must not
 * reach outside the current directory (no "/", no "Volume:") */
static void *f_open_write(void *u, const char *name, long size, zm_u32 mtime)
{
    BPTR lock;
    (void)u;
    (void)size;
    (void)mtime;
    if (!*name || strchr(name, '/') || strchr(name, ':'))
        return 0;
    if (!overwrite && (lock = Lock((STRPTR)name, SHARED_LOCK)) != 0) {
        UnLock(lock);
        return 0; /* there already: skipped (rz OVERWRITE replaces it) */
    }
    return (void *)Open((STRPTR)name, MODE_NEWFILE);
}

static long f_write(void *u, void *f, const unsigned char *b, long n)
{
    (void)u;
    return Write((BPTR)f, (APTR)b, n);
}

static void f_close(void *u, void *f, int complete)
{
    (void)u;
    (void)complete;
    Close((BPTR)f);
}

static const zm_files files = { f_open_read, f_read, f_seek, f_open_write, f_write, f_close, 0 };

static const char *const why[] = {
    "", "the other side stopped answering", "cancelled", "the line went away",
    "too many errors", "a file could not be read or written"
};

int main(void)
{
#ifdef ZM_SZ
    static const char tmpl[] = "FILES/M/A";
#else
    static const char tmpl[] = "OVERWRITE/S";
#endif
    LONG args[1] = { 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)tmpl, args, 0);
    static aline al;
    zm_line line;
    int rc, n = 0;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)(vers[0] ? (vers + 6) : ""));
        return RETURN_FAIL;
    }
    al.in = Input();
    al.out = Output();
    line.getc = a_getc;
    line.write = a_write;
    line.user = &al;
    SetMode(al.in, 1); /* every byte as it is, both ways (PTY: makes it cfmakeraw) */
#ifdef ZM_SZ
    {
        const char *const *names = (const char *const *)args[0];
        while (names[n])
            n++;
        rc = zm_send(&line, &files, names, n);
    }
#else
    overwrite = args[0] != 0;
    rc = zm_receive(&line, &files, &n);
#endif
    SetMode(al.in, 0);
    FreeArgs(rd);
    if (rc != ZM_OK) {
        Printf("\r\n%s: %s\r\n", (STRPTR)(vers + 6), (STRPTR)why[rc]);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}
