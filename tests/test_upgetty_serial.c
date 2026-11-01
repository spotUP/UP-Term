/* upgetty's serial.device request (device/upgetty_serial.h), on the host
 * against the NDK's structs: `make test ONLY=upgetty`.
 *
 * Regression: zmodem_rig check 4 (host lsz -> VTC:rz through upgetty) ended
 * rc 128 with RAM:zm-back.bin 0 bytes. upgetty left io_RBufLen at
 * serial.device's 512-byte default on a line with no flow control; a
 * 1024-byte ZMODEM subpacket overflowed it between two pump turns
 * (SerErr_BufOverflow, the buffer's bytes lost), rz asked ZRPOS(0) again and
 * again. The model below is serial.device's receive buffer: bytes past
 * io_RBufLen are dropped. */
#include <stdio.h>
#include <string.h>
#include "../device/upgetty_serial.h"

static int fails, checks;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, what); } } while (0)

/* bytes of a burst serial.device keeps when the pump does not read during it */
static unsigned long kept(const struct IOExtSer *io, unsigned long burst)
{
    return burst < io->io_RBufLen ? burst : io->io_RBufLen;
}

int main(void)
{
    struct IOExtSer io;
    /* what lsz streams before rz's next read can be served: ZDATA header and
     * subpackets of 1024 bytes, escapes (ZDLE doubles some bytes) and CRC;
     * eight back to back, the most measured waiting is 512 (the old buffer, full) */
    const unsigned long sub = 1024 * 2 + 6, burst = 8 * sub;

    memset(&io, 0x55, sizeof(io));
    io.io_RBufLen = 512; /* what OpenDevice leaves there: serial.device's default */
    upgetty_setparams(&io, 19200, 0);
    CHECK(io.IOSer.io_Command == SDCMD_SETPARAMS, "a SETPARAMS request");
    CHECK(io.io_Baud == 19200 && io.io_ReadLen == 8 && io.io_WriteLen == 8 && io.io_StopBits == 1, "19200 8N1");
    CHECK(io.io_SerFlags & SERF_XDISABLED, "no XON/XOFF: zmodem's data passes");
    CHECK(!(io.io_SerFlags & SERF_7WIRE), "no RTS/CTS unless asked");
    CHECK(io.io_RBufLen > 512, "the receive buffer is set, not serial.device's 512 default");
    CHECK(io.io_RBufLen <= 65536 && io.io_RBufLen % 64 == 0, "a buffer size serial.device takes");
    CHECK(kept(&io, burst) == burst, "eight streamed 1 KB subpackets, worst-case escaped, are held whole");

    upgetty_setparams(&io, 115200, 1);
    CHECK(io.io_SerFlags & SERF_7WIRE, "RTSCTS asks for RTS/CTS");
    CHECK(io.io_RBufLen == UPGETTY_RBUFLEN, "the same buffer with flow control");

    /* The write request answers at its own port: DoIO on it clears that
     * port's signal, which must never be the read's (getty_rig checks 6-8) */
    {
        struct IOExtSer rd, wr;
        struct MsgPort rport, wport;
        memset(&rd, 0, sizeof(rd));
        memset(&wr, 0x55, sizeof(wr));
        rd.IOSer.io_Message.mn_ReplyPort = &rport;
        rd.io_Baud = 19200;
        upgetty_setup_write(&wr, &rd, &wport);
        CHECK(wr.IOSer.io_Message.mn_ReplyPort == &wport, "the write replies at its own port");
        CHECK(wr.IOSer.io_Message.mn_ReplyPort != rd.IOSer.io_Message.mn_ReplyPort, "not the read's port");
        CHECK(wr.io_Baud == 19200, "the write keeps the opened device's parameters");
    }

    printf("upgetty: %d of %d checks passed\n", checks - fails, checks);
    return fails != 0;
}
