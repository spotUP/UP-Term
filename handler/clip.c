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
