/* The Amiga clipboard (clipboard.device unit 0) as IFF FTXT with one CHRS
 * chunk: what ConClip, the Shell and editors exchange. Written and read
 * with plain device IO, following the RKM Devices clipboard chapter
 * (write the FORM sequentially from offset 0 and finish with CMD_UPDATE;
 * a read must go on until io_Actual is 0, or the clip stays locked). */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/clipboard.h>
#include <proto/exec.h>
#include "clip.h"

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

static int put_long(struct IOClipReq *io, ULONG v)
{
    return put(io, &v, 4); /* the 68k is big-endian, as IFF is */
}

int clip_write(const char *text, long len)
{
    struct MsgPort *port;
    struct IOClipReq *io = clip_open(&port);
    ULONG pad = len & 1;
    int ok;
    if (!io)
        return 0;
    io->io_Offset = 0;
    io->io_Error = 0;
    io->io_ClipID = 0;
    ok = put(io, "FORM", 4) && put_long(io, 4 + 8 + len + pad) && put(io, "FTXT", 4) &&
         put(io, "CHRS", 4) && put_long(io, (ULONG)len) && put(io, text, (ULONG)len) &&
         (!pad || put(io, "", 1));
    io->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *)io);
    clip_close(io, port);
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

long clip_read(char *out, long max)
{
    struct MsgPort *port;
    struct IOClipReq *io = clip_open(&port);
    ULONG hdr[3], ck[2];
    long n = 0;
    char sink[64];
    if (!io)
        return 0;
    io->io_Offset = 0;
    io->io_Error = 0;
    io->io_ClipID = 0;
    if (get(io, hdr, 12) == 12 && hdr[0] == 0x464F524DUL /* FORM */ && hdr[2] == 0x46545854UL) {
        ULONG left = hdr[1] - 4;
        while (left >= 8 && get(io, ck, 8) == 8) {
            ULONG size = ck[1], skip = size + (size & 1);
            left -= 8;
            if (ck[0] == 0x43485253UL /* CHRS */ && !n) {
                ULONG take = size < (ULONG)(max - 1) ? size : (ULONG)(max - 1);
                if (get(io, out, take) != (LONG)take)
                    break;
                n = (long)take;
                skip -= take;
            }
            while (skip) {
                ULONG k = skip < sizeof(sink) ? skip : sizeof(sink);
                if (get(io, sink, k) <= 0)
                    break;
                skip -= k;
            }
            left = left > size + (size & 1) ? left - size - (size & 1) : 0;
        }
    }
    /* read to the end, so the clipboard releases the clip */
    while (get(io, sink, sizeof(sink)) > 0)
        ;
    clip_close(io, port);
    out[n] = 0;
    return n;
}
