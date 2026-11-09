/* RIPscrip 1.54 byte parser (RIP-1). Pure C: no allocation, no vt_term, no
 * platform header, so DCTelnet can compile it on its own (D18).
 *
 * Grammar, from the RIP-0 spec check (RIPSCRIP.DOC 1.53 text) and the fixtures
 * in tests/rip/:
 *   - a command line starts with `!|` at column 0, or `|` after a ^A (SOH) or
 *     ^B (STX) anywhere in a line; commands are separated by `|`; a CR or LF
 *     ends the line; text after a command's last parameter, up to the next
 *     `|`, is ignored; a line without the start is text;
 *   - `\` before CR/LF continues the line (a CRLF or LFCR pair counts once),
 *     `\!` `\|` `\\` are literal characters, so `\\\` + newline is a literal
 *     backslash followed by a continuation;
 *   - after `|`: level digits (the first is the level, the rest a sub-level,
 *     up to nine digits), then the command character (any non-digit);
 *   - parameters are base-36 numbers of a FIXED width per command and per
 *     parameter (the table below); after the numbers a command may carry a text
 *     tail (`t`) or a counted list of coordinate pairs (`P`).
 * A malformed command (a short parameter list, a character that is not a base-36
 * digit, a pair count over RIP_MAXARGS) is dropped and counted; a (level, letter)
 * missing from the table is counted as unknown. */
#include <stddef.h>
#include <string.h>
#include "rip.h"

enum { M_TEXT, M_BOL, M_BANG, M_SOH, M_CMD };
enum { PH_LEVEL, PH_NUM, PH_TEXT, PH_IGNORE };
enum { B_OK, B_BAD, B_UNKNOWN };

/* width codes: 1-9 a base-36 number of that many digits, t the text tail,
 * P a count (the last number read) times two numbers of width 2 */
struct rip_def {
    char level, ch;
    const char *wid;
};

static const struct rip_def defs[] = {
    /* level 0: windows, cursor, colour */
    { 0, 'w', "222211" }, { 0, 'v', "2222" }, { 0, '*', "" }, { 0, 'e', "" }, { 0, 'E', "" },
    { 0, 'g', "22" }, { 0, 'H', "" }, { 0, '>', "" }, { 0, 'c', "2" },
    { 0, 'Q', "2222222222222222" }, { 0, 'a', "22" }, { 0, 'W', "2" }, { 0, '=', "242" },
    { 0, 'S', "22" }, { 0, 's', "222222222" }, { 0, 'Y', "2222" },
    /* text */
    { 0, '@', "22t" }, { 0, 'T', "t" },
    /* drawing */
    { 0, 'X', "22" }, { 0, 'm', "22" }, { 0, 'L', "2222" }, { 0, 'R', "2222" }, { 0, 'B', "2222" },
    { 0, 'C', "222" }, { 0, 'O', "222222" }, { 0, 'o', "2222" }, { 0, 'A', "22222" },
    { 0, 'V', "222222" }, { 0, 'I', "22222" }, { 0, 'i', "222222" }, { 0, 'Z', "222222222" },
    { 0, 'P', "2P" }, { 0, 'p', "2P" }, { 0, 'l', "2P" }, { 0, 'F', "222" }, { 0, '#', "" },
    /* level 1: mouse, text regions, clipboard, buttons, files */
    { 1, 'M', "22222" "11" "5" "t" }, { 1, 'K', "" }, { 1, 'T', "22222" }, { 1, 't', "1t" },
    { 1, 'E', "" }, { 1, 'C', "22221" }, { 1, 'P', "2221" }, { 1, 'W', "1t" },
    { 1, 'I', "222122t" },
    { 1, 'B', "222" "4" "2" "222222222" }, /* res:6 after the corner colour is ignored */
    { 1, 'U', "2222" "2" "1" "1" "t" }, { 1, 'D', "32t" }, { 1, 'G', "222222" },
    { 1, 'R', "8t" }, { 1, 'F', "24t" }, { 1, 27, "13t" },
    /* level 9: block transfer */
    { 9, 27, "11" "2" "4" "t" }
};

void rip_init(struct rip_state *st)
{
    memset(st, 0, sizeof(*st));
    st->mode = M_BOL;
}

long rip_commands(const struct rip_state *st) { return st->n_cmd; }
long rip_errors(const struct rip_state *st) { return st->n_err; }
long rip_unknown(const struct rip_state *st) { return st->n_unknown; }

static int digit36(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    return -1;
}

static void begin(struct rip_state *st)
{
    st->c.level = st->c.sub = 0;
    st->c.ch = -1;
    st->c.nargs = st->c.ntext = st->c.truncated = 0;
    st->phase = PH_LEVEL;
    st->ndig = 0;
    st->bad = B_OK;
    st->wid = NULL;
    st->wi = st->wk = st->pairs = 0;
    st->val = 0;
    st->bs = 0;
}

/* the width of the number being read */
static int curw(const struct rip_state *st)
{
    return st->pairs > 0 ? 2 : st->wid[st->wi] - '0';
}

/* the phase for what follows wid[wi] (after a number is complete or at the start) */
static void advance(struct rip_state *st)
{
    char w = st->wid[st->wi];
    st->wk = 0;
    st->val = 0;
    if (w == '\0') {
        st->phase = PH_IGNORE;
    } else if (w == 't') {
        st->phase = PH_TEXT;
    } else if (w == 'P') {
        long n = st->c.nargs ? st->c.arg[st->c.nargs - 1] : 0;
        if (n < 0 || 2 * n > RIP_MAXARGS - st->c.nargs) {
            st->bad = B_BAD;
            st->phase = PH_IGNORE;
        } else if (n == 0) {
            st->phase = PH_IGNORE;
        } else {
            st->pairs = (int)(2 * n);
            st->phase = PH_NUM;
        }
    } else {
        st->phase = PH_NUM;
    }
}

static void put(struct rip_state *st, int c)
{
    int d;
    switch (st->phase) {
    case PH_LEVEL:
        d = (c >= '0' && c <= '9') ? c - '0' : -1;
        if (d >= 0) {
            if (st->ndig >= 9) {
                st->bad = B_BAD;
                st->phase = PH_IGNORE;
            } else if (st->ndig++ == 0) {
                st->c.level = d;
            } else {
                st->c.sub = st->c.sub * 10 + d;
            }
        } else {
            size_t i;
            st->c.ch = c;
            for (i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
                if (defs[i].level == st->c.level && defs[i].ch == c)
                    break;
            if (i == sizeof(defs) / sizeof(defs[0])) {
                st->bad = B_UNKNOWN;
                st->phase = PH_IGNORE;
            } else {
                st->wid = defs[i].wid;
                st->wi = 0;
                advance(st);
            }
        }
        break;
    case PH_NUM:
        d = digit36(c);
        if (d < 0) {
            st->bad = B_BAD;
            st->phase = PH_IGNORE;
            break;
        }
        st->val = st->val * 36 + d;
        if (++st->wk == curw(st)) {
            st->c.arg[st->c.nargs++] = st->val;
            if (st->pairs > 0) {
                if (--st->pairs == 0)
                    st->phase = PH_IGNORE;
                else
                    st->val = 0, st->wk = 0;
            } else {
                st->wi++;
                advance(st);
            }
        }
        break;
    case PH_TEXT:
        if (st->c.ntext < RIP_TEXT_MAX)
            st->c.text[st->c.ntext++] = (unsigned char)c;
        else
            st->c.truncated = 1;
        break;
    default:
        break;
    }
}

/* a command ended (`|` or end of line) */
static void finish(struct rip_state *st, const struct rip_sinks *sk)
{
    if (st->phase == PH_LEVEL && st->ndig == 0 && st->c.ch < 0)
        return; /* nothing between two separators */
    if (st->phase == PH_LEVEL || st->bad == B_BAD || (st->phase == PH_NUM && st->bad == B_OK)) {
        st->n_err++; /* digits but no letter, a bad digit, a short list */
        return;
    }
    if (st->bad == B_UNKNOWN) {
        st->n_unknown++;
        return;
    }
    st->c.text[st->c.ntext] = 0;
    st->n_cmd++;
    if (sk && sk->cmd)
        sk->cmd(sk->ud, &st->c);
}

static void emit(const struct rip_sinks *sk, const unsigned char *p, long n)
{
    if (n > 0 && sk && sk->text)
        sk->text(sk->ud, p, n);
}

void rip_feed(struct rip_state *st, const unsigned char *p, long n, const struct rip_sinks *sk)
{
    long i, run = (st->mode == M_TEXT || st->mode == M_BOL) ? 0 : -1;
    for (i = 0; i < n; i++) {
        int c = p[i];
        if (st->eol) { /* the second byte of a CRLF / LFCR that ended a command line */
            int e = st->eol;
            st->eol = 0;
            if (c == e) {
                if (run == i)
                    run = i + 1;
                continue;
            }
        }
        switch (st->mode) {
        case M_BOL:
        case M_TEXT:
            if (c == '!' && st->mode == M_BOL) {
                emit(sk, p + run, i - run);
                run = -1;
                st->mode = M_BANG;
            } else if (c == 1 || c == 2) {
                emit(sk, p + run, i - run);
                run = -1;
                st->sohch = c;
                st->mode = M_SOH;
            } else {
                st->mode = (c == '\r' || c == '\n') ? M_BOL : M_TEXT;
            }
            break;
        case M_BANG:
        case M_SOH:
            if (c == '|') {
                begin(st);
                st->mode = M_CMD;
            } else {
                unsigned char held = (unsigned char)(st->mode == M_BANG ? '!' : st->sohch);
                emit(sk, &held, 1);
                st->mode = M_TEXT;
                run = i;
                i--; /* look at c again in text mode */
            }
            break;
        case M_CMD:
            if (st->bs == 1) {
                if (c == '!' || c == '|' || c == '\\') {
                    st->bs = 0;
                    put(st, c);
                    break;
                }
                if (c == '\r' || c == '\n') {
                    st->bs = 2;
                    st->sohch = c;
                    break;
                }
                st->bs = 0;
                put(st, '\\'); /* a lone backslash is itself; c is looked at below */
            } else if (st->bs == 2) {
                st->bs = 0;
                if ((c == '\r' || c == '\n') && c != st->sohch)
                    break;
            }
            if (c == '\\') {
                st->bs = 1;
            } else if (c == '|') {
                finish(st, sk);
                begin(st);
            } else if (c == '\r' || c == '\n') {
                finish(st, sk);
                st->mode = M_BOL;
                st->eol = c == '\r' ? '\n' : '\r';
                run = i + 1;
            } else {
                put(st, c);
            }
            break;
        }
    }
    if (run >= 0)
        emit(sk, p + run, n - run);
}

static int add(char *out, int max, int len, const char *s)
{
    while (*s && len < max - 1)
        out[len++] = *s++;
    return len;
}

static int addnum(char *out, int max, int len, long v)
{
    char t[24], b[2];
    int nb = 0;
    do {
        t[nb++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    b[1] = 0;
    while (nb) {
        b[0] = t[--nb];
        len = add(out, max, len, b);
    }
    return len;
}

int rip_dump(const struct rip_cmd *c, char *out, int max)
{
    char b[24];
    int len = 0, k;
    static const char hex[] = "0123456789ABCDEF";
    b[0] = (char)('0' + c->level);
    b[1] = 0;
    len = add(out, max, len, b);
    if (c->sub) {
        len = add(out, max, len, ".");
        len = addnum(out, max, len, c->sub);
    }
    len = add(out, max, len, " ");
    if (c->ch == 27)
        len = add(out, max, len, "ESC");
    else
        b[0] = (char)c->ch, b[1] = 0, len = add(out, max, len, b);
    for (k = 0; k < c->nargs; k++) {
        len = add(out, max, len, k ? "," : " ");
        len = addnum(out, max, len, c->arg[k]);
    }
    if (c->ntext) {
        len = add(out, max, len, " \"");
        for (k = 0; k < c->ntext; k++) {
            unsigned char ch = c->text[k];
            if (ch < 32 || ch > 126) {
                b[0] = '\\', b[1] = 'x', b[2] = hex[ch >> 4], b[3] = hex[ch & 15], b[4] = 0;
                len = add(out, max, len, b);
            } else {
                b[0] = (char)ch, b[1] = 0;
                len = add(out, max, len, b);
            }
        }
        len = add(out, max, len, "\"");
    }
    out[len] = 0;
    return len;
}
