/* A Unix line discipline (termios), portable C89 with no OS calls, for the
 * consoles that serve ixemul programs: XCON: now, PTY: later (one module,
 * one behaviour). Tested on the host (tests/test_ldisc.c).
 *
 * vt_termios is ixemul 48.2's struct termios byte for byte (BSD 4.4
 * sys/termios.h): it is also the wire format of ACTION_VTCON_TCGETA/TCSETA,
 * so a patched ixemul passes a program's termios through untouched. */
#ifndef LDISC_H
#define LDISC_H

typedef unsigned int ld_flag;          /* tcflag_t: 32 bits on both sides */

#define LD_NCCS 20
typedef struct vt_termios {
    ld_flag c_iflag, c_oflag, c_cflag, c_lflag;
    unsigned char c_cc[LD_NCCS];
    int c_ispeed, c_ospeed;
} vt_termios;

/* c_cc indices */
#define LD_VEOF 0
#define LD_VEOL 1
#define LD_VEOL2 2
#define LD_VERASE 3
#define LD_VWERASE 4
#define LD_VKILL 5
#define LD_VREPRINT 6
#define LD_VINTR 8
#define LD_VQUIT 9
#define LD_VSUSP 10
#define LD_VDSUSP 11
#define LD_VSTART 12
#define LD_VSTOP 13
#define LD_VLNEXT 14
#define LD_VDISCARD 15
#define LD_VMIN 16
#define LD_VTIME 17
#define LD_VSTATUS 18
#define LD_DISABLED 0xFF               /* _POSIX_VDISABLE */

/* c_iflag */
#define LD_IGNBRK 0x001
#define LD_BRKINT 0x002
#define LD_ISTRIP 0x020
#define LD_INLCR 0x040
#define LD_IGNCR 0x080
#define LD_ICRNL 0x100
#define LD_IXON 0x200
#define LD_IXOFF 0x400
#define LD_IXANY 0x800
#define LD_IMAXBEL 0x2000
/* c_oflag */
#define LD_OPOST 0x001
#define LD_ONLCR 0x002
#define LD_OXTABS 0x004
#define LD_ONOEOT 0x008
/* c_cflag */
#define LD_CS8 0x300
#define LD_CREAD 0x800
/* c_lflag */
#define LD_ECHOKE 0x001
#define LD_ECHOE 0x002
#define LD_ECHOK 0x004
#define LD_ECHO 0x008
#define LD_ECHONL 0x010
#define LD_ECHOPRT 0x020
#define LD_ECHOCTL 0x040
#define LD_ISIG 0x080
#define LD_ICANON 0x100
#define LD_ALTWERASE 0x200
#define LD_IEXTEN 0x400
#define LD_TOSTOP 0x00400000UL
#define LD_NOFLSH 0x80000000UL

/* tcsetattr's optional_actions */
#define LD_TCSANOW 0
#define LD_TCSADRAIN 1
#define LD_TCSAFLUSH 2

/* BSD signal numbers, as ixemul numbers them */
#define LD_SIGINT 2
#define LD_SIGQUIT 3
#define LD_SIGTSTP 18
#define LD_SIGWINCH 28

/* struct winsize (sys/ttycom.h): the ACTION_VTCON_GWINSZ/SWINSZ wire format */
typedef struct vt_winsize {
    unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel;
} vt_winsize;

#define LD_LINE 1024                   /* one canonical line */
#define LD_QUEUE 4096                  /* input ready for read() */

typedef struct ldisc {
    vt_termios t;
    unsigned char line[LD_LINE];       /* the canonical line being edited */
    int len;
    unsigned char q[LD_QUEUE];         /* ready for read(): whole lines, or raw bytes */
    int qlen;
    int lines;                         /* complete lines and EOFs in q (ICANON) */
    int lnext;                         /* the next byte is literal (VLNEXT) */
    int stopped;                       /* output stopped by VSTOP (IXON) */
    /* echo: bytes for the terminal, already output-processed */
    void (*echo)(void *user, const unsigned char *s, int n);
    /* ISIG: a key asked for a signal (LD_SIG*) */
    void (*signal)(void *user, int sig);
    void *user;
} ldisc;

/* ixemul 63.1's defaults (Linux-like): ICRNL IXON, OPOST ONLCR, CS8 CREAD,
 * ISIG ICANON IEXTEN ECHO ECHOE ECHOK ECHOKE ECHOCTL; ^C ^\ ^Z ^D, DEL erase,
 * ^U kill, ^W word, ^R reprint, ^V lnext, VMIN 1, VTIME 0. */
void ld_defaults(vt_termios *t);
void ld_init(ldisc *l);

/* tcsetattr: TCSAFLUSH also drops pending input. Switching from canonical
 * to not moves an unfinished line into the queue, as a Unix tty does. */
void ld_set(ldisc *l, const vt_termios *t, int action);

/* Bytes from the keyboard. */
void ld_input(ldisc *l, const unsigned char *b, int n);

/* What a read() may take now, into buf (at most max). ICANON: one line
 * (with its newline) or 0 with *eof set at a VEOF; otherwise the queued
 * bytes. -1: nothing yet (the reader waits; for !ICANON, VMIN/VTIME
 * decide how long, see ld_read_ready). */
long ld_read(ldisc *l, unsigned char *buf, long max, int *eof);

/* May a read return now? ICANON: a line or an EOF is there. Otherwise
 * VMIN bytes are there (VMIN 0: always). The VTIME timer is the caller's. */
int ld_read_ready(const ldisc *l);

/* What a read() that is waiting does now. The VTIME timer is the
 * caller's; timer_fired: it ran out since the read began to wait.
 *   LD_RD_TAKE  ld_read what is there (it may be nothing after VTIME)
 *   LD_RD_ZERO  return 0 bytes: VMIN 0 and VTIME 0, a poll found nothing
 *   LD_RD_WAIT  keep waiting; *arm set: start the VTIME timer
 *               (c_cc[VTIME] tenths) unless it is running already.
 * XCON: and PTY: both serve their reads with it. */
#define LD_RD_WAIT 0
#define LD_RD_TAKE 1
#define LD_RD_ZERO 2
int ld_read_action(const ldisc *l, int timer_fired, int *arm);

/* ACTION_WAIT_CHAR, select(): would a read find input? */
int ld_input_pending(const ldisc *l);

/* Output processing (OPOST: ONLCR, OXTABS, ONOEOT) of n bytes into out;
 * out needs 8 * n bytes at most (a tab to 8 spaces). *col tracks the
 * column for OXTABS. Returns the bytes written. */
long ld_output(const vt_termios *t, const unsigned char *in, long n, unsigned char *out, int *col);

#endif
