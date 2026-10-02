/* zmodem (see zmodem.h). The frame layout and the constants are the ZMODEM
 * specification's (Chuck Forsberg, 1988); written from the specification,
 * checked against lrzsz on the host (tests/test_zmodem.c). Portable C89. */
#include <string.h>
#include "zmodem.h"
#ifdef ZM_TRACE
#include <stdio.h>
#define TR(a, b) fprintf(stderr, "[%d] %s %ld\n", (int)getpid(), a, (long)(b))
#include <unistd.h>
#else
#define TR(a, b)
#endif

/* ---- the protocol's bytes ------------------------------------------------ */

#define ZPAD   '*'
#define ZDLE   0x18          /* also CAN: five in a row cancel */
#define ZBIN   'A'
#define ZHEX   'B'
#define ZBIN32 'C'
#define XON    0x11
#define XOFF   0x13

/* frame types */
#define ZRQINIT 0
#define ZRINIT  1
#define ZSINIT  2
#define ZACK    3
#define ZFILE   4
#define ZSKIP   5
#define ZNAK    6
#define ZABORT  7
#define ZFIN    8
#define ZRPOS   9
#define ZDATA   10
#define ZEOF    11
#define ZFERR   12
#define ZCRC    13
#define ZCHALLENGE 14
#define ZCOMPL  15
#define ZCAN    16
#define ZFREECNT 17
#define ZCOMMAND 18

/* data subpacket ends */
#define ZCRCE 'h'            /* end of frame, no reply wanted */
#define ZCRCG 'i'            /* more follows, no reply */
#define ZCRCQ 'j'            /* more follows, ZACK wanted */
#define ZCRCW 'k'            /* end of frame, ZACK wanted */
#define ZRUB0 'l'            /* 0x7f */
#define ZRUB1 'm'            /* 0xff */

/* ZRINIT's ZF0 */
#define CANFDX  0x01
#define CANOVIO 0x02
#define CANFC32 0x20

/* ZFILE's ZF0: binary */
#define ZCBIN 1

/* the header's bytes: ZF0 is the last, the position little-endian */
#define ZF0 3

/* internal results (negative) */
#define R_TIMEOUT (-1)
#define R_LINE    (-2)
#define R_CANCEL  (-3)
#define R_ERROR   (-4)       /* garbage, a bad CRC, a bad escape */

#define GOTFRAME  0x100      /* zdlread: a frame end, in the low byte */

#define SUBPACKET 1024       /* the data we send per subpacket */
#define RXMAX     8192       /* the largest subpacket we take (lrzsz -8) */
#define TRIES     10
#define T_HEADER  10000      /* ms */

typedef struct zs {
    const zm_line *line;
    const zm_files *files;
    unsigned char hdr[4];
    int rx32;                /* the last binary header came with CRC-32: so does its data */
    int tx32;                /* we send CRC-32 (the receiver offered it) */
    unsigned char obuf[2048];
    int olen;
    int line_gone;
} zs;

/* ---- checksums ------------------------------------------------------------ */

unsigned zm_crc16(unsigned crc, const unsigned char *b, long n)
{
    int k;
    while (n-- > 0) {
        crc ^= (unsigned)*b++ << 8;
        for (k = 0; k < 8; k++)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        crc &= 0xFFFF;
    }
    return crc;
}

zm_u32 zm_crc32(zm_u32 crc, const unsigned char *b, long n)
{
    int k;
    while (n-- > 0) {
        crc ^= *b++;
        for (k = 0; k < 8; k++)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
    return crc & 0xFFFFFFFFUL;
}

/* ---- output ----------------------------------------------------------------- */

static void flush(zs *s)
{
    if (s->olen && !s->line_gone && s->line->write(s->line->user, s->obuf, s->olen))
        s->line_gone = 1;
    s->olen = 0;
}

static void put(zs *s, int c)
{
    if (s->olen == (int)sizeof(s->obuf))
        flush(s);
    s->obuf[s->olen++] = (unsigned char)c;
}

static void puts_raw(zs *s, const char *t)
{
    while (*t)
        put(s, (unsigned char)*t++);
}

/* a byte ZDLE-escaped: ZDLE itself, DLE, XON and XOFF in both parities
 * (they would be eaten by a modem or a flow-controlled line) */
static void zput(zs *s, int c)
{
    c &= 0xFF;
    switch (c) {
    case ZDLE: case 0x10: case 0x90: case XON: case 0x91: case XOFF: case 0x93:
        put(s, ZDLE);
        put(s, c ^ 0x40);
        break;
    default:
        put(s, c);
    }
}

static void put_hex(zs *s, int c)
{
    static const char d[] = "0123456789abcdef";
    put(s, d[(c >> 4) & 15]);
    put(s, d[c & 15]);
}

static void set_pos(zs *s, zm_u32 pos)
{
    s->hdr[0] = (unsigned char)(pos & 0xFF);
    s->hdr[1] = (unsigned char)((pos >> 8) & 0xFF);
    s->hdr[2] = (unsigned char)((pos >> 16) & 0xFF);
    s->hdr[3] = (unsigned char)((pos >> 24) & 0xFF);
}

static zm_u32 get_pos(const zs *s)
{
    return (zm_u32)s->hdr[0] | ((zm_u32)s->hdr[1] << 8) | ((zm_u32)s->hdr[2] << 16) |
           ((zm_u32)s->hdr[3] << 24);
}

static void hex_header(zs *s, int type)
{
    unsigned char b[5];
    unsigned crc;
    int i;
    b[0] = (unsigned char)type;
    memcpy(b + 1, s->hdr, 4);
    crc = zm_crc16(0, b, 5);
    put(s, ZPAD);
    put(s, ZPAD);
    put(s, ZDLE);
    put(s, ZHEX);
    for (i = 0; i < 5; i++)
        put_hex(s, b[i]);
    put_hex(s, (int)(crc >> 8));
    put_hex(s, (int)(crc & 0xFF));
    put(s, '\r');
    put(s, 0x8A);            /* LF with the parity bit, as the specification's own sender */
    if (type != ZFIN && type != ZACK)
        put(s, XON);         /* in case the far end was stopped by a stray XOFF */
    flush(s);
}

static void bin_header(zs *s, int type)
{
    unsigned char b[5];
    int i;
    b[0] = (unsigned char)type;
    memcpy(b + 1, s->hdr, 4);
    put(s, ZPAD);
    put(s, ZDLE);
    put(s, s->tx32 ? ZBIN32 : ZBIN);
    for (i = 0; i < 5; i++)
        zput(s, b[i]);
    if (s->tx32) {
        zm_u32 crc = ~zm_crc32(0xFFFFFFFFUL, b, 5) & 0xFFFFFFFFUL;
        for (i = 0; i < 4; i++, crc >>= 8)
            zput(s, (int)(crc & 0xFF));
    } else {
        unsigned crc = zm_crc16(0, b, 5);
        zput(s, (int)(crc >> 8));
        zput(s, (int)(crc & 0xFF));
    }
}

static void data_packet(zs *s, const unsigned char *b, long n, int end)
{
    unsigned char e = (unsigned char)end;
    long i;
    for (i = 0; i < n; i++)
        zput(s, b[i]);
    put(s, ZDLE);
    put(s, end);
    if (s->tx32) {
        zm_u32 crc = ~zm_crc32(zm_crc32(0xFFFFFFFFUL, b, n), &e, 1) & 0xFFFFFFFFUL;
        for (i = 0; i < 4; i++, crc >>= 8)
            zput(s, (int)(crc & 0xFF));
    } else {
        unsigned crc = zm_crc16(zm_crc16(0, b, n), &e, 1);
        zput(s, (int)(crc >> 8));
        zput(s, (int)(crc & 0xFF));
    }
    if (end == ZCRCW)
        put(s, XON);
    flush(s);
}

/* ---- input ---------------------------------------------------------------------- */

/* a byte off the line, XON and XOFF dropped (flow control's, never data:
 * data has them escaped) */
static int raw(zs *s, int timeout)
{
    for (;;) {
        int c = s->line->getc(s->line->user, timeout);
        if (c == ZM_TIMEOUT)
            return R_TIMEOUT;
        if (c < 0)
            return R_LINE;
        if (c == XON || c == XOFF || c == 0x91 || c == 0x93)
            continue;
        return c;
    }
}

/* a ZDLE-decoded byte; GOTFRAME | end for a subpacket end */
static int zdlread(zs *s, int timeout)
{
    int c = raw(s, timeout), cans = 1;
    if (c != ZDLE)
        return c;
    for (;;) {
        c = raw(s, timeout);
        if (c < 0)
            return c;
        if (c == ZDLE) {
            if (++cans >= 5)
                return R_CANCEL;
            continue;
        }
        switch (c) {
        case ZCRCE: case ZCRCG: case ZCRCQ: case ZCRCW:
            return GOTFRAME | c;
        case ZRUB0:
            return 0x7F;
        case ZRUB1:
            return 0xFF;
        }
        if ((c & 0x60) == 0x40)
            return c ^ 0x40;
        return R_ERROR;
    }
}

static int hexval(int c)
{
    c &= 0x7F;
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int hexbyte(zs *s, int timeout)
{
    int a = raw(s, timeout), b, ha, hb;
    if (a < 0)
        return a;
    b = raw(s, timeout);
    if (b < 0)
        return b;
    ha = hexval(a);
    hb = hexval(b);
    return ha < 0 || hb < 0 ? R_ERROR : ha * 16 + hb;
}

/* the next header: its type (hdr filled), or R_* */
static int get_header(zs *s, int timeout)
{
    unsigned char b[9];
    int c, i, garbage = 0, cans = 0;
    for (;;) {
        c = raw(s, timeout);
        if (c < 0)
            return c;
        if (c == ZDLE) {
            if (++cans >= 5)
                return R_CANCEL;
        } else
            cans = 0;
        if ((c & 0x7F) != ZPAD) {
            if (++garbage > 2400)
                return R_ERROR; /* no header in a whole buffer's worth */
            continue;
        }
        do
            c = raw(s, timeout);
        while (c >= 0 && (c & 0x7F) == ZPAD);
        if (c < 0)
            return c;
        if (c != ZDLE)
            continue;
        c = raw(s, timeout);
        if (c < 0)
            return c;
        if (c == ZHEX) {
            for (i = 0; i < 7; i++) {
                c = hexbyte(s, timeout);
                if (c < 0)
                    return c == R_ERROR ? R_ERROR : c;
                b[i] = (unsigned char)c;
            }
            if (zm_crc16(0, b, 5) != ((unsigned)b[5] << 8 | b[6]))
                return R_ERROR;
            memcpy(s->hdr, b + 1, 4);
            TR("hex header", b[0]);
            return b[0];
        }
        if (c == ZBIN || c == ZBIN32) {
            int n = c == ZBIN32 ? 9 : 7;
            for (i = 0; i < n; i++) {
                c = zdlread(s, timeout);
                if (c < 0)
                    return c;
                if (c & GOTFRAME)
                    return R_ERROR;
                b[i] = (unsigned char)c;
            }
            if (n == 9) {
                zm_u32 crc = ~zm_crc32(0xFFFFFFFFUL, b, 5) & 0xFFFFFFFFUL;
                zm_u32 got = (zm_u32)b[5] | ((zm_u32)b[6] << 8) | ((zm_u32)b[7] << 16) |
                             ((zm_u32)b[8] << 24);
                if (crc != got)
                    return R_ERROR;
            } else if (zm_crc16(0, b, 5) != ((unsigned)b[5] << 8 | b[6]))
                return R_ERROR;
            s->rx32 = n == 9;
            memcpy(s->hdr, b + 1, 4);
            TR("bin header", b[0]);
            return b[0];
        }
    }
}

/* a data subpacket into buf: its end byte (ZCRCE...), or R_* */
static int get_data(zs *s, unsigned char *buf, long max, long *len, int timeout)
{
    zm_u32 c32 = 0xFFFFFFFFUL;
    unsigned c16 = 0;
    int c, i, n;
    *len = 0;
    for (;;) {
        c = zdlread(s, timeout);
        if (c < 0)
            return c;
        if (c & GOTFRAME) {
            unsigned char e = (unsigned char)(c & 0xFF), t[4];
            n = s->rx32 ? 4 : 2;
            for (i = 0; i < n; i++) {
                int d = zdlread(s, timeout);
                if (d < 0)
                    return d;
                if (d & GOTFRAME)
                    return R_ERROR;
                t[i] = (unsigned char)d;
            }
            if (s->rx32) {
                zm_u32 crc = ~zm_crc32(c32, &e, 1) & 0xFFFFFFFFUL;
                if (crc != ((zm_u32)t[0] | ((zm_u32)t[1] << 8) | ((zm_u32)t[2] << 16) |
                            ((zm_u32)t[3] << 24)))
                    return R_ERROR;
            } else if (zm_crc16(c16, &e, 1) != ((unsigned)t[0] << 8 | t[1]))
                return R_ERROR;
            return e;
        }
        if (*len >= max)
            return R_ERROR; /* longer than any subpacket may be */
        buf[(*len)++] = (unsigned char)c;
        if (s->rx32) {
            unsigned char b = (unsigned char)c;
            c32 = zm_crc32(c32, &b, 1);
        } else {
            unsigned char b = (unsigned char)c;
            c16 = zm_crc16(c16, &b, 1);
        }
    }
}

static void cancel(zs *s)
{
    int i;
    for (i = 0; i < 8; i++)
        put(s, ZDLE);
    for (i = 0; i < 8; i++)
        put(s, 0x08);        /* backspaces over the CANs, as lrzsz does */
    flush(s);
}

static int result(int r)
{
    return r == R_CANCEL ? ZM_ERR_CANCEL : r == R_LINE ? ZM_ERR_LINE
         : r == R_TIMEOUT ? ZM_ERR_TIMEOUT : ZM_ERR_PROTO;
}

/* ---- sending ------------------------------------------------------------------- */

static const char *base_name(const char *p)
{
    const char *b = p;
    for (; *p; p++)
        if (*p == '/' || *p == ':')
            b = p + 1;
    return b;
}

static int put_num(char *o, unsigned long v, int base)
{
    char t[24];
    int n = 0, k = 0;
    do {
        t[n++] = (char)('0' + v % (unsigned long)base);
        v /= (unsigned long)base;
    } while (v);
    while (n)
        o[k++] = t[--n];
    return k;
}

/* ZCRC: the receiver asks for the CRC of the file (it has one already) */
static zm_u32 file_crc(zs *s, void *f)
{
    unsigned char b[512];
    zm_u32 crc = 0xFFFFFFFFUL;
    long n;
    if (s->files->seek(s->files->user, f, 0))
        return 0;
    while ((n = s->files->read(s->files->user, f, b, sizeof(b))) > 0)
        crc = zm_crc32(crc, b, n);
    return ~crc & 0xFFFFFFFFUL;
}

/* one file's data from pos to the end; ZM_OK once the receiver took it */
static int send_data(zs *s, void *f, long size, zm_u32 pos, int window)
{
    unsigned char b[SUBPACKET];
    int tries = 0, t;
    for (;;) {
        int restart = 0;
        if (s->files->seek(s->files->user, f, (long)pos))
            return ZM_ERR_FILE;
        set_pos(s, pos);
        bin_header(s, ZDATA);
        for (;;) {
            long n = s->files->read(s->files->user, f, b, sizeof(b));
            int end;
            if (n < 0)
                return ZM_ERR_FILE;
            pos += (zm_u32)n;
            end = (n == 0 || (size >= 0 && (long)pos >= size)) ? ZCRCE : window ? ZCRCW : ZCRCG;
            data_packet(s, b, n, end);
            if (s->line_gone)
                return ZM_ERR_LINE;
            if (end == ZCRCW) {
                /* a receiver with a buffer limit: wait for its ZACK */
                t = get_header(s, T_HEADER);
                if (t == ZRPOS) {
                    pos = get_pos(s);
                    restart = 1;
                    break;
                }
                if (t == R_CANCEL || t == R_LINE)
                    return result(t);
                if (t != ZACK && ++tries > TRIES)
                    return result(t);
                continue;
            }
            if (end == ZCRCE)
                break;
            /* streaming: anything the receiver says now is about an error */
            t = s->line->getc(s->line->user, 0);
            if (t >= 0) {
                if (t == ZPAD || t == (ZPAD | 0x80) || t == ZDLE) {
                    /* put it back is not possible: read the header from here */
                    int h;
                    if (t == ZDLE) {
                        /* a cancel starts with ZDLE: count it in get_header */
                    }
                    h = get_header(s, T_HEADER);
                    if (h == ZRPOS) {
                        pos = get_pos(s);
                        restart = 1;
                        break;
                    }
                    if (h == R_CANCEL)
                        return ZM_ERR_CANCEL;
                }
            } else if (t == ZM_LINE_ERROR)
                return ZM_ERR_LINE;
        }
        if (restart) {
            if (++tries > TRIES)
                return ZM_ERR_PROTO;
            continue;
        }
        /* the end: ZEOF until the receiver says ZRINIT */
        for (;;) {
            set_pos(s, pos);
            bin_header(s, ZEOF);
            flush(s);
            t = get_header(s, T_HEADER);
            if (t == ZRINIT)
                return ZM_OK;
            if (t == ZRPOS) {
                pos = get_pos(s);
                restart = 1;
                break;
            }
            if (t == ZACK)
                continue;
            if (t == R_CANCEL || t == R_LINE)
                return result(t);
            if (++tries > TRIES)
                return result(t);
        }
        if (++tries > TRIES)
            return ZM_ERR_PROTO;
    }
}

int zm_send(const zm_line *line, const zm_files *files, const char *const *names, int count)
{
    zs s;
    unsigned char info[512];
    int t, tries = 0, i, rc = ZM_OK, window = 0;
    memset(&s, 0, sizeof(s));
    s.line = line;
    s.files = files;
    puts_raw(&s, "rz\r");
    set_pos(&s, 0);
    hex_header(&s, ZRQINIT);
    for (;;) {
        t = get_header(&s, T_HEADER);
        if (t == ZRINIT) {
            s.tx32 = (s.hdr[ZF0] & CANFC32) != 0;
            window = (s.hdr[0] | s.hdr[1]) != 0; /* a buffer size: no streaming */
            break;
        }
        if (t == ZCHALLENGE) {
            hex_header(&s, ZACK); /* the same number back */
            continue;
        }
        if (t == R_CANCEL || t == R_LINE)
            return result(t);
        if (t < 0 && ++tries > TRIES)
            return result(t);
        if (t < 0) {
            set_pos(&s, 0);
            hex_header(&s, ZRQINIT);
        }
    }
    for (i = 0; i < count && rc == ZM_OK; i++) {
        long size = -1, k;
        zm_u32 mtime = 0;
        void *f = files->open_read(files->user, names[i], &size, &mtime);
        const char *bn = base_name(names[i]);
        if (!f)
            continue;
        /* the file's name, then "size mtime mode serial files-left bytes-left" */
        k = (long)strlen(bn);
        if (k > 255)
            k = 255;
        memcpy(info, bn, k);
        info[k++] = 0;
        k += put_num((char *)info + k, (unsigned long)(size < 0 ? 0 : size), 10);
        info[k++] = ' ';
        k += put_num((char *)info + k, mtime, 8);
        memcpy(info + k, " 100644 0 ", 10);
        k += 10;
        k += put_num((char *)info + k, (unsigned long)(count - i), 10);
        info[k++] = ' ';
        k += put_num((char *)info + k, (unsigned long)(size < 0 ? 0 : size), 10);
        info[k++] = 0;
        tries = 0;
        for (;;) {
            s.hdr[0] = s.hdr[1] = s.hdr[2] = 0;
            s.hdr[ZF0] = ZCBIN;
            bin_header(&s, ZFILE);
            data_packet(&s, info, k, ZCRCW);
            /* a ZRINIT now is a late copy (the receiver answers ZRQINIT
             * with one more): not an answer to this ZFILE -- sending ZFILE
             * again for it set off a ZRPOS storm. Only a timeout or ZNAK
             * repeats the offer. */
            do
                t = get_header(&s, T_HEADER);
            while (t == ZRINIT || t == R_ERROR);
            while (t == ZCRC) {
                set_pos(&s, file_crc(&s, f));
                hex_header(&s, ZCRC);
                t = get_header(&s, T_HEADER);
            }
            if (t == ZRPOS) {
                rc = send_data(&s, f, size, get_pos(&s), window);
                files->close(files->user, f, rc == ZM_OK);
                break;
            }
            if (t == ZSKIP) {
                files->close(files->user, f, 0);
                break;
            }
            if (t == R_CANCEL || t == R_LINE) {
                files->close(files->user, f, 0);
                return result(t);
            }
            if (++tries > TRIES) {
                files->close(files->user, f, 0);
                return result(t);
            }
        }
    }
    if (rc != ZM_OK) {
        if (rc != ZM_ERR_CANCEL && rc != ZM_ERR_LINE)
            cancel(&s);
        return rc;
    }
    /* the session's end: ZFIN both ways, then "OO" (over and out) */
    for (tries = 0; tries < 3; tries++) {
        set_pos(&s, 0);
        hex_header(&s, ZFIN);
        t = get_header(&s, T_HEADER);
        if (t == ZFIN)
            break;
        if (t == R_CANCEL || t == R_LINE)
            return result(t);
    }
    puts_raw(&s, "OO");
    flush(&s);
    return ZM_OK;
}

/* ---- receiving ----------------------------------------------------------------- */

static long parse_dec(const unsigned char **p)
{
    long v = 0;
    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '9')
        v = v * 10 + (*(*p)++ - '0');
    return v;
}

static zm_u32 parse_oct(const unsigned char **p)
{
    zm_u32 v = 0;
    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '7')
        v = v * 8 + (zm_u32)(*(*p)++ - '0');
    return v;
}

static void send_rinit(zs *s)
{
    s->hdr[0] = s->hdr[1] = s->hdr[2] = 0; /* no buffer limit: stream */
    s->hdr[ZF0] = CANFDX | CANOVIO | CANFC32;
    hex_header(s, ZRINIT);
}

/* one file's data, after ZFILE was answered with ZRPOS(0). ZM_OK once ZEOF
 * closed it; 1 when the sender ended the session (ZFIN) instead. */
static int receive_file(zs *s, void *f, unsigned char *buf)
{
    zm_u32 pos = 0;
    int t, tries = 0;
    for (;;) {
        t = get_header(s, T_HEADER);
        if (t == ZDATA) {
            if (get_pos(s) != pos) {
                set_pos(s, pos);
                hex_header(s, ZRPOS);
                continue;
            }
            for (;;) {
                long n;
                int e = get_data(s, buf, RXMAX, &n, T_HEADER);
                if (e < 0) {
                    if (e == R_CANCEL || e == R_LINE)
                        return result(e);
                    if (++tries > TRIES)
                        return ZM_ERR_PROTO;
                    set_pos(s, pos);   /* from the last good byte again */
                    hex_header(s, ZRPOS);
                    break;
                }
                if (n && s->files->write(s->files->user, f, buf, n) != n) {
                    cancel(s);
                    return ZM_ERR_FILE;
                }
                pos += (zm_u32)n;
                tries = 0;
                if (e == ZCRCQ || e == ZCRCW) {
                    set_pos(s, pos);
                    hex_header(s, ZACK);
                }
                if (e == ZCRCE || e == ZCRCW)
                    break;
            }
            continue;
        }
        if (t == ZEOF) {
            if (get_pos(s) != pos)
                continue; /* an old one: the data from our ZRPOS is still coming */
            return ZM_OK;
        }
        if (t == ZFILE) {
            /* our ZRPOS was lost: the sender offers the file again */
            long n;
            get_data(s, buf, RXMAX, &n, T_HEADER);
            set_pos(s, pos);
            hex_header(s, ZRPOS);
            continue;
        }
        if (t == ZFIN)
            return 1;
        if (t == R_CANCEL || t == R_LINE)
            return result(t);
        if (t == R_ERROR)
            continue; /* the rest of a stream we already asked to restart: it
                       * can look like a header and fail its CRC; answering each
                       * with ZRPOS restarted the sender again and again */
        if (++tries > TRIES)
            return result(t);
        set_pos(s, pos);
        hex_header(s, ZRPOS);
    }
}

static unsigned char rxbuf[RXMAX + 8];

int zm_receive(const zm_line *line, const zm_files *files, int *received)
{
    zs s;
    int t, tries = 0, r;
    *received = 0;
    memset(&s, 0, sizeof(s));
    s.line = line;
    s.files = files;
    send_rinit(&s);
    for (;;) {
        t = get_header(&s, T_HEADER);
        if (t == ZFILE) {
            long n, size;
            zm_u32 mtime;
            const unsigned char *p;
            void *f;
            int e = get_data(&s, rxbuf, RXMAX, &n, T_HEADER);
            if (e < 0) {
                if (e == R_CANCEL || e == R_LINE)
                    return result(e);
                hex_header(&s, ZNAK);
                continue;
            }
            rxbuf[n] = 0;
            p = rxbuf + strlen((const char *)rxbuf) + 1;
            size = (p < rxbuf + n && *p) ? parse_dec(&p) : -1;
            mtime = (p < rxbuf + n && *p) ? parse_oct(&p) : 0;
            f = files->open_write(files->user, (const char *)rxbuf, size, mtime);
            if (!f) {
                set_pos(&s, 0);
                hex_header(&s, ZSKIP);
                continue;
            }
            set_pos(&s, 0);
            hex_header(&s, ZRPOS);
            r = receive_file(&s, f, rxbuf);
            files->close(files->user, f, r == ZM_OK);
            if (r == ZM_OK) {
                (*received)++;
                tries = 0;
                send_rinit(&s);
                continue;
            }
            if (r != 1)
                return r;
            t = ZFIN; /* the session ended in the middle of a file */
        }
        if (t == ZFIN) {
            int k;
            set_pos(&s, 0);
            hex_header(&s, ZFIN);
            for (k = 0; k < 2; k++)
                if (raw(&s, 1000) < 0)
                    break; /* "OO": wanted, not needed */
            return ZM_OK;
        }
        if (t == ZSINIT) {
            long n;
            get_data(&s, rxbuf, RXMAX, &n, T_HEADER);
            set_pos(&s, 1);
            hex_header(&s, ZACK);
            continue;
        }
        if (t == ZRQINIT || t == ZDATA || t == ZEOF) {
            send_rinit(&s);
            continue;
        }
        if (t == R_CANCEL || t == R_LINE)
            return result(t);
        if (t < 0) {
            if (++tries > TRIES)
                return result(t);
            send_rinit(&s);
        }
    }
}
