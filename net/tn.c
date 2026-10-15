/* uptelnet's telnet protocol: see tn.h. */
#include "tn.h"
#include "../llm/sha256.h"

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
static int us_ok(int opt, const tn *t)
{
    return opt == TN_BINARY || opt == TN_SGA || opt == TN_TTYPE || opt == TN_NAWS || (opt == TN_AUTH && t->want_key);
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
    else if (opt == TN_AUTH && t->auth == TN_AUTH_OFF)
        t->auth = TN_AUTH_ASKED;
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
            if (us_ok(opt, t)) {
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

int tn_ct_eq(const unsigned char *a, const unsigned char *b, int n)
{
    unsigned char d = 0;
    int i;
    for (i = 0; i < n; i++)
        d = (unsigned char)(d | (a[i] ^ b[i]));
    return d == 0;
}

/* HMAC(key, label | a | b | c) with the pieces that are not empty */
static void mac(const unsigned char *key, const char *label, const unsigned char *a, int an,
                const unsigned char *b, int bn, const unsigned char *c, int cn, unsigned char out[32])
{
    unsigned char m[96];
    int n = 0, i;
    for (i = 0; label[i]; i++)
        m[n++] = (unsigned char)label[i];
    for (i = 0; i < an; i++)
        m[n++] = a[i];
    for (i = 0; i < bn; i++)
        m[n++] = b[i];
    for (i = 0; i < cn; i++)
        m[n++] = c[i];
    llm_hmac_sha256(key, TN_KEY_LEN, m, n, out);
}

/* SB AUTH IS 0x75 ID 0 cn [mac], every byte of 255 doubled */
static void auth_is(tn *t)
{
    unsigned char m[32], b[2 * (TN_ID_MAX + 1 + 16 + 32) + 8], body[TN_ID_MAX + 1 + 16 + 32];
    int n = 0, k = 0, i, idn = 0;
    if (!t->pairing) {
        while (t->id[idn])
            idn++;
        mac(t->key, "upt1-client", t->sn, 16, t->cn, 16, (const unsigned char *)t->id, idn, m);
    }
    for (i = 0; i < idn; i++)
        body[k++] = (unsigned char)t->id[i];
    body[k++] = 0;
    for (i = 0; i < 16; i++)
        body[k++] = t->cn[i];
    if (!t->pairing)
        for (i = 0; i < 32; i++)
            body[k++] = m[i];
    b[n++] = TN_IAC;
    b[n++] = TN_SB;
    b[n++] = TN_AUTH;
    b[n++] = TN_AUTH_IS;
    b[n++] = TN_AUTH_TYPE;
    for (i = 0; i < k; i++) {
        b[n++] = body[i];
        if (body[i] == TN_IAC)
            b[n++] = TN_IAC;
    }
    b[n++] = TN_IAC;
    b[n++] = TN_SE;
    t->out(t->user, b, n);
    t->auth = TN_AUTH_SENT;
}

static void auth_to(tn *t, int state)
{
    t->auth = state;
    if (t->event)
        t->event(t->user, state, state == TN_AUTH_PAIRED ? t->id : 0, state == TN_AUTH_PAIRED ? t->key : 0);
}

/* what the server said about the login: sb = AUTH REPLY 0x75 code ... */
static void auth_reply(tn *t)
{
    int code = t->sb[3], i;
    if (code == TN_AUTH_ACCEPT && t->auth == TN_AUTH_SENT && !t->pairing && t->sblen == 4 + 32) {
        unsigned char want[32];
        mac(t->key, "upt1-server", t->sn, 16, t->cn, 16, 0, 0, want);
        auth_to(t, tn_ct_eq(want, t->sb + 4, 32) ? TN_AUTH_OK : TN_AUTH_BAD_SERVER);
    } else if (code == TN_AUTH_ACCEPT && t->auth == TN_AUTH_SENT) {
        auth_to(t, TN_AUTH_BAD_SERVER);     /* accepting a mac that was never sent, or a short proof */
    } else if (code == TN_AUTH_REJECT && t->auth == TN_AUTH_SENT) {
        auth_to(t, TN_AUTH_REJECTED);
    } else if (code == TN_AUTH_PAIR && t->pairing && t->auth == TN_AUTH_SENT) {
        /* ID 0 K[32]: only a client that asked takes a key */
        int idn = 0;
        while (4 + idn < t->sblen && t->sb[4 + idn])
            idn++;
        if (idn >= 1 && idn <= TN_ID_MAX && t->sblen == 4 + idn + 1 + TN_KEY_LEN) {
            for (i = 0; i < idn; i++)
                t->id[i] = (char)t->sb[4 + i];
            t->id[idn] = 0;
            for (i = 0; i < TN_KEY_LEN; i++)
                t->key[i] = t->sb[4 + idn + 1 + i];
            t->pairing = 0;
            auth_to(t, TN_AUTH_PAIRED);
        }
    }
}

static void subneg(tn *t)
{
    if (t->sbover) {
        t->sbover = 0;
        return;
    }
    if (t->sblen >= 2 && t->sb[0] == TN_TTYPE && t->sb[1] == TN_TTYPE_SEND && t->us[TN_TTYPE] == TN_YES)
        ttype_is(t);
    else if (t->sblen >= 3 && t->sb[0] == TN_AUTH && t->us[TN_AUTH] == TN_YES && t->sb[2] == TN_AUTH_TYPE) {
        if (t->sb[1] == TN_AUTH_SEND && t->sblen == 3 + 16 && t->auth == TN_AUTH_ASKED) {
            int i;
            for (i = 0; i < 16; i++)
                t->sn[i] = t->sb[3 + i];
            auth_is(t);
        } else if (t->sb[1] == TN_AUTH_REPLY && t->sblen >= 4) {
            /* the reply's fields start after "AUTH REPLY 0x75 code": index 4 */
            auth_reply(t);
        }
    }
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
    t->sblen = t->sbover = 0;
    t->cr = 0;
    t->blen = 0;
    t->esc = t->nparm = t->alt = 0;
    t->parm[0] = 0;
    t->auth = TN_AUTH_OFF;
    t->want_key = t->pairing = 0;
    t->id[0] = 0;
    t->event = 0;
}

void tn_set_key(tn *t, const char *id, const unsigned char *key, const unsigned char *cn,
                void (*event)(void *user, int state, const char *id, const unsigned char *key))
{
    int i;
    t->want_key = 1;
    t->pairing = !id || !id[0];
    for (i = 0; id && id[i] && i < TN_ID_MAX; i++)
        t->id[i] = id[i];
    t->id[i] = 0;
    for (i = 0; i < TN_KEY_LEN; i++)
        t->key[i] = t->pairing || !key ? 0 : key[i];
    for (i = 0; i < 16; i++)
        t->cn[i] = cn[i];
    t->event = event;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int tn_key_parse(const char *text, char *id, unsigned char *key)
{
    int n = 0, i;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    while (text[n] && text[n] != ' ' && text[n] != '\t' && text[n] != '\r' && text[n] != '\n') {
        char c = text[n];
        if (n >= TN_ID_MAX || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                c == '.' || c == '_' || c == '-'))
            return 0;
        id[n] = c;
        n++;
    }
    if (n == 0 || text[n] != ' ')
        return 0;
    id[n] = 0;
    text += n;
    while (*text == ' ' || *text == '\t')
        text++;
    for (i = 0; i < TN_KEY_LEN; i++) {
        int hi = hexval(text[2 * i]), lo = hi < 0 ? -1 : hexval(text[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        key[i] = (unsigned char)(hi * 16 + lo);
    }
    return text[2 * TN_KEY_LEN] == 0 || text[2 * TN_KEY_LEN] == '\r' || text[2 * TN_KEY_LEN] == '\n' ||
           text[2 * TN_KEY_LEN] == ' ';
}

int tn_key_format(const char *id, const unsigned char *key, char *out)
{
    static const char hex[] = "0123456789abcdef";
    int n = 0, i;
    for (i = 0; id[i]; i++)
        out[n++] = id[i];
    out[n++] = ' ';
    for (i = 0; i < TN_KEY_LEN; i++) {
        out[n++] = hex[key[i] >> 4];
        out[n++] = hex[key[i] & 15];
    }
    out[n++] = '\n';
    out[n] = 0;
    return n;
}

int tn_auth(const tn *t)
{
    return t->auth;
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
        if (t->auth == TN_AUTH_BAD_SERVER)
            break;              /* a server that failed its proof is not listened to again */
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
                t->sbover = 0;
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
            else
                t->sbover = 1;
            break;
        case S_SB_IAC:
            if (c == TN_SE) {
                t->st = S_DATA;
                subneg(t);
            } else {
                if (c == TN_IAC && t->sblen < (int)sizeof(t->sb))
                    t->sb[t->sblen++] = c;
                else if (c == TN_IAC)
                    t->sbover = 1;
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

void tn_session_key(const unsigned char *k, int klen, const unsigned char *sn, int snlen, const unsigned char *cn,
                    int cnlen, char hex[65])
{
    static const char label[] = "wasabi-session";
    static const char digits[] = "0123456789abcdef";
    unsigned char m[14 + 64], out[32];
    int n = 0, i;
    hex[0] = 0;
    if (snlen < 0 || cnlen < 0 || snlen + cnlen > 64)
        return;
    for (i = 0; label[i]; i++)
        m[n++] = (unsigned char)label[i];
    for (i = 0; i < snlen; i++)
        m[n++] = sn[i];
    for (i = 0; i < cnlen; i++)
        m[n++] = cn[i];
    llm_hmac_sha256(k, klen, m, n, out);
    for (i = 0; i < 32; i++) {
        hex[i * 2] = digits[out[i] >> 4];
        hex[i * 2 + 1] = digits[out[i] & 15];
    }
    hex[64] = 0;
}
