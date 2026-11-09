/* RIPscrip mouse fields and buttons (RIP-6), pure C like the other rip_*.c files.
 *
 *  1M  a mouse field: a rectangle with a host command
 *  1U  a button: a field, drawn (surface, bevel, label) in the style the last 1B set
 *  1B  the button style
 *  1K  kills every field
 * Fields and buttons share 128 slots (RIP_MOUSE_MAX); the 129th is rejected. On a click the
 * LAST defined field containing the point wins. Button drawing builds ordinary S, B, c, L
 * and @ commands for rip_gfx_exec, so there is no second drawing path.
 *
 * Host command expansion (rip_expand): `^x` is the control character (^M is CR); a backquote
 * and a $NAME$ variable are recognised and expand to nothing, because the engine has no text
 * variables (UNVERIFIED which names RIPscrip 1.54 defines; a known name would be a lookup
 * here). Anything else is kept as it is. */
#include <string.h>
#include "rip.h"

void rip_mouse_init(struct rip_mouse *m)
{
    memset(m, 0, sizeof(*m));
    m->st.bevel = 2;
    m->st.dfore = 0;
    m->st.bright = 15;
    m->st.dark = 8;
    m->st.surface = 7;
}

int rip_mouse_handles(const struct rip_cmd *c)
{
    return c->level == 1 && (c->ch == 'M' || c->ch == 'K' || c->ch == 'B' || c->ch == 'U');
}

/* `<>label<>command` or `<>icon<>label<>command`: the label and the command */
static void split_text(const struct rip_cmd *c, const unsigned char **lab, int *nlab,
                       const unsigned char **cmd, int *ncmd)
{
    const unsigned char *p = c->text, *e = c->text + c->ntext, *tok[4];
    int len[4], n = 0;
    *lab = p;
    *nlab = 0;
    *cmd = p;
    *ncmd = c->ntext;
    if (c->ntext < 2 || p[0] != '<' || p[1] != '>')
        return;
    p += 2;
    tok[0] = p;
    while (p < e) {
        if (p + 1 < e && p[0] == '<' && p[1] == '>' && n < 2) {
            len[n] = (int)(p - tok[n]);
            n++;
            p += 2;
            tok[n] = p;
        } else {
            p++;
        }
    }
    len[n] = (int)(e - tok[n]);
    n++;
    if (n == 1) { /* only a label */
        *lab = tok[0];
        *nlab = len[0];
        *ncmd = 0;
    } else if (n == 2) {
        *lab = tok[0];
        *nlab = len[0];
        *cmd = tok[1];
        *ncmd = len[1];
    } else { /* icon, label, command: the icon is not drawn here */
        *lab = tok[1];
        *nlab = len[1];
        *cmd = tok[2];
        *ncmd = len[2];
    }
}

static int add_field(struct rip_mouse *m, int x0, int y0, int x1, int y1, int clk, int clr, int hotkey,
                     const unsigned char *cmd, int ncmd)
{
    struct rip_mfield *f;
    int t;
    if (m->n >= RIP_MOUSE_MAX) {
        m->rejected++;
        return 0;
    }
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    f = &m->f[m->n++];
    f->x0 = x0;
    f->y0 = y0;
    f->x1 = x1;
    f->y1 = y1;
    f->clk = clk;
    f->clr = clr;
    f->hotkey = hotkey;
    if (ncmd > RIP_MCMD)
        ncmd = RIP_MCMD;
    memcpy(f->cmd, cmd, (size_t)ncmd);
    f->ncmd = ncmd;
    return 1;
}

static void draw_button(const struct rip_mouse *m, const struct rip_gfx *g0, struct cv *cv, int x0, int y0,
                        int x1, int y1, const unsigned char *lab, int nlab)
{
    struct rip_gfx g = *g0;
    struct rip_cmd c;
    int i, n = m->st.bevel;
    memset(&c, 0, sizeof(c));
    g.vx0 = 0;
    g.vy0 = 0;
    g.vx1 = CV_RIP_W - 1;
    g.vy1 = CV_RIP_H - 1;
    g.wmode = 0;
    g.fdir = 0;
    g.fsize = 1;
    /* surface */
    c.ch = 'S'; c.nargs = 2; c.arg[0] = 1; c.arg[1] = m->st.surface & 15;
    rip_gfx_exec(&g, cv, &c);
    c.ch = 'B'; c.nargs = 4; c.arg[0] = x0; c.arg[1] = y0; c.arg[2] = x1; c.arg[3] = y1;
    rip_gfx_exec(&g, cv, &c);
    /* bevel: bright along the top and left, dark along the bottom and right */
    for (i = 0; i < n && x0 + i < x1 - i && y0 + i < y1 - i; i++) {
        c.ch = 'c'; c.nargs = 1; c.arg[0] = m->st.bright & 15;
        rip_gfx_exec(&g, cv, &c);
        c.ch = 'L'; c.nargs = 4; c.arg[0] = x0 + i; c.arg[1] = y0 + i; c.arg[2] = x1 - i; c.arg[3] = y0 + i;
        rip_gfx_exec(&g, cv, &c);
        c.arg[0] = x0 + i; c.arg[1] = y0 + i; c.arg[2] = x0 + i; c.arg[3] = y1 - i;
        rip_gfx_exec(&g, cv, &c);
        c.ch = 'c'; c.nargs = 1; c.arg[0] = m->st.dark & 15;
        rip_gfx_exec(&g, cv, &c);
        c.ch = 'L'; c.nargs = 4; c.arg[0] = x0 + i; c.arg[1] = y1 - i; c.arg[2] = x1 - i; c.arg[3] = y1 - i;
        rip_gfx_exec(&g, cv, &c);
        c.arg[0] = x1 - i; c.arg[1] = y0 + i; c.arg[2] = x1 - i; c.arg[3] = y1 - i;
        rip_gfx_exec(&g, cv, &c);
    }
    /* label, centred, in the foreground colour */
    if (nlab > 0) {
        if (nlab > RIP_TEXT_MAX)
            nlab = RIP_TEXT_MAX;
        c.ch = 'c'; c.nargs = 1; c.arg[0] = m->st.dfore & 15;
        rip_gfx_exec(&g, cv, &c);
        c.ch = '@'; c.nargs = 2;
        c.arg[0] = (x0 + x1 + 1) / 2 - 4 * nlab;
        c.arg[1] = (y0 + y1 + 1) / 2 - 4;
        memcpy(c.text, lab, (size_t)nlab);
        c.ntext = nlab;
        rip_gfx_exec(&g, cv, &c);
    }
}

int rip_mouse_exec(struct rip_mouse *m, struct rip_gfx *g, struct cv *cv, const struct rip_cmd *c)
{
    const unsigned char *lab, *cmd;
    int nlab, ncmd;
    const long *a = c->arg;
    if (!rip_mouse_handles(c))
        return 0;
    switch (c->ch) {
    case 'K':
        m->n = 0;
        break;
    case 'B':
        if (c->nargs < 10)
            break;
        m->st.wid = (int)a[0];
        m->st.hgt = (int)a[1];
        m->st.bevel = (int)a[4];
        m->st.dfore = (int)a[5];
        m->st.dback = (int)a[6];
        m->st.bright = (int)a[7];
        m->st.dark = (int)a[8];
        m->st.surface = (int)a[9];
        break;
    case 'M':
        if (c->nargs < 7)
            break;
        add_field(m, (int)a[1], (int)a[2], (int)a[3], (int)a[4], (int)a[5], (int)a[6], 0, c->text, c->ntext);
        break;
    case 'U': {
        int x0, y0, x1, y1;
        if (c->nargs < 5)
            break;
        split_text(c, &lab, &nlab, &cmd, &ncmd);
        x0 = (int)a[0];
        y0 = (int)a[1];
        x1 = m->st.wid > 0 ? x0 + m->st.wid : (int)a[2];
        y1 = m->st.hgt > 0 ? y0 + m->st.hgt : (int)a[3];
        if (!add_field(m, x0, y0, x1, y1, 0, 0, (int)a[4], cmd, ncmd))
            break; /* rejected: not drawn either */
        if (cv)
            draw_button(m, g, cv, x0, y0, x1, y1, lab, nlab);
        break;
    }
    }
    return 1;
}

int rip_mouse_at(const struct rip_mouse *m, int px, int py)
{
    int i;
    for (i = m->n - 1; i >= 0; i--) {
        const struct rip_mfield *f = &m->f[i];
        if (px >= f->x0 && px <= f->x1 && py >= f->y0 && py <= f->y1)
            return i;
    }
    return -1;
}

int rip_expand(const unsigned char *src, int n, unsigned char *out, int max)
{
    int i = 0, o = 0;
    while (i < n && o < max) {
        unsigned char b = src[i];
        if (b == '^' && i + 1 < n) {
            unsigned char x = src[i + 1];
            if (x >= 'a' && x <= 'z')
                x = (unsigned char)(x - 32);
            if (x >= '@' && x <= '_')
                out[o++] = (unsigned char)(x & 31);
            i += 2; /* an unknown ^x is dropped */
        } else if (b == '`') {
            i++;
        } else if (b == '$') {
            int j = i + 1;
            while (j < n && ((src[j] >= 'A' && src[j] <= 'Z') || (src[j] >= '0' && src[j] <= '9') || src[j] == '_'))
                j++;
            if (j < n && j > i + 1 && src[j] == '$')
                i = j + 1; /* $NAME$: no variable is defined, so nothing */
            else
                out[o++] = src[i++];
        } else {
            out[o++] = src[i++];
        }
    }
    return o;
}

int rip_mouse_click(const struct rip_mouse *m, int px, int py, unsigned char *out, int max, int *clr)
{
    int i = rip_mouse_at(m, px, py);
    if (i < 0)
        return -1;
    if (clr)
        *clr = m->f[i].clr;
    return rip_expand(m->f[i].cmd, m->f[i].ncmd, out, max);
}

int rip_mouse_key(const struct rip_mouse *m, int key, unsigned char *out, int max, int *clr)
{
    int i;
    if (key >= 'a' && key <= 'z')
        key -= 32;
    for (i = m->n - 1; i >= 0; i--) {
        int h = m->f[i].hotkey;
        if (h >= 'a' && h <= 'z')
            h -= 32;
        if (h && h == key) {
            if (clr)
                *clr = m->f[i].clr;
            return rip_expand(m->f[i].cmd, m->f[i].ncmd, out, max);
        }
    }
    return -1;
}
