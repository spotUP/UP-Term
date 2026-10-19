/* splits_state: ENV:up-term/splits as text and back (splits_state.h). */
#include "splits_state.h"

static int ss_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

/* Does the control-character rule hold: any byte below 0x20 or 0x7f. */
static int ss_has_ctl(const char *s)
{
    for (; *s; s++)
        if ((unsigned char)*s < 0x20 || (unsigned char)*s == 0x7f)
            return 1;
    return 0;
}

static int ss_len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

/* Is the first n bytes of s the word w, in either case. */
static int ss_word(const char *s, int n, const char *w)
{
    int i;
    for (i = 0; i < n; i++)
        if (!w[i] || ss_lower((unsigned char)s[i]) != w[i])
            return 0;
    return w[n] == 0;
}

/* Append text to the output; once something does not fit, everything after
 * it is dropped and ok stays 0, so the caller refuses the whole file. */
typedef struct ss_out {
    char *buf;
    int size, len, ok;
} ss_out;

static void ss_put(ss_out *o, const char *s)
{
    int n = ss_len(s);
    if (!o->ok || o->len + n >= o->size) {
        o->ok = 0;
        return;
    }
    while (*s)
        o->buf[o->len++] = *s++;
    o->buf[o->len] = 0;
}

static void ss_put_pane(ss_out *o, int k, const char *what, const char *val)
{
    char key[16];
    int i = 0;
    key[i++] = 'p'; key[i++] = 'a'; key[i++] = 'n'; key[i++] = 'e';
    if (k >= 10)
        key[i++] = (char)('0' + k / 10);
    key[i++] = (char)('0' + k % 10);
    key[i] = 0;
    ss_put(o, key);
    ss_put(o, what);
    ss_put(o, val);
    ss_put(o, "\n");
}

int splits_state_text(const splits_state *s, char *buf, int size)
{
    int from[LAY_MAX_PANES], dir[LAY_MAX_PANES], leaf[LAY_MAX_PANES];
    ss_out o;
    int k;

    if (size < 1 || s->n < 2 || s->n > LAY_MAX_PANES
        || ss_len(s->layout) >= SPLITS_LAYOUT_MAX
        || lay_steps(s->layout, from, dir, leaf) != s->n)
        return -1;
    o.buf = buf; o.size = size; o.len = 0; o.ok = 1;
    buf[0] = 0;
    ss_put(&o, "[splits]\nlayout = ");
    ss_put(&o, s->layout);
    ss_put(&o, "\n");
    for (k = 0; k < s->n; k++) {
        if (s->cwd[k][0] && ss_len(s->cwd[k]) < SPLITS_CWD_MAX
            && !ss_has_ctl(s->cwd[k]))
            ss_put_pane(&o, k, "-cwd = ", s->cwd[k]);
        if (s->cmd[k][0] && ss_len(s->cmd[k]) < SPLITS_CMD_MAX
            && !ss_has_ctl(s->cmd[k]))
            ss_put_pane(&o, k, "-cmd = ", s->cmd[k]);
    }
    return o.ok ? o.len : -1;
}

/* Set dst (cap bytes) to the n bytes at v, or to empty when they do not fit
 * with the NUL or, for a command, hold a control character. */
static void ss_set(char *dst, int cap, const char *v, int n, int cmd)
{
    int i;
    dst[0] = 0;
    if (n >= cap)
        return;
    if (cmd)
        for (i = 0; i < n; i++)
            if ((unsigned char)v[i] < 0x20 || (unsigned char)v[i] == 0x7f)
                return;
    for (i = 0; i < n; i++)
        dst[i] = v[i];
    dst[n] = 0;
}

/* One line, already trimmed, not a comment: key = value. A pane key whose
 * index is past LAY_MAX_PANES is unknown; one between n and that is dropped
 * after the layout is known (the layout line may come last). */
static void ss_line(splits_state *s, int *have, const char *l, int n)
{
    int eq, ke, vs, ve, k, i;

    for (eq = 0; eq < n && l[eq] != '='; eq++)
        ;
    if (eq >= n)
        return;
    for (ke = eq; ke > 0 && (l[ke - 1] == ' ' || l[ke - 1] == '\t'); ke--)
        ;
    for (vs = eq + 1; vs < n && (l[vs] == ' ' || l[vs] == '\t'); vs++)
        ;
    ve = n;
    if (ss_word(l, ke, "layout")) {
        ss_set(s->layout, SPLITS_LAYOUT_MAX, l + vs, ve - vs, 1);
        *have = s->layout[0] != 0;
        return;
    }
    if (ke < 9 || !ss_word(l, 4, "pane"))
        return;
    i = 4;
    k = 0;
    while (i < ke && l[i] >= '0' && l[i] <= '9' && k < 100)
        k = k * 10 + (l[i++] - '0');
    if (i == 4 || k >= LAY_MAX_PANES)
        return;
    if (ss_word(l + i, ke - i, "-cwd"))
        ss_set(s->cwd[k], SPLITS_CWD_MAX, l + vs, ve - vs, 0);
    else if (ss_word(l + i, ke - i, "-cmd"))
        ss_set(s->cmd[k], SPLITS_CMD_MAX, l + vs, ve - vs, 1);
}

int splits_state_parse(splits_state *s, const char *buf, long len)
{
    int from[LAY_MAX_PANES], dir[LAY_MAX_PANES], leaf[LAY_MAX_PANES];
    int have = 0, n, k;
    long i = 0, e;
    int a, b;

    for (k = 0; k < LAY_MAX_PANES; k++)
        s->cwd[k][0] = s->cmd[k][0] = 0;
    s->layout[0] = 0;
    s->n = 0;
    while (i < len) {
        for (e = i; e < len && buf[e] != '\n'; e++)
            ;
        a = 0;
        b = (int)(e - i);
        if (e - i > 1000)
            b = 1000;   /* longer than any value: only the key matters */
        while (a < b && (buf[i + a] == ' ' || buf[i + a] == '\t'))
            a++;
        while (b > a && (buf[i + b - 1] == ' ' || buf[i + b - 1] == '\t'
                         || buf[i + b - 1] == '\r'))
            b--;
        if (b > a && buf[i + a] != ';' && buf[i + a] != '#'
            && buf[i + a] != '[')
            ss_line(s, &have, buf + i + a, b - a);
        i = e + 1;
    }
    n = have ? lay_steps(s->layout, from, dir, leaf) : -1;
    if (n < 2 || n > LAY_MAX_PANES) {
        s->layout[0] = 0;
        for (k = 0; k < LAY_MAX_PANES; k++)
            s->cwd[k][0] = s->cmd[k][0] = 0;
        return 0;
    }
    for (k = n; k < LAY_MAX_PANES; k++)
        s->cwd[k][0] = s->cmd[k][0] = 0;
    s->n = n;
    return n;
}
