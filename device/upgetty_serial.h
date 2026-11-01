/* upgetty's serial.device parameters: one function, so the host test
 * (tests/test_upgetty_serial.c, `make test ONLY=upgetty`) checks the request
 * main() sends.
 *
 * The receive buffer. Without RTS/CTS (the default: a three-wire cable, and
 * FS-UAE's serial port never raises CTS) nothing holds the far end back, so
 * serial.device's receive buffer is all that stands between the wire and the
 * pump. serial.device's default is 512 bytes; a host sending one ZMODEM
 * subpacket (lsz: 1024 bytes, streamed back to back) filled it before the
 * pump's next turn, the read came back SerErr_BufOverflow with the buffer's
 * bytes gone, and rz answered ZRPOS(0) for ever (zmodem_rig check 4, rc 128,
 * 0 bytes; thoughts/spot/term-findings/zmodem-rz-rc128.md "Cause 2":
 * 3-4 overflows a run with 512, none and 70 KB byte for byte with 64 KB).
 * serial.device takes up to 65536 (a multiple of 64). */
#ifndef UPGETTY_SERIAL_H
#define UPGETTY_SERIAL_H
#include <exec/types.h>
#include <exec/io.h>
#include <devices/serial.h>

#define UPGETTY_RBUFLEN 65536UL

/* 8N1, no XON/XOFF (zmodem's data must pass), RTS/CTS only when asked, the
 * receive buffer above; ready for DoIO. */
static void upgetty_setparams(struct IOExtSer *io, ULONG baud, int rtscts)
{
    io->io_Baud = baud;
    io->io_ReadLen = io->io_WriteLen = 8;
    io->io_StopBits = 1;
    io->io_RBufLen = UPGETTY_RBUFLEN;
    io->io_SerFlags = SERF_XDISABLED | SERF_RAD_BOOGIE | (rtscts ? SERF_7WIRE : 0);
    io->IOSer.io_Command = SDCMD_SETPARAMS;
}

/* The write request, copied from the opened read request and answering at its
 * OWN reply port. serial_write uses DoIO, which clears its reply port's
 * signal bit while it waits; on the read's port that swallowed the signal of
 * the pending 1-byte CMD_READ when it completed during the write, the main
 * Wait then slept on a finished read, and a key typed right after output was
 * lost (getty_rig checks 6, 7, 8; thoughts/spot/term-findings/
 * getty-rig-checks-6-8.md "Fix (reply port)"). */
static void upgetty_setup_write(struct IOExtSer *wr, const struct IOExtSer *rd, struct MsgPort *wport)
{
    *wr = *rd;
    wr->IOSer.io_Message.mn_ReplyPort = wport;
}
#endif
