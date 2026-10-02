/* zmodem: ZMODEM file transfer, sender (sz) and receiver (rz), as Chuck
 * Forsberg's ZMODEM specification describes it and lrzsz speaks it.
 *
 * Pure C89, no OS calls: the line and the files are reached through
 * callbacks, so the same core runs on the host (tests/test_zmodem.c, also
 * against the host's own lrzsz) and on the Amiga (zm/zm_amiga.c: C:sz and
 * C:rz over a serial login, ledger T4 G2/G3) -- and small enough for a ROM.
 *
 * What it does: CRC-32 when the receiver offers it (else CRC-16), binary
 * headers, streaming data (ZCRCG) with a ZRPOS restart from the receiver's
 * last good byte on any error, several files a session, 8-bit clean (ZDLE
 * escapes ZDLE, DLE, XON and XOFF in both parities). What it does not: the
 * crash-recovery resume, ZCOMMAND, compression, encryption. */
#ifndef ZMODEM_H
#define ZMODEM_H

typedef unsigned long zm_u32;

/* The line. getc: one byte (0-255), ZM_TIMEOUT after timeout_ms with
 * nothing, ZM_LINE_ERROR when the line is gone. write: all n bytes, 0 when
 * they went. */
#define ZM_TIMEOUT    (-1)
#define ZM_LINE_ERROR (-2)
typedef struct zm_line {
    int (*getc)(void *user, int timeout_ms);
    int (*write)(void *user, const unsigned char *b, long n);
    void *user;
} zm_line;

/* The files. A handle is the caller's (0 = failed). open_read gives the
 * size; mtime is seconds since 1970 (0 when unknown). open_write gets the
 * name the sender sent (no directory part is trusted: the caller decides)
 * and its size (-1 unknown); 0 skips the file. */
typedef struct zm_files {
    void *(*open_read)(void *user, const char *name, long *size, zm_u32 *mtime);
    long (*read)(void *user, void *f, unsigned char *b, long n);       /* 0 at the end, <0 error */
    int (*seek)(void *user, void *f, long pos);                        /* 0 when done */
    void *(*open_write)(void *user, const char *name, long size, zm_u32 mtime);
    long (*write)(void *user, void *f, const unsigned char *b, long n); /* n when done */
    void (*close)(void *user, void *f, int complete);                 /* complete: the whole file */
    void *user;
} zm_files;

/* Results */
#define ZM_OK          0
#define ZM_ERR_TIMEOUT 1   /* the other side stopped answering */
#define ZM_ERR_CANCEL  2   /* the other side cancelled (5 x CAN) */
#define ZM_ERR_LINE    3   /* the line is gone */
#define ZM_ERR_PROTO   4   /* too many errors in a row */
#define ZM_ERR_FILE    5   /* a file could not be read or written */

/* Send the files (names as open_read takes them). The receiver is started
 * with "rz\r" first, as lrzsz's sz does. */
int zm_send(const zm_line *line, const zm_files *files, const char *const *names, int count);

/* Receive files until the sender ends the session. *received counts them. */
int zm_receive(const zm_line *line, const zm_files *files, int *received);

/* The two checksums, exposed for the tests: CRC-16/XMODEM and CRC-32/IEEE
 * over n bytes, continuing from crc (0 / 0xFFFFFFFF to start; CRC-32 is
 * complemented at the end by the caller, as ZMODEM does). */
unsigned zm_crc16(unsigned crc, const unsigned char *b, long n);
zm_u32 zm_crc32(zm_u32 crc, const unsigned char *b, long n);

#endif
