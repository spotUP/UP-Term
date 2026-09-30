/* The line discipline (tty/ldisc.c): termios input, echo, signals, output. */
#include "harness.h"
#include "../tty/ldisc.h"

static char echoed[512];
static int n_echoed, sigs[8], n_sigs;

static void on_echo(void *u, const unsigned char *s, int n)
{
    (void)u;
    if (n_echoed + n < (int)sizeof(echoed) - 1) {
        memcpy(echoed + n_echoed, s, n);
        n_echoed += n;
        echoed[n_echoed] = 0;
    }
}

static void on_signal(void *u, int sig)
{
    (void)u;
    if (n_sigs < 8)
        sigs[n_sigs++] = sig;
}

static ldisc L;

static void fresh(void)
{
    L.echo = on_echo;
    L.signal = on_signal;
    L.user = 0;
    ld_init(&L);
    n_echoed = 0;
    echoed[0] = 0;
    n_sigs = 0;
}

static void type(const char *s)
{
    ld_input(&L, (const unsigned char *)s, (int)strlen(s));
}

/* What the next read gets, as a string ("<EOF>" at an EOF, "" for nothing). */
static const char *rd(long max)
{
    static char buf[512];
    int eof;
    long n = ld_read(&L, (unsigned char *)buf, max, &eof);
    if (eof)
        return "<EOF>";
    if (n < 0)
        n = 0;
    buf[n] = 0;
    return buf;
}

static void canonical_editing(void)
{
    fresh();
    type("ab\r");
    CHECK_STR(rd(100), "ab\n");                 /* ICRNL */
    CHECK_STR(echoed, "ab\r\n");                /* echoed, ONLCR */
    fresh();
    type("abc\x7f\r");
    CHECK_STR(rd(100), "ab\n");
    CHECK_STR(echoed, "abc\b \b\r\n");          /* ECHOE rubs out */
    fresh();
    type("abc\x15xy\r");
    CHECK_STR(rd(100), "xy\n");                 /* VKILL */
    fresh();
    type("one two\x17three\r");
    CHECK_STR(rd(100), "one three\n");          /* VWERASE */
    fresh();
    type("\xc3\xa9\x7f" "x\r");
    CHECK_STR(rd(100), "x\n");                  /* a UTF-8 character erased whole */
    fresh();
    type("ab");
    CHECK_INT(ld_read_ready(&L), 0);            /* no line yet */
    CHECK_STR(rd(100), "");
    type("\x04");
    CHECK_STR(rd(100), "ab");                   /* VEOF: the line without a newline */
    type("\x04");
    CHECK_STR(rd(100), "<EOF>");                /* VEOF on an empty line */
    fresh();
    type("abcd\r");
    CHECK_STR(rd(2), "ab");                     /* a read smaller than the line */
    CHECK_STR(rd(100), "cd\n");
    fresh();
    type("x\x16\x03y\r");
    CHECK_STR(rd(100), "x\x03y\n");             /* VLNEXT: ^C taken literally */
    CHECK_INT(n_sigs, 0);
    CHECK_STR(echoed, "x^Cy\r\n");
    fresh();
    type("ab\x12");
    CHECK_STR(echoed, "ab^R\r\nab");            /* VREPRINT */
}

static void signals(void)
{
    fresh();
    type("ab\x03");
    CHECK_INT(n_sigs, 1);
    CHECK_INT(sigs[0], LD_SIGINT);
    CHECK_STR(echoed, "ab^C");
    type("c\r");
    CHECK_STR(rd(100), "c\n");                  /* the line before ^C was flushed */
    fresh();
    type("\x1c\x1a");
    CHECK_INT(n_sigs, 2);
    CHECK_INT(sigs[0], LD_SIGQUIT);
    CHECK_INT(sigs[1], LD_SIGTSTP);
    fresh();
    L.t.c_lflag |= LD_NOFLSH;
    type("ab\x03" "c\r");
    CHECK_STR(rd(100), "abc\n");                /* NOFLSH keeps it */
    fresh();
    L.t.c_lflag &= ~(ld_flag)LD_ISIG;
    type("\x03\r");
    CHECK_INT(n_sigs, 0);
    CHECK_STR(rd(100), "\x03\n");               /* without ISIG ^C is data */
    fresh();
    type("\x13");
    CHECK_INT(L.stopped, 1);                    /* IXON */
    type("\x11");
    CHECK_INT(L.stopped, 0);
}

static void non_canonical(void)
{
    vt_termios t;
    fresh();
    t = L.t;
    t.c_lflag &= ~(ld_flag)(LD_ICANON | LD_ECHO);
    ld_set(&L, &t, LD_TCSANOW);
    type("x");
    CHECK_INT(ld_read_ready(&L), 1);            /* VMIN 1 */
    CHECK_STR(rd(100), "x");
    CHECK_STR(echoed, "");                      /* no ECHO */
    t.c_cc[LD_VMIN] = 3;
    ld_set(&L, &t, LD_TCSANOW);
    type("ab");
    CHECK_INT(ld_read_ready(&L), 0);
    type("c");
    CHECK_INT(ld_read_ready(&L), 1);
    CHECK_STR(rd(100), "abc");
    /* a password prompt: canonical without echo */
    fresh();
    t = L.t;
    t.c_lflag &= ~(ld_flag)LD_ECHO;
    ld_set(&L, &t, LD_TCSANOW);
    type("secret\r");
    CHECK_STR(rd(100), "secret\n");
    CHECK_STR(echoed, "");
    /* an unfinished line becomes input when canonical mode ends */
    fresh();
    type("ab");
    t = L.t;
    t.c_lflag &= ~(ld_flag)LD_ICANON;
    ld_set(&L, &t, LD_TCSANOW);
    CHECK_STR(rd(100), "ab");
    /* TCSAFLUSH drops what was typed */
    fresh();
    type("typed ahead\r");
    ld_set(&L, &L.t, LD_TCSAFLUSH);
    CHECK_STR(rd(100), "");
}

static void output(void)
{
    vt_termios t;
    unsigned char out[64];
    int col = 0;
    long n;
    ld_defaults(&t);
    n = ld_output(&t, (const unsigned char *)"a\nb", 3, out, &col);
    CHECK_INT(n, 4);
    CHECK_INT(memcmp(out, "a\r\nb", 4), 0);     /* ONLCR */
    t.c_oflag &= ~(ld_flag)LD_OPOST;
    n = ld_output(&t, (const unsigned char *)"a\nb", 3, out, &col);
    CHECK_INT(n, 3);                            /* no OPOST: untouched (vttest m1 s04) */
    t.c_oflag = LD_OPOST | LD_OXTABS;
    col = 0;
    n = ld_output(&t, (const unsigned char *)"ab\tc", 4, out, &col);
    CHECK_INT(n, 9);
    CHECK_INT(memcmp(out, "ab      c", 9), 0);  /* OXTABS to column 8 */
}

/* How a waiting read is served (XCON: and PTY: share this). */
static void read_action(void)
{
    vt_termios t;
    int arm;
    fresh();
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_WAIT);  /* canonical, no line */
    CHECK_INT(arm, 0);
    type("ab");
    CHECK_INT(ld_input_pending(&L), 0);                  /* a line is not finished */
    type("\r");
    CHECK_INT(ld_input_pending(&L), 1);
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_TAKE);
    /* VMIN 0 VTIME 0: a poll */
    fresh();
    t = L.t;
    t.c_lflag &= ~(ld_flag)(LD_ICANON | LD_ECHO);
    t.c_cc[LD_VMIN] = 0;
    t.c_cc[LD_VTIME] = 0;
    ld_set(&L, &t, LD_TCSANOW);
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_ZERO);
    type("x");
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_TAKE);
    /* VMIN 0 VTIME 5: the timer starts at once, its end returns what is there */
    fresh();
    t.c_cc[LD_VTIME] = 5;
    ld_set(&L, &t, LD_TCSANOW);
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_WAIT);
    CHECK_INT(arm, 1);
    CHECK_INT(ld_read_action(&L, 1, &arm), LD_RD_TAKE);
    /* VMIN 2 VTIME 5: the timer starts at the first byte */
    fresh();
    t.c_cc[LD_VMIN] = 2;
    ld_set(&L, &t, LD_TCSANOW);
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_WAIT);
    CHECK_INT(arm, 0);
    type("a");
    CHECK_INT(ld_input_pending(&L), 1);
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_WAIT);
    CHECK_INT(arm, 1);
    type("b");
    CHECK_INT(ld_read_action(&L, 0, &arm), LD_RD_TAKE);
}

/* FIONREAD: what a read would get now. libevent sizes its read with it; 1
 * at a time split a terminal's 9-byte answer (tmux's theme report) past
 * tmux's escape timeout, and the half went to the pane as keys. */
static void nread(void)
{
    vt_termios t;
    fresh();
    CHECK_INT(ld_nread(&L), 0);
    type("ab");
    CHECK_INT(ld_nread(&L), 0);                 /* ICANON: no line yet */
    type("\r");
    CHECK_INT(ld_nread(&L), 3);                 /* "ab\n" */
    type("xyz\r");
    CHECK_INT(ld_nread(&L), 3);                 /* the first line only */
    CHECK_STR(rd(100), "ab\n");
    CHECK_INT(ld_nread(&L), 4);
    fresh();
    t = L.t;
    t.c_lflag &= ~(ld_flag)(LD_ICANON | LD_ECHO);
    ld_set(&L, &t, LD_TCSANOW);
    type("\033[?997;1n");
    CHECK_INT(ld_nread(&L), 9);                 /* raw: everything queued */
    CHECK_STR(rd(100), "\033[?997;1n");
    CHECK_INT(ld_nread(&L), 0);
}

void suite_ldisc(void)
{
    nread();
    read_action();
    canonical_editing();
    signals();
    non_canonical();
    output();
}
