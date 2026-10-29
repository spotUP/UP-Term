/* The Amiga clipboard (clipboard.device unit 0) as IFF FTXT: a CHRS chunk
 * in Latin-1, what ConClip, the Shell and editors exchange, and a UTF8
 * chunk for the readers that know it (the formats: clipfmt.c). Written and read
 * with plain device IO, following the RKM Devices clipboard chapter
 * (write the FORM sequentially from offset 0 and finish with CMD_UPDATE;
 * a read must go on until io_Actual is 0, or the clip stays locked). */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/clipboard.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "clip.h"
#include "clipfmt.h"

static struct IOClipReq *clip_open(struct MsgPort **port)
{
    struct IOClipReq *io;
    *port = CreateMsgPort();
    if (!*port)
        return 0;
    io = (struct IOClipReq *)CreateIORequest(*port, sizeof(struct IOClipReq));
    if (!io || OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)io, 0)) {
        if (io)
            DeleteIORequest((struct IORequest *)io);
        DeleteMsgPort(*port);
        return 0;
    }
    return io;
}

static void clip_close(struct IOClipReq *io, struct MsgPort *port)
{
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
}

static int put(struct IOClipReq *io, const void *data, ULONG len)
{
    io->io_Command = CMD_WRITE;
    io->io_Data = (STRPTR)data;
    io->io_Length = len;
    DoIO((struct IORequest *)io);
    return io->io_Error == 0 && io->io_Actual == len;
}

int clip_write(const char *utf8, long len)
{
    struct MsgPort *port;
    struct IOClipReq *io;
    unsigned char head[20], mid[12];
    char *lat = (char *)AllocVec((ULONG)len + 1, MEMF_ANY); /* Latin-1 is never longer */
    long nl;
    int ok, k;
    if (!lat)
        return 0;
    nl = cf_to_latin1(utf8, len, lat);
    io = clip_open(&port);
    if (!io) {
        FreeVec(lat);
        return 0;
    }
    io->io_Offset = 0;
    io->io_Error = 0;
    io->io_ClipID = 0;
    cf_ftxt_head(head, nl, len);
    k = cf_ftxt_mid(mid, nl, len);
    ok = put(io, head, 20) && put(io, lat, (ULONG)nl) && put(io, mid, (ULONG)k) &&
         put(io, utf8, (ULONG)len) && (!(len & 1) || put(io, "", 1));
    io->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *)io);
    clip_close(io, port);
    FreeVec(lat);
    return ok;
}

int clip_write_form(const char *path)
{
    struct MsgPort *port;
    struct IOClipReq *io;
    unsigned char head[12];
    static unsigned char buf[4096];
    struct FileInfoBlock *fib;
    BPTR fh;
    long total = -1, left, n;
    int ok = 0;
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && ExamineFH(fh, fib) && Read(fh, head, 12) == 12)
        total = cf_form_total(head, (long)fib->fib_Size);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (total < 0 || Seek(fh, 0, OFFSET_BEGINNING) < 0) {
        Close(fh);
        return 0;
    }
    io = clip_open(&port);
    if (!io) {
        Close(fh);
        return 0;
    }
    io->io_Offset = 0;
    io->io_Error = 0;
    io->io_ClipID = 0;
    ok = 1;
    for (left = total; ok && left > 0; left -= n) {
        n = Read(fh, buf, left < (long)sizeof buf ? left : (long)sizeof buf);
        ok = n > 0 && put(io, buf, (ULONG)n);
        if (n <= 0)
            break;
    }
    io->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *)io);
    clip_close(io, port);
    Close(fh);
    return ok && left <= 0;
}

static LONG get(struct IOClipReq *io, void *buf, ULONG len)
{
    io->io_Command = CMD_READ;
    io->io_Data = (STRPTR)buf;
    io->io_Length = len;
    DoIO((struct IORequest *)io);
    return io->io_Error ? -1 : (LONG)io->io_Actual;
}

static long reader(void *u, void *buf, long n)
{
    return (long)get((struct IOClipReq *)u, buf, (ULONG)n);
}

static void *alloc(unsigned long n)
{
    return AllocVec((ULONG)n, MEMF_ANY);
}

static void release(void *p)
{
    FreeVec(p);
}

char *clip_read(long *len)
{
    struct MsgPort *port;
    struct IOClipReq *io = clip_open(&port);
    char sink[64], *text;
    *len = 0;
    if (!io)
        return 0;
    io->io_Offset = 0;
    io->io_Error = 0;
    io->io_ClipID = 0;
    text = cf_read_ftxt(reader, io, alloc, release, len);
    /* read to the end, so the clipboard releases the clip */
    while (get(io, sink, sizeof(sink)) > 0)
        ;
    clip_close(io, port);
    return text;
}

/* The clipboard's whole content as raw IFF bytes (FORM ILBM from clip_write_form, FORM FTXT from clip_write); the
 * rig's reader for the capture proof. FreeVec the result; 0 when the clipboard is empty or unreachable. */
char *clip_read_raw(long *len)
{
    struct MsgPort *port;
    struct IOClipReq *io = clip_open(&port);
    char *buf = 0;
    long cap = 0, n = 0, k;
    *len = 0;
    if (!io)
        return 0;
    io->io_Offset = 0;
    io->io_Error = 0;
    io->io_ClipID = 0;
    for (;;) {
        if (n == cap) {
            char *nb = (char *)alloc(cap ? cap * 2 : 8192);
            if (!nb)
                break;
            if (buf) {
                memcpy(nb, buf, n);
                release(buf);
            }
            buf = nb;
            cap = cap ? cap * 2 : 8192;
        }
        k = get(io, buf + n, cap - n);
        if (k <= 0)
            break;
        n += k;
    }
    clip_close(io, port);
    *len = n;
    return buf;
}
