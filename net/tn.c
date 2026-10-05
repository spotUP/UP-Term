/* uptelnet's telnet protocol: see tn.h. */
#include "tn.h"

enum { S_DATA, S_IAC, S_VERB, S_SB, S_SB_IAC };

static void flush(tn *t)
{
    if (t->blen) {
        t->show(t->user, t->buf, t->blen);
        t->blen = 0;
    }
}

/* follows CSI ? 47/1047/1049 h/l in what is shown: tn_goodbye needs to
 * know whether the far side left the alternate screen on */
static void track(tn *t, unsigned char c)
{
    int k;
    switch (t->esc) {
    case 0:
        if (c == 0x1b)
            t->esc = 1;
        return;
    case 1:
        t->esc = c == '[' ? 2 : 0;
        return;
    case 2:
        t->esc = c == '?' ? 3 : 0;
        t->nparm = 0;
        t->parm[0] = 0;
        return;
    default:
        if (c >= '0' && c <= '9') {
            if (t->parm[t->nparm] < 10000)
                t->parm[t->nparm] = t->parm[t->nparm] * 10 + (c - '0');
            return;
        }
        if (c == ';' && t->nparm < 3) {
            t->parm[++t->nparm] = 0;
            return;
        }
        if (c == 'h' || c == 'l')
            for (k = 0; k <= t->nparm; k++)
                if (t->parm[k] == 47 || t->parm[k] == 1047 || t->parm[k] == 1049)
                    t->alt = c == 'h';
        t->esc = c == 0x1b ? 1 : 0;
    }
}

static void put(tn *t, unsigned char c)
{
    track(t, c);
    if (t->blen == (int)sizeof(t->buf))
        flush(t);
    t->buf[t->blen++] = c;
}

static void cmd(tn *t, int verb, int opt)
{
    unsigned char b[3];
    b[0] = TN_IAC;
    b[1] = (unsigned char)verb;
    b[2] = (unsigned char)opt;
    t->out(t->user, b, 3);
}

/* the options we will do, and the ones we want the server to do */
static int us_ok(int opt)
{
    return opt == TN_BINARY || opt == TN_SGA || opt == TN_TTYPE || opt == TN_NAWS;
}

static int him_ok(int opt)
{
    return opt == TN_BINARY || opt == TN_SGA || opt == TN_ECHO;
}

static void naws(tn *t)
{
    unsigned char b[16];
    int n = 0, i;
    unsigned v[4];
    v[0] = ((unsigned)t->cols >> 8) & 0xFF;
    v[1] = (unsigned)t->cols & 0xFF;
    v[2] = ((unsigned)t->rows >> 8) & 0xFF;
    v[3] = (unsigned)t->rows & 0xFF;
    b[n++] = TN_IAC;
    b[n++] = TN_SB;
    b[n++] = TN_NAWS;
    for (i = 0; i < 4; i++) {
        b[n++] = (unsigned char)v[i];
        if (v[i] == TN_IAC)
            b[n++] = TN_IAC; /* a 255 in the data is doubled */
    }
    b[n++] = TN_IAC;
    b[n++] = TN_SE;
    t->out(t->user, b, n);
}

static void ttype_is(tn *t)
{
    unsigned char b[sizeof(t->term) + 6];
    int n = 0, i;
    b[n++] = TN_IAC;
    b[n++] = TN_SB;
    b[n++] = TN_TTYPE;
    b[n++] = TN_TTYPE_IS;
    for (i = 0; t->term[i]; i++)
        b[n++] = (unsigned char)t->term[i];
    b[n++] = TN_IAC;
    b[n++] = TN_SE;
    t->out(t->user, b, n);
}

/* An option of ours came on: what it sends then. */
static void us_on(tn *t, int opt)
{
    if (opt == TN_NAWS)
        naws(t);
}

static void got(tn *t, int verb, int opt)
{
    unsigned char *q;
    if (opt >= TN_NOPTS) {
        /* not one we know: refuse a request, ignore a refusal */
        if (verb == TN_WILL)
            cmd(t, TN_DONT, opt);
        else if (verb == TN_DO)
            cmd(t, TN_WONT, opt);
        return;
    }
    if (verb == TN_WILL || verb == TN_WONT) {
        q = &t->him[opt];
        if (verb == TN_WILL) {
            if (*q == TN_NO) {
                if (him_ok(opt)) {
                    *q = TN_YES;
                    cmd(t, TN_DO, opt);
                } else {
                    cmd(t, TN_DONT, opt);
                }
            } else if (*q == TN_WANTNO) {
                *q = TN_NO; /* our DONT answered by WILL: RFC 1143 says NO */
            } else if (*q == TN_WANTYES) {
                *q = TN_YES;
            }
        } else {
            if (*q == TN_YES) {
                *q = TN_NO;
                cmd(t, TN_DONT, opt);
            } else if (*q == TN_WANTNO || *q == TN_WANTYES) {
                *q = TN_NO;
            }
        }
        return;
    }
    q = &t->us[opt];
    if (verb == TN_DO) {
        if (*q == TN_NO) {
            if (us_ok(opt)) {
                *q = TN_YES;
                cmd(t, TN_WILL, opt);
                us_on(t, opt);
            } else {
                cmd(t, TN_WONT, opt);
            }
        } else if (*q == TN_WANTNO) {
            *q = TN_NO;
        } else if (*q == TN_WANTYES) {
            *q = TN_YES;
            us_on(t, opt);
        }
    } else {
        if (*q == TN_YES) {
            *q = TN_NO;
            cmd(t, TN_WONT, opt);
        } else if (*q == TN_WANTNO || *q == TN_WANTYES) {
            *q = TN_NO;
        }
    }
}

static void subneg(tn *t)
{
    if (t->sblen >= 2 && t->sb[0] == TN_TTYPE && t->sb[1] == TN_TTYPE_SEND && t->us[TN_TTYPE] == TN_YES)
        ttype_is(t);
}

void tn_init(tn *t, const char *term, int cols, int rows,
             void (*out)(void *, const unsigned char *, int),
             void (*show)(void *, const unsigned char *, int), void *user)
{
    int i;
    for (i = 0; i < TN_NOPTS; i++)
        t->us[i] = t->him[i] = TN_NO;
    for (i = 0; term && term[i] && i < (int)sizeof(t->term) - 1; i++)
        t->term[i] = term[i];
    t->term[i] = 0;
    t->cols = cols;
    t->rows = rows;
    t->out = out;
    t->show = show;
    t->user = user;
    t->st = S_DATA;
    t->verb = 0;
    t->sblen = 0;
    t->cr = 0;
    t->blen = 0;
    t->esc = t->nparm = t->alt = 0;
    t->parm[0] = 0;
}

void tn_start(tn *t)
{
    static const unsigned char will[] = { TN_NAWS, TN_TTYPE, TN_BINARY };
    static const unsigned char dos[] = { TN_BINARY, TN_SGA };
    int i;
    for (i = 0; i < (int)sizeof(will); i++) {
        t->us[will[i]] = TN_WANTYES;
        cmd(t, TN_WILL, will[i]);
    }
    for (i = 0; i < (int)sizeof(dos); i++) {
        t->him[dos[i]] = TN_WANTYES;
        cmd(t, TN_DO, dos[i]);
    }
}

void tn_recv(tn *t, const unsigned char *b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        unsigned char c = b[i];
        switch (t->st) {
        case S_DATA:
            if (c == TN_IAC) {
                t->st = S_IAC;
                break;
            }
            if (t->cr && c == 0 && t->him[TN_BINARY] != TN_YES) {
                t->cr = 0; /* NVT: CR NUL is a bare CR */
                break;
            }
            t->cr = c == '\r';
            put(t, c);
            break;
        case S_IAC:
            t->st = S_DATA;
            if (c == TN_IAC) {
                t->cr = 0;
                put(t, TN_IAC);
            } else if (c >= TN_WILL && c <= TN_DONT) {
                t->verb = c;
                t->st = S_VERB;
            } else if (c == TN_SB) {
                t->sblen = 0;
                t->st = S_SB;
            }
            /* NOP, GA, DM, AYT...: nothing to do */
            break;
        case S_VERB:
            t->st = S_DATA;
            flush(t); /* what came before a mode change is shown first */
            got(t, t->verb, c);
            break;
        case S_SB:
            if (c == TN_IAC)
                t->st = S_SB_IAC;
            else if (t->sblen < (int)sizeof(t->sb))
                t->sb[t->sblen++] = c;
            break;
        case S_SB_IAC:
            if (c == TN_SE) {
                t->st = S_DATA;
                subneg(t);
            } else {
                if (c == TN_IAC && t->sblen < (int)sizeof(t->sb))
                    t->sb[t->sblen++] = c;
                t->st = S_SB; /* IAC IAC is a 255; anything else is not ours to judge */
            }
            break;
        }
    }
    flush(t);
}

void tn_send(tn *t, const unsigned char *b, int n)
{
    unsigned char o[256];
    int k = 0, i;
    int nvt = t->us[TN_BINARY] != TN_YES;
    int echo = t->him[TN_ECHO] != TN_YES;
    for (i = 0; i < n; i++) {
        if (k > (int)sizeof(o) - 2) {
            t->out(t->user, o, k);
            k = 0;
        }
        o[k++] = b[i];
        if (b[i] == TN_IAC)
            o[k++] = TN_IAC;
        else if (b[i] == '\r' && nvt)
            o[k++] = 0; /* NVT: a CR alone is CR NUL */
    }
    if (k)
        t->out(t->user, o, k);
    if (echo) {
        for (i = 0; i < n; i++) {
            put(t, b[i]);
            if (b[i] == '\r')
                put(t, '\n');
        }
        flush(t);
    }
}

static const char bye_alt[] = "\033[?2026l\033[?1049l";   /* synchronized output off, main screen */
static const char bye_main[] = "\033[?2026l";
static const char bye[] =
    "\0337\033[r\0338"                                /* the whole screen scrolls; DECSTBM homes: kept */
    "\033[0m\033[?25h\033[?7h"                       /* plain text, cursor shown, wrap */
    "\033[?1l\033>"                                  /* cursor and keypad keys normal */
    "\033[?9l\033[?1000l\033[?1002l\033[?1003l"      /* no mouse reports */
    "\033[?1005l\033[?1006l\033[?1015l"
    "\033[?1004l\033[?2004l";                        /* no focus reports, no paste marks */
static char goodbye[sizeof(bye_alt) + sizeof(bye)];

const char *tn_goodbye(const tn *t)
{
    const char *a = t->alt ? bye_alt : bye_main;
    int i = 0, k;
    for (k = 0; a[k]; k++)
        goodbye[i++] = a[k];
    for (k = 0; bye[k]; k++)
        goodbye[i++] = bye[k];
    goodbye[i] = 0;
    return goodbye;
}                        /* no focus reports, no paste marks */

void tn_size(tn *t, int cols, int rows)
{
    if (cols == t->cols && rows == t->rows)
        return;
    t->cols = cols;
    t->rows = rows;
    if (t->us[TN_NAWS] == TN_YES)
        naws(t);
}

int tn_us(const tn *t, int opt)
{
    return opt >= 0 && opt < TN_NOPTS && t->us[opt] == TN_YES;
}

int tn_him(const tn *t, int opt)
{
    return opt >= 0 && opt < TN_NOPTS && t->him[opt] == TN_YES;
}
