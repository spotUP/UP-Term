/* A Unix line discipline (see ldisc.h). */
#include <string.h>
#include "ldisc.h"

void ld_defaults(vt_termios *t)
{
    memset(t, 0, sizeof(*t));
    t->c_iflag = LD_ICRNL | LD_IXON;
    t->c_oflag = LD_OPOST | LD_ONLCR;
    t->c_cflag = LD_CS8 | LD_CREAD;
    t->c_lflag = LD_ISIG | LD_ICANON | LD_IEXTEN | LD_ECHO | LD_ECHOE | LD_ECHOK | LD_ECHOKE |
                 LD_ECHOCTL;
    memset(t->c_cc, LD_DISABLED, sizeof(t->c_cc));
    t->c_cc[LD_VEOF] = 0x04;
    t->c_cc[LD_VERASE] = 0x7F;
    t->c_cc[LD_VWERASE] = 0x17;
    t->c_cc[LD_VKILL] = 0x15;
    t->c_cc[LD_VREPRINT] = 0x12;
    t->c_cc[LD_VINTR] = 0x03;
    t->c_cc[LD_VQUIT] = 0x1C;
    t->c_cc[LD_VSUSP] = 0x1A;
    t->c_cc[LD_VSTART] = 0x11;
    t->c_cc[LD_VSTOP] = 0x13;
    t->c_cc[LD_VLNEXT] = 0x16;
    t->c_cc[LD_VMIN] = 1;
    t->c_cc[LD_VTIME] = 0;
    t->c_ispeed = t->c_ospeed = 38400;
}

void ld_init(ldisc *l)
{
    void (*echo)(void *, const unsigned char *, int) = l->echo;
    void (*signal)(void *, int) = l->signal;
    void *user = l->user;
    memset(l, 0, sizeof(*l));
    l->echo = echo;
    l->signal = signal;
    l->user = user;
    ld_defaults(&l->t);
}

/* Is c the character in c_cc[i] (and that one not disabled)? */
static int is_cc(const ldisc *l, int c, int i)
{
    return l->t.c_cc[i] != LD_DISABLED && c == l->t.c_cc[i];
}

static void echo_raw(ldisc *l, const unsigned char *s, int n)
{
    unsigned char out[64];
    int col = 0;
    while (n > 0 && l->echo) {
        int k = n > 8 ? 8 : n; /* 8 in -> at most 64 out */
        long m = ld_output(&l->t, s, k, out, &col);
        l->echo(l->user, out, (int)m);
        s += k;
        n -= k;
    }
}

static int is_ctl(int c)
{
    return (c < 0x20 && c != '\t' && c != '\n') || c == 0x7F;
}

/* Echo one input character: ^X for controls with ECHOCTL. Without ECHO
 * only a newline with ECHONL. */
static void echo_char(ldisc *l, int c)
{
    unsigned char s[2];
    if (!(l->t.c_lflag & LD_ECHO)) {
        if (c == '\n' && (l->t.c_lflag & LD_ECHONL))
            echo_raw(l, (const unsigned char *)"\n", 1);
        return;
    }
    if ((l->t.c_lflag & LD_ECHOCTL) && is_ctl(c)) {
        s[0] = '^';
        s[1] = (unsigned char)(c == 0x7F ? '?' : c + '@');
        echo_raw(l, s, 2);
        return;
    }
    s[0] = (unsigned char)c;
    echo_raw(l, s, 1);
}

/* Visually take back the last character of the line (ECHOE). */
static void rub_out(ldisc *l, int c)
{
    int w = ((l->t.c_lflag & LD_ECHOCTL) && is_ctl(c)) ? 2 : 1;
    if (!(l->t.c_lflag & LD_ECHO))
        return;
    if (l->t.c_lflag & LD_ECHOE) {
        while (w--)
            echo_raw(l, (const unsigned char *)"\b \b", 3);
    } else {
        echo_char(l, l->t.c_cc[LD_VERASE]);
    }
}

static void erase_char(ldisc *l)
{
    if (l->len > 0) {
        int c = l->line[--l->len];
        /* a UTF-8 character goes whole: its continuation bytes first */
        while ((c & 0xC0) == 0x80 && l->len > 0 && (l->line[l->len - 1] & 0x80))
            c = l->line[--l->len];
        rub_out(l, c);
    }
}

static void erase_word(ldisc *l)
{
    while (l->len > 0 && (l->line[l->len - 1] == ' ' || l->line[l->len - 1] == '\t'))
        erase_char(l);
    while (l->len > 0 && l->line[l->len - 1] != ' ' && l->line[l->len - 1] != '\t')
        erase_char(l);
}

static void kill_line(ldisc *l)
{
    if ((l->t.c_lflag & LD_ECHO) && (l->t.c_lflag & LD_ECHOKE)) {
        while (l->len > 0)
            erase_char(l);
        return;
    }
    echo_char(l, l->t.c_cc[LD_VKILL]);
    if (l->t.c_lflag & LD_ECHOK)
        echo_raw(l, (const unsigned char *)"\n", 1);
    l->len = 0;
}

static void reprint(ldisc *l)
{
    int i;
    echo_char(l, l->t.c_cc[LD_VREPRINT]);
    echo_raw(l, (const unsigned char *)"\n", 1);
    for (i = 0; i < l->len; i++)
        echo_char(l, l->line[i]);
}

/* In ICANON mode the queue holds whole lines, each as <len hi><len lo>
 * <bytes>, and an EOF (VEOF on an empty line) as a line of length 0xFFFF;
 * `lines` counts them. Without ICANON it holds plain bytes. */
static void queue_bytes(ldisc *l, const unsigned char *b, int n)
{
    if (l->qlen + n > LD_QUEUE)
        n = LD_QUEUE - l->qlen; /* full: the rest is lost, as on a Unix tty */
    if (n > 0) {
        memcpy(l->q + l->qlen, b, n);
        l->qlen += n;
    }
}

static void commit_line(ldisc *l, int with_eof)
{
    unsigned char hdr[2];
    int n = with_eof && l->len == 0 ? 0xFFFF : l->len;
    if (l->qlen + 2 + (n == 0xFFFF ? 0 : n) > LD_QUEUE) {
        l->len = 0; /* no room: dropped */
        return;
    }
    hdr[0] = (unsigned char)(n >> 8);
    hdr[1] = (unsigned char)(n & 0xFF);
    queue_bytes(l, hdr, 2);
    if (n != 0xFFFF)
        queue_bytes(l, l->line, l->len);
    l->lines++;
    l->len = 0;
}

static void flush_input(ldisc *l)
{
    l->len = 0;
    l->qlen = 0;
    l->lines = 0;
    l->lnext = 0;
}

static void input_one(ldisc *l, int c)
{
    ld_flag lf = l->t.c_lflag, inf = l->t.c_iflag;
    if (inf & LD_ISTRIP)
        c &= 0x7F;
    if (l->lnext) {
        l->lnext = 0;
        goto ordinary;
    }
    if (c == '\r') {
        if (inf & LD_IGNCR)
            return;
        if (inf & LD_ICRNL)
            c = '\n';
    } else if (c == '\n' && (inf & LD_INLCR)) {
        c = '\r';
    }
    if (inf & LD_IXON) {
        if (is_cc(l, c, LD_VSTOP)) {
            l->stopped = 1;
            return;
        }
        if (is_cc(l, c, LD_VSTART)) {
            l->stopped = 0;
            return;
        }
        if (inf & LD_IXANY)
            l->stopped = 0;
    }
    if (lf & LD_ISIG) {
        int sig = is_cc(l, c, LD_VINTR) ? LD_SIGINT : is_cc(l, c, LD_VQUIT) ? LD_SIGQUIT
                : is_cc(l, c, LD_VSUSP) ? LD_SIGTSTP : 0;
        if (sig) {
            if (!(lf & LD_NOFLSH))
                flush_input(l);
            echo_char(l, c);
            if (l->signal)
                l->signal(l->user, sig);
            return;
        }
    }
    if ((lf & LD_IEXTEN) && is_cc(l, c, LD_VLNEXT)) {
        l->lnext = 1;
        return;
    }
    if (lf & LD_ICANON) {
        if (is_cc(l, c, LD_VERASE)) {
            erase_char(l);
            return;
        }
        if (is_cc(l, c, LD_VKILL)) {
            kill_line(l);
            return;
        }
        if ((lf & LD_IEXTEN) && is_cc(l, c, LD_VWERASE)) {
            erase_word(l);
            return;
        }
        if ((lf & LD_IEXTEN) && is_cc(l, c, LD_VREPRINT)) {
            reprint(l);
            return;
        }
        if (is_cc(l, c, LD_VEOF)) {
            commit_line(l, 1); /* the line so far without a newline, or an EOF */
            return;
        }
        if (c == '\n' || is_cc(l, c, LD_VEOL) || is_cc(l, c, LD_VEOL2)) {
            if (l->len < LD_LINE)
                l->line[l->len++] = (unsigned char)c;
            echo_char(l, c);
            commit_line(l, 0);
            return;
        }
    }
ordinary:
    if (lf & LD_ICANON) {
        if (l->len >= LD_LINE - 1)
            return; /* the line is full: the byte is lost (IMAXBEL would ring) */
        l->line[l->len++] = (unsigned char)c;
    } else {
        unsigned char b = (unsigned char)c;
        queue_bytes(l, &b, 1);
    }
    echo_char(l, c);
}

void ld_input(ldisc *l, const unsigned char *b, int n)
{
    int i;
    for (i = 0; i < n; i++)
        input_one(l, b[i]);
}

/* The queue as raw bytes (the line headers taken out), for a switch to
 * non-canonical mode; and the reverse, all of it one line. */
static void queue_to_raw(ldisc *l)
{
    unsigned char tmp[LD_QUEUE];
    int i = 0, n = 0;
    while (i + 2 <= l->qlen) {
        int len = (l->q[i] << 8) | l->q[i + 1];
        i += 2;
        if (len == 0xFFFF)
            continue;
        memcpy(tmp + n, l->q + i, len);
        n += len;
        i += len;
    }
    memcpy(l->q, tmp, n);
    l->qlen = n;
    l->lines = 0;
}

static void queue_to_lines(ldisc *l)
{
    unsigned char tmp[LD_QUEUE];
    int n = l->qlen;
    if (!n)
        return;
    if (n > LD_QUEUE - 2)
        n = LD_QUEUE - 2;
    memcpy(tmp, l->q, n);
    l->q[0] = (unsigned char)(n >> 8);
    l->q[1] = (unsigned char)(n & 0xFF);
    memcpy(l->q + 2, tmp, n);
    l->qlen = n + 2;
    l->lines = 1;
}

void ld_set(ldisc *l, const vt_termios *t, int action)
{
    int was_canon = (l->t.c_lflag & LD_ICANON) != 0, canon = (t->c_lflag & LD_ICANON) != 0;
    l->t = *t;
    if ((action & 0xF) == LD_TCSAFLUSH) {
        flush_input(l);
        return;
    }
    if (was_canon && !canon) {
        queue_to_raw(l);
        queue_bytes(l, l->line, l->len); /* the unfinished line is input now */
        l->len = 0;
    } else if (!was_canon && canon) {
        queue_to_lines(l);
    }
}

int ld_read_ready(const ldisc *l)
{
    if (l->t.c_lflag & LD_ICANON)
        return l->lines > 0;
    return l->qlen >= l->t.c_cc[LD_VMIN] || l->t.c_cc[LD_VMIN] == 0;
}

int ld_read_action(const ldisc *l, int timer_fired, int *arm)
{
    int canon = (l->t.c_lflag & LD_ICANON) != 0;
    int vmin = l->t.c_cc[LD_VMIN], vtime = l->t.c_cc[LD_VTIME];
    *arm = 0;
    if (ld_read_ready(l) && (canon || vmin > 0 || l->qlen > 0))
        return LD_RD_TAKE;
    if (canon)
        return LD_RD_WAIT;
    if (vmin == 0 && vtime == 0)
        return LD_RD_ZERO;
    if (vtime > 0 && timer_fired)
        return LD_RD_TAKE;
    /* VTIME counts from now (VMIN 0) or from the first byte */
    if (vtime > 0 && (vmin == 0 || l->qlen > 0))
        *arm = 1;
    return LD_RD_WAIT;
}

int ld_input_pending(const ldisc *l)
{
    return (l->t.c_lflag & LD_ICANON) ? l->lines > 0 : l->qlen > 0;
}

long ld_read(ldisc *l, unsigned char *buf, long max, int *eof)
{
    *eof = 0;
    if (l->t.c_lflag & LD_ICANON) {
        int len, take;
        if (!l->lines || l->qlen < 2)
            return -1;
        len = (l->q[0] << 8) | l->q[1];
        if (len == 0xFFFF) {
            memmove(l->q, l->q + 2, l->qlen - 2);
            l->qlen -= 2;
            l->lines--;
            *eof = 1;
            return 0;
        }
        take = len < max ? len : (int)max;
        memcpy(buf, l->q + 2, take);
        if (take < len) {
            /* the rest of the line stays for the next read */
            int rest = len - take;
            memmove(l->q + 2, l->q + 2 + take, l->qlen - 2 - take);
            l->q[0] = (unsigned char)(rest >> 8);
            l->q[1] = (unsigned char)(rest & 0xFF);
            l->qlen -= take;
        } else {
            memmove(l->q, l->q + 2 + len, l->qlen - 2 - len);
            l->qlen -= 2 + len;
            l->lines--;
        }
        return take;
    }
    if (!l->qlen)
        return -1;
    {
        long take = l->qlen < max ? l->qlen : max;
        memcpy(buf, l->q, take);
        memmove(l->q, l->q + take, l->qlen - take);
        l->qlen -= (int)take;
        return take;
    }
}

long ld_output(const vt_termios *t, const unsigned char *in, long n, unsigned char *out, int *col)
{
    long i, o = 0;
    if (!(t->c_oflag & LD_OPOST)) {
        memcpy(out, in, n);
        return n;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = in[i];
        if (c == '\n' && (t->c_oflag & LD_ONLCR)) {
            out[o++] = '\r';
            out[o++] = '\n';
            *col = 0;
        } else if (c == '\t' && (t->c_oflag & LD_OXTABS)) {
            do
                out[o++] = ' ';
            while (++*col & 7);
        } else if (c == 0x04 && (t->c_oflag & LD_ONOEOT)) {
            continue;
        } else {
            out[o++] = c;
            if (c == '\r')
                *col = 0;
            else if (c == '\b')
                *col = *col > 0 ? *col - 1 : 0;
            else if (c >= 0x20 && (c & 0xC0) != 0x80)
                ++*col;
        }
    }
    return o;
}
