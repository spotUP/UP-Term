/* keys -- see keys.h. */
#include <string.h>
#include "keys.h"

void keys_init(cl_keys *k)
{
    memset(k, 0, sizeof(*k));
    jw_init(&k->pb);
    jw_init(&k->out);
}

void keys_free(cl_keys *k)
{
    jw_free(&k->pb);
    jw_free(&k->out);
    k->n = 0;
}

void keys_feed(cl_keys *k, const char *s, long n)
{
    while (n > 0) {
        long room = (long)sizeof(k->b) - k->n, take = n < room ? n : room;
        if (take <= 0) {
            /* a flood nobody reads: in a paste it is the paste's */
            if (k->paste) {
                jw_raw(&k->pb, (const char *)k->b, k->n);
                k->n = 0;
                continue;
            }
            return;
        }
        memcpy(k->b + k->n, s, (size_t)take);
        k->n += (int)take;
        s += take;
        n -= take;
    }
}

static void eat(cl_keys *k, int n)
{
    if (n > k->n)
        n = k->n;
    memmove(k->b, k->b + n, (size_t)(k->n - n));
    k->n -= n;
}

static int set(cl_key *key, int kind, unsigned long ch, int mods)
{
    memset(key, 0, sizeof(*key));
    key->k = kind;
    key->ch = ch;
    key->mods = mods;
    return 1;
}

/* a key named by its code (kitty's CSI code;mods u, modifyOtherKeys'
 * CSI 27;mods;code ~); mods as KM_* */
static int code_key(cl_key *key, long code, int mods)
{
    if (code == 13 || code == 57414)        /* Return, keypad Enter */
        return set(key, (mods & (KM_SHIFT | KM_ALT | KM_CTRL)) ? K_NEWLINE : K_ENTER, 0, mods);
    if (code == 27)
        return set(key, K_ESC, 0, mods);
    if (code == 9)
        return set(key, (mods & KM_SHIFT) ? K_BTAB : K_TAB, 0, mods);
    if (code == 127 || code == 8)
        return (mods & (KM_ALT | KM_CTRL)) ? set(key, K_ALT, 0x7f, mods) : set(key, K_BS, 0, mods);
    if (code >= 57344 && code <= 63743)
        return 0;                           /* kitty's private-use keys: not ours */
    if ((mods & KM_CTRL) && code == '[')
        return set(key, K_ESC, 0, mods);    /* Ctrl+[ is Escape */
    if ((mods & KM_CTRL) && (code == '_' || code == '-'))
        return set(key, K_CTRL, '_', mods); /* Ctrl+_ / Ctrl+Shift+-: undo */
    if ((mods & KM_CTRL) && ((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')))
        return set(key, K_CTRL, (unsigned long)(code | 0x20), mods);
    if (mods & (KM_ALT | KM_CTRL))
        return set(key, K_ALT, (unsigned long)code, mods);
    if (code < 0x20)
        return 0;
    return set(key, K_CHAR, (unsigned long)code, mods);
}

static int xmods(long p)
{
    int m = p > 1 ? (int)(p - 1) : 0;
    return (m & 1 ? KM_SHIFT : 0) | (m & 2 ? KM_ALT : 0) | (m & 4 ? KM_CTRL : 0);
}

/* a CSI sequence at b[p..]: its length from p (the final included), 0
 * incomplete, -1 malformed */
static int csi_len(const unsigned char *b, int n, int p)
{
    int i;
    for (i = p; i < n && i - p < 40; i++) {
        if (b[i] >= 0x40 && b[i] <= 0x7e)
            return i - p + 1;
        if (b[i] < 0x20 || b[i] > 0x3f)
            return -1;
    }
    return i - p >= 40 ? -1 : 0;
}

static int csi_key(cl_key *key, const unsigned char *s, int len)
{
    long p[4];
    int np = 0, i = 0, inter = 0, priv = 0;
    unsigned char fin = s[len - 1];
    memset(p, 0, sizeof(p));
    if (len > 1 && s[0] >= '<' && s[0] <= '?') {
        priv = 1;
        i = 1;
    }
    for (; i < len - 1; i++) {
        unsigned char c = s[i];
        if (c >= '0' && c <= '9') {
            if (np == 0)
                np = 1;
            if (np <= 4 && p[np - 1] < 100000L)
                p[np - 1] = p[np - 1] * 10 + (c - '0');
        } else if (c == ';') {
            np = np ? np + 1 : 2;
        } else if (c == ':') {
            /* sub-parameters (kitty's alternate keys): skip to the next ';' */
            while (i + 1 < len - 1 && s[i + 1] != ';')
                i++;
        } else if (c >= 0x20 && c <= 0x2f) {
            inter = c;
        }
    }
    if (priv)
        return 0;
    if (inter == ' ') {                     /* the Amiga's Shift+Right / Shift+Left */
        if (fin == '@')
            return set(key, K_RIGHT, 0, KM_SHIFT);
        if (fin == 'A')
            return set(key, K_LEFT, 0, KM_SHIFT);
        return 0;
    }
    switch (fin) {
    case 'A':
        return set(key, K_UP, 0, xmods(p[1]));
    case 'B':
        return set(key, K_DOWN, 0, xmods(p[1]));
    case 'C':
        return set(key, K_RIGHT, 0, xmods(p[1]));
    case 'D':
        return set(key, K_LEFT, 0, xmods(p[1]));
    case 'H':
        return set(key, K_HOME, 0, xmods(p[1]));
    case 'F':
        return set(key, K_END, 0, xmods(p[1]));
    case 'T':
        return set(key, K_UP, 0, KM_SHIFT);   /* Amiga Shift+Up */
    case 'S':
        return set(key, K_DOWN, 0, KM_SHIFT);
    case 'Z':
        return set(key, K_BTAB, 0, KM_SHIFT);
    case 'R':
        if (np >= 2) {
            set(key, K_CPR, 0, 0);
            key->row = (int)p[0];
            key->col = (int)p[1];
            return 1;
        }
        return 0;
    case 'u':
        return code_key(key, p[0], xmods(p[1]));
    case '~':
        switch (p[0]) {
        case 1:
        case 7:
            return set(key, K_HOME, 0, xmods(p[1]));
        case 4:
        case 8:
            return set(key, K_END, 0, xmods(p[1]));
        case 2:
            return set(key, K_INS, 0, 0);
        case 3:
            return set(key, K_DEL, 0, xmods(p[1]));
        case 5:
            return set(key, K_PGUP, 0, 0);
        case 6:
            return set(key, K_PGDN, 0, 0);
        case 27:
            return np >= 3 ? code_key(key, p[2], xmods(p[1])) : 0;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

/* where the paste's end marker is in b, -1 none; *keep: bytes at the end
 * that may be the start of one */
static int paste_end(const unsigned char *b, int n, int *at, int *len)
{
    static const char e7[] = "\033[201~", e8[] = "\233201~";
    int i;
    for (i = 0; i < n; i++) {
        if (b[i] == 0x1b && n - i >= 6 && !memcmp(b + i, e7, 6)) {
            *at = i;
            *len = 6;
            return 1;
        }
        if (b[i] == 0x9b && n - i >= 5 && !memcmp(b + i, e8, 5)) {
            *at = i;
            *len = 5;
            return 1;
        }
    }
    /* a possible start of the marker at the end stays */
    for (i = n - 5 < 0 ? 0 : n - 5; i < n; i++)
        if (b[i] == 0x1b || b[i] == 0x9b) {
            *at = i;
            *len = 0;
            return 0;
        }
    *at = n;
    *len = 0;
    return 0;
}

int keys_next(cl_keys *k, cl_key *key, int idle)
{
    for (;;) {
        unsigned char c;
        int l;
        if (k->paste) {
            int at, len;
            int done = paste_end(k->b, k->n, &at, &len);
            jw_raw(&k->pb, (const char *)k->b, at);
            eat(k, at + len);
            if (!done) {
                if (!idle || !k->n)
                    return 0;
                /* the marker never came: what is held is text */
                jw_raw(&k->pb, (const char *)k->b, k->n);
                k->n = 0;
            }
            k->paste = 0;
            jw_reset(&k->out);
            jw_raw(&k->out, k->pb.p ? k->pb.p : "", k->pb.n);
            jw_reset(&k->pb);
            set(key, K_PASTE, 0, 0);
            key->text = k->out.p ? k->out.p : "";
            key->n = k->out.n;
            return 1;
        }
        if (!k->n)
            return 0;
        c = k->b[0];
        if (c == 0x1b || c == 0x9b) {
            int p = c == 0x9b ? 1 : 2, ok;
            if (c == 0x1b) {
                if (k->n == 1) {
                    if (!idle)
                        return 0;
                    eat(k, 1);
                    return set(key, K_ESC, 0, 0);
                }
                if (k->b[1] == 'O') {
                    if (k->n < 3) {
                        if (!idle)
                            return 0;
                        eat(k, 2);
                        return set(key, K_ALT, 'O', KM_ALT);
                    }
                    c = k->b[2];
                    eat(k, 3);
                    switch (c) {
                    case 'A':
                        return set(key, K_UP, 0, 0);
                    case 'B':
                        return set(key, K_DOWN, 0, 0);
                    case 'C':
                        return set(key, K_RIGHT, 0, 0);
                    case 'D':
                        return set(key, K_LEFT, 0, 0);
                    case 'H':
                        return set(key, K_HOME, 0, 0);
                    case 'F':
                        return set(key, K_END, 0, 0);
                    case 'M':
                        return set(key, K_ENTER, 0, 0);
                    default:
                        continue;
                    }
                }
                if (k->b[1] != '[') {
                    unsigned long cp;
                    if (k->b[1] == 0x1b) {
                        eat(k, 1);
                        return set(key, K_ESC, 0, 0);
                    }
                    if (k->b[1] == '\r' || k->b[1] == '\n') {
                        eat(k, 2);
                        return set(key, K_NEWLINE, 0, KM_ALT);
                    }
                    if (k->b[1] == 0x7f || k->b[1] == 0x08) {
                        eat(k, 2);
                        return set(key, K_ALT, 0x7f, KM_ALT);
                    }
                    l = json_utf8((const char *)k->b + 1, k->n - 1, &cp);
                    if (!l) {
                        l = 1;
                        cp = k->b[1];
                    }
                    eat(k, 1 + l);
                    if (cp < 0x20)
                        continue;
                    return set(key, K_ALT, cp, KM_ALT);
                }
            }
            l = csi_len(k->b, k->n, p);
            if (l == 0) {
                if (!idle)
                    return 0;
                k->n = 0;           /* a sequence cut off: dropped */
                return 0;
            }
            if (l < 0) {
                eat(k, p);
                continue;
            }
            if (l == 4 && !memcmp(k->b + p, "200~", 4)) {
                eat(k, p + l);
                k->paste = 1;
                jw_reset(&k->pb);
                continue;
            }
            ok = csi_key(key, k->b + p, l);
            eat(k, p + l);
            if (ok)
                return 1;
            continue;
        }
        if (c < 0x20 || c == 0x7f) {
            eat(k, 1);
            switch (c) {
            case '\r':
                return set(key, K_ENTER, 0, 0);
            case '\n':
                return set(key, K_NEWLINE, 0, 0);
            case '\t':
                return set(key, K_TAB, 0, 0);
            case 0x7f:
            case 0x08:
                return set(key, K_BS, 0, 0);
            case 0:
                continue;
            case 0x1f:
                return set(key, K_CTRL, '_', KM_CTRL);
            default:
                if (c <= 26)
                    return set(key, K_CTRL, (unsigned long)('a' + c - 1), KM_CTRL);
                continue;
            }
        }
        {
            unsigned long cp;
            l = json_utf8((const char *)k->b, k->n, &cp);
            if (!l) {
                /* a UTF-8 lead whose rest has not come yet */
                int need = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc2 ? 2 : 1;
                if (need > k->n && !idle) {
                    int i, cont = 1;
                    for (i = 1; i < k->n; i++)
                        if ((k->b[i] & 0xc0) != 0x80)
                            cont = 0;
                    if (cont)
                        return 0;
                }
                l = 1;
                cp = c;             /* Latin-1 */
            }
            eat(k, l);
            return set(key, K_CHAR, cp, 0);
        }
    }
}
