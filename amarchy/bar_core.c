/* The Amarchy bar, the portable core (bar_core.h). */
#include <string.h>
#include "bar_core.h"

void bar_init(bar *b, int width, int pad, int gap)
{
    memset(b, 0, sizeof *b);
    b->width = width;
    b->pad = pad;
    b->gap = gap;
    b->cw = 8;
}

static void put_text(bar_seg *s, const char *text, int tw)
{
    strncpy(s->text, text ? text : "", BAR_TEXT - 1);
    s->text[BAR_TEXT - 1] = 0;
    s->tw = tw;
}

int bar_add(bar *b, int id, int side, const char *text, int tw)
{
    bar_seg *s;
    if (b->n >= BAR_MAX_SEGS)
        return -1;
    s = &b->seg[b->n];
    memset(s, 0, sizeof *s);
    s->id = id;
    s->side = side;
    put_text(s, text, tw);
    s->dirty = 1;
    return b->n++;
}

int bar_set_text(bar *b, int id, const char *text, int tw)
{
    int i;
    for (i = 0; i < b->n; i++) {
        bar_seg *s = &b->seg[i];
        if (s->id != id)
            continue;
        if (s->tw == tw && !strncmp(s->text, text ? text : "", BAR_TEXT - 1))
            return 0;
        put_text(s, text, tw);
        s->dirty = 1;
        return 1;
    }
    return 0;
}

int bar_set_slot(bar *b, int id, int slot)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (b->seg[i].id == id && b->seg[i].slot != slot) {
            b->seg[i].slot = slot;
            b->seg[i].dirty = 1;
            return 1;
        }
    return 0;
}

static void place(bar_seg *s, int x, int w)
{
    if (s->x != x || s->w != w)
        s->dirty = 1;
    s->x = x;
    s->w = w;
}

/* Appends decimal n, zero padded to `min` digits, at t + o; returns the new offset (no stdio: m68k libc and
 * -Werror hosts both stay clear of sprintf). */
static int put_u(char *t, int o, unsigned long n, int min)
{
    char d[12];
    int k = 0;
    do {
        d[k++] = (char)('0' + n % 10UL);
        n /= 10UL;
    } while (n);
    while (k < min)
        d[k++] = '0';
    while (k)
        t[o++] = d[--k];
    return o;
}

static int put_s(char *t, int o, const char *s)
{
    while (*s)
        t[o++] = *s++;
    return o;
}

static int segw(const bar_seg *s, int pad)
{
    return (s->tw > s->slot ? s->tw : s->slot) + 2 * pad;
}

/* The left group with `k` screen entries shown (-1 = all): placed when `commit`, else measured. Returns its end x
 * (0 when empty). The marker follows the last shown entry. */
static int place_left(bar *b, int k, int commit)
{
    int i, x = 0, j = 0, end = 0;
    for (i = 0; i < b->n; i++) {
        bar_seg *s = &b->seg[i];
        int show = 1, w;
        if (s->side != BAR_LEFT)
            continue;
        if (BAR_IS_SCREEN(s->id)) {
            show = k < 0 || j < k;
            j++;
        } else if (s->id == BAR_SEG_MORE) {
            show = k >= 0;
        }
        if (!show) {
            if (commit) {
                place(s, 0, 0);
                s->hidden = 1;
            }
            continue;
        }
        w = segw(s, b->pad);
        if (commit) {
            place(s, x, w);
            s->hidden = 0;
        }
        x += w + b->gap;
        end = x - b->gap;
    }
    return end;
}

static int marker_text(bar *b, int hidden_n)
{
    char t[BAR_TEXT];
    int i, o = put_s(t, 0, "+");
    o = put_u(t, o, (unsigned long)hidden_n, 1);
    t[o] = 0;
    for (i = 0; i < b->n; i++)
        if (b->seg[i].id == BAR_SEG_MORE)
            return bar_set_text(b, BAR_SEG_MORE, t, (int)strlen(t) * b->cw);
    return 0;
}

/* Drop order of the right group when the bar is too narrow: rank 0 goes first. Every other id (System, the clock,
 * the screens) never drops; the first screen is hidden only when nothing else is left to drop (bar_layout). */
static int drop_rank(int id)
{
    switch (id) {
    case BAR_SEG_NET: return 0;
    case BAR_SEG_CPU: return 1;
    case BAR_SEG_RAM: return 2;
    case BAR_SEG_AGENTS: return 3;
    default: return BAR_IS_TRAY(id) ? 4 : -1;     /* the bar.conf tray segments go after Agents, all at once */
    }
}
#define BAR_DROP_RANKS 5

/* The right group from the right edge with the first `ndrop` ranks dropped (placed when `commit`, else measured);
 * returns the x its leftmost shown segment starts at, minus the gap. */
static int place_right(bar *b, int ndrop, int commit)
{
    int i, r = b->width;
    for (i = b->n - 1; i >= 0; i--) {
        bar_seg *s = &b->seg[i];
        int rk;
        if (s->side != BAR_RIGHT)
            continue;
        rk = drop_rank(s->id);
        if (rk >= 0 && rk < ndrop) {
            if (commit) {
                place(s, 0, 0);
                s->hidden = 1;
            }
            continue;
        }
        if (commit) {
            place(s, r - segw(s, b->pad), segw(s, b->pad));
            s->hidden = 0;
        }
        r -= segw(s, b->pad) + b->gap;
    }
    return r;
}

void bar_layout(bar *b, int width)
{
    int i, x, r, nscr = 0, k = -1, c_w = 0, c_n = 0, left_end, cx, nd;
    b->width = width;
    for (i = 0; i < b->n; i++) {
        if (b->seg[i].side == BAR_CENTRE) {
            c_w += segw(&b->seg[i], b->pad) + (c_n ? b->gap : 0);
            c_n++;
        } else if (b->seg[i].side == BAR_LEFT && BAR_IS_SCREEN(b->seg[i].id)) {
            nscr++;
        }
    }
    /* too narrow even with one screen entry: drop right segments by rank until the smallest left group and the
     * centre fit (the screens list cut below only trims the list, it never frees room for the right group) */
    if (nscr > 1)
        marker_text(b, nscr - 1);
    for (nd = 0; nd <= BAR_DROP_RANKS; nd++) {
        int rr = place_right(b, nd, 0);
        int lim = c_n ? rr - c_w - b->gap : rr;
        if (place_left(b, nscr > 1 ? 1 : -1, 0) <= lim)
            break;
    }
    if (nd > BAR_DROP_RANKS)
        nd = BAR_DROP_RANKS;
    r = place_right(b, nd, 1);
    /* the room the left group may use: up to the centre group's latest place, or the right group */
    {
        int limit = c_n ? r - c_w - b->gap : r;
        left_end = place_left(b, -1, 0);
        if (left_end > limit && nscr > 1) {
            for (k = nscr - 1; k >= 1; k--) {
                marker_text(b, nscr - k);
                left_end = place_left(b, k, 0);
                if (left_end <= limit)
                    break;
            }
            if (k < 1)
                k = 1;
        }
    }
    if (k >= 0)
        marker_text(b, nscr - k);
    left_end = place_left(b, k, 1);
    /* still too wide with a single screen entry (Clock + System + that entry do not fit): clip the left group to
     * the room the centre and right groups leave, never into them. No room at all (limit <= 0: the centre and the
     * undroppable right segments alone fill the bar): the last to give way is the first screen entry, hidden, so no
     * two shown segments ever intersect (the plan's "first screen never drops" holds at every width with room). */
    {
        int limit = c_n ? r - c_w - b->gap : r;
        if (left_end > limit) {
            for (i = 0; i < b->n; i++) {
                bar_seg *s = &b->seg[i];
                if (s->side != BAR_LEFT || s->hidden || s->x + s->w <= limit)
                    continue;
                if (limit - s->x > 0) {
                    place(s, s->x, limit - s->x);
                } else {
                    place(s, 0, 0);
                    s->hidden = 1;
                }
            }
            left_end = limit > 0 ? limit : 0;
        }
    }
    if (c_n) {
        cx = (width - c_w) / 2;
        if (left_end && cx < left_end + b->gap)
            cx = left_end + b->gap;
        if (cx + c_w > r)
            cx = r - c_w;
        if (cx < 0)
            cx = 0;
        x = cx;
        for (i = 0; i < b->n; i++) {
            bar_seg *s = &b->seg[i];
            if (s->side != BAR_CENTRE)
                continue;
            place(s, x, segw(s, b->pad));
            x += s->w + b->gap;
        }
    }
}

int bar_hit(const bar *b, int x)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (!b->seg[i].hidden && x >= b->seg[i].x && x < b->seg[i].x + b->seg[i].w)
            return i;
    return -1;
}

int bar_click(const bar *b, int x, int button, int *id)
{
    int i = bar_hit(b, x);
    if (id)
        *id = 0;
    if (i < 0)
        return BAR_ACT_NONE;
    if (id)
        *id = b->seg[i].id;
    switch (b->seg[i].id) {
    case BAR_SEG_AGENTS:
        if (button == BAR_BTN_LEFT)
            return BAR_ACT_PANEL;
        if (button == BAR_BTN_RIGHT)
            return BAR_ACT_LAUNCH_AGENT;
        break;
    case BAR_SEG_SYSTEM:
        return button == BAR_BTN_LEFT ? BAR_ACT_SYSTEM : BAR_ACT_NONE;
    case BAR_SEG_CLOCK:
        return button == BAR_BTN_LEFT ? BAR_ACT_CLOCK : BAR_ACT_NONE;
    default:
        if (BAR_IS_SCREEN(b->seg[i].id) && button == BAR_BTN_LEFT)
            return BAR_ACT_SCREEN;
        if (BAR_IS_TRAY(b->seg[i].id) && button == BAR_BTN_LEFT)
            return BAR_ACT_RUN;
        break;
    }
    return BAR_ACT_NONE;
}

int bar_press_act(const bar *b, int px, int py, int top, int h, int button)
{
    if (py < top || py >= top + h || px < 0 || px >= b->width)
        return BAR_ACT_NONE;
    return bar_click(b, px, button, 0);
}

int bar_take_dirty(bar *b, int from)
{
    int i;
    for (i = from < 0 ? 0 : from; i < b->n; i++)
        if (b->seg[i].dirty) {
            b->seg[i].dirty = 0;
            return i;
        }
    return -1;
}

void bar_panel_anchor(int screen_w, int bar_top, int *ax, int *ay)
{
    *ax = screen_w;
    *ay = bar_top;
}

int bar_is_desktop(unsigned long wflags, int l, int r, int screen_w)
{
    if (wflags & BAR_WFLG_BACKDROP)
        return 1;
    return (wflags & BAR_WFLG_WBENCHWINDOW) != 0 && l <= 0 && r >= screen_w - 1;
}

int bar_covered(int l, int t, int r, int b, const bar_lyr *front, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        const bar_lyr *f = &front[i];
        if (f->ours || !f->backdrop)
            continue;
        if (f->l <= r && f->r >= l && f->t <= b && f->b >= t)
            return 1;
    }
    return 0;
}

/* Formatters */

static void out_put(char *out, int cap, const char *s)
{
    int n = (int)strlen(s);
    if (cap <= 0)
        return;
    if (n > cap - 1)
        n = cap - 1;
    memcpy(out, s, n);
    out[n] = 0;
}

void bar_fmt_bytes(char *out, int cap, unsigned long n)
{
    char t[16];
    int o = 0;
    unsigned long k = n > 0xFFFFFFFFUL - 512UL ? n >> 10 : (n + 512UL) >> 10;     /* rounded KB, no wrap at 4 GB */
    if (k < 1024UL) {
        o = put_u(t, o, k, 1);
        t[o++] = 'K';
    } else {
        unsigned long whole = n >> 20, rem = n & 0xFFFFFUL, tenths = (rem * 10UL + 524288UL) >> 20;
        if (whole >= 10UL) {
            if (rem >= 524288UL)
                whole++;
            tenths = 10UL;                                   /* no decimal */
        } else if (tenths >= 10UL) {
            whole++;
            tenths = 0;
        }
        o = put_u(t, o, whole, 1);
        if (whole < 10UL && tenths < 10UL) {
            t[o++] = '.';
            o = put_u(t, o, tenths, 1);
        }
        t[o++] = 'M';
    }
    t[o] = 0;
    out_put(out, cap, t);
}

void bar_fmt_ram(char *out, int cap, unsigned long chip, unsigned long fast)
{
    char c[16], f[16], t[BAR_TEXT];
    int o = put_s(t, 0, "C ");
    bar_fmt_bytes(c, sizeof c, chip);
    bar_fmt_bytes(f, sizeof f, fast);
    o = put_s(t, o, c);
    o = put_s(t, o, " F ");
    o = put_s(t, o, f);
    t[o] = 0;
    out_put(out, cap, t);
}

void bar_fmt_time(char *out, int cap, const char *fmt, int wday, int day, int mon, int year, int hour, int min)
{
    static const char wd[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char mn[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    char t[BAR_TEXT * 2];
    int o = 0;
    for (; *fmt && o < (int)sizeof t - 8; fmt++) {
        if (*fmt != '%' || !fmt[1]) {
            t[o++] = *fmt;
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 'H': o = put_u(t, o, (unsigned long)hour, 2); break;
        case 'M': o = put_u(t, o, (unsigned long)min, 2); break;
        case 'a': o = put_s(t, o, wd[(wday % 7 + 7) % 7]); break;
        case 'd': o = put_u(t, o, (unsigned long)day, 2); break;
        case 'b': o = put_s(t, o, mn[(mon % 12 + 12) % 12]); break;
        case 'Y': o = put_u(t, o, (unsigned long)year, 4); break;
        case '%': t[o++] = '%'; break;
        default: t[o++] = '%'; t[o++] = *fmt; break;
        }
    }
    t[o] = 0;
    out_put(out, cap, t);
}

int bar_fmt_percent(unsigned long idle, unsigned long total)
{
    if (!total || idle >= total)
        return 0;
    return (int)(((total - idle) * 100UL + total / 2) / total);
}

void bar_fmt_screen(char *out, int cap, const char *title)
{
    int n = 0;
    if (cap <= 0)
        return;
    while (title && *title == ' ')
        title++;
    while (title && title[n] && title[n] != ' ' && n < cap - 1) {
        out[n] = title[n];
        n++;
    }
    out[n] = 0;
}

unsigned long bar_cpu_cal(unsigned long cal, unsigned long dcount, unsigned long dticks)
{
    unsigned long rate;
    if (!dticks)
        return cal;
    rate = dcount / dticks;
    return rate > cal ? rate : cal;
}

int bar_cpu_pct(unsigned long dcount, unsigned long dticks, unsigned long cal)
{
    unsigned long expect;
    if (!cal || !dticks)
        return 0;
    if (dticks > BAR_CPU_MAX_TICKS)
        dticks = BAR_CPU_MAX_TICKS;
    expect = (cal >> 8) * dticks;           /* counts / 256: the percent maths stays inside 32 bits */
    return bar_fmt_percent(dcount >> 8, expect);
}

void bar_fmt_cpu(char *out, int cap, int pct)
{
    char t[BAR_TEXT];
    int n = 0;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    memcpy(t, "CPU ", 4);
    n = 4;
    if (pct >= 100)
        t[n++] = '1';
    if (pct >= 10)
        t[n++] = (char)('0' + (pct / 10) % 10);
    t[n++] = (char)('0' + pct % 10);
    t[n++] = '%';
    t[n] = 0;
    out_put(out, cap, t);
}

int bar_net_kind(const char *id_string)
{
    if (id_string && id_string[0] == 'U' && id_string[1] == 'A' && id_string[2] == 'E')
        return BAR_NET_EMULATED;
    return BAR_NET_STACK;
}

int bar_net_state_kind(long ifq_state)
{
    if (ifq_state == BAR_SM_UP || ifq_state == BAR_SM_ONLINE)
        return BAR_NET_UP;
    if (ifq_state == BAR_SM_DOWN || ifq_state == BAR_SM_OFFLINE)
        return BAR_NET_DOWN;
    return BAR_NET_STACK;
}

void bar_fmt_net(char *out, int cap, int kind, const char *name)
{
    char t[BAR_TEXT];
    if (kind == BAR_NET_EMULATED) {
        out_put(out, cap, "Network emulated");
    } else if (kind == BAR_NET_UP || kind == BAR_NET_DOWN) {
        t[0] = 0;
        if (name && name[0]) {
            strncat(t, name, 24);
            strcat(t, " ");
        } else {
            strcat(t, "Network ");
        }
        strcat(t, kind == BAR_NET_UP ? "up" : "down");
        out_put(out, cap, t);
    } else {
        out_put(out, cap, "No network");
    }
}

static int find_seg(const bar *b, int id)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (b->seg[i].id == id)
            return i;
    return -1;
}

static void remove_at(bar *b, int i)
{
    memmove(&b->seg[i], &b->seg[i + 1], (size_t)(b->n - i - 1) * sizeof b->seg[0]);
    b->n--;
}

int bar_set_accent(bar *b, int id, int on)
{
    int i = find_seg(b, id);
    if (i < 0 || b->seg[i].accent == (on != 0))
        return 0;
    b->seg[i].accent = on != 0;
    b->seg[i].dirty = 1;
    return 1;
}

int bar_set_screens(bar *b, int n, const char *const *names, const int *tw, int front)
{
    int i, ch = 0, m;
    if (n > BAR_MAX_SCREENS)
        n = BAR_MAX_SCREENS;
    for (i = 0; i < n; i++) {
        int id = BAR_SEG_SCREEN0 + i;
        if (find_seg(b, id) < 0) {
            if (bar_add(b, id, BAR_LEFT, names[i], tw[i]) < 0)
                break;
            ch = 1;
        } else {
            ch |= bar_set_text(b, id, names[i], tw[i]);
        }
        ch |= bar_set_accent(b, id, i == front);
    }
    for (i = n; i < BAR_MAX_SCREENS; i++) {
        int k = find_seg(b, BAR_SEG_SCREEN0 + i);
        if (k >= 0) {
            remove_at(b, k);
            ch = 1;
        }
    }
    m = find_seg(b, BAR_SEG_MORE);
    if (m >= 0 && m != b->n - 1) {      /* the marker follows the last entry in the array order place_left walks */
        bar_seg tmp = b->seg[m];
        remove_at(b, m);
        b->seg[b->n++] = tmp;
        ch = 1;
    }
    return ch;
}

void bar_dump(const bar *b, char *out, int cap)
{
    char t[BAR_TEXT * 2 + 40];
    int i, o = 0, n = 0;
    out[0] = 0;
    for (i = 0; i < b->n; i++) {
        const bar_seg *s = &b->seg[i];
        int k = 0;
        if (n)
            t[k++] = '|';
        k = put_u(t, k, (unsigned long)s->id, 1);
        t[k++] = '@';
        k = put_u(t, k, (unsigned long)s->x, 1);
        t[k++] = '+';
        k = put_u(t, k, (unsigned long)s->w, 1);
        if (s->accent)
            t[k++] = '*';
        t[k++] = '=';
        k = put_s(t, k, s->text);
        t[k] = 0;
        if (o + k >= cap)
            break;
        memcpy(out + o, t, (size_t)k + 1);
        o += k;
        n++;
    }
}

void bar_days_to_date(long days, int *wday, int *day, int *mon, int *year)
{
    long z = days + 2922L + 719468L;     /* 1978-01-01 is 2922 days after 1970-01-01 (civil-from-days, 400 year eras) */
    long era = z / 146097L, doe = z - era * 146097L;
    long yoe = (doe - doe / 1460L + doe / 36524L - doe / 146096L) / 365L;
    long doy = doe - (365L * yoe + yoe / 4L - yoe / 100L);
    long mp = (5L * doy + 2L) / 153L;
    long d = doy - (153L * mp + 2L) / 5L + 1L;
    long m = mp < 10 ? mp + 3 : mp - 9;
    *wday = (int)((days % 7L + 7L) % 7L);
    *day = (int)d;
    *mon = (int)(m - 1);
    *year = (int)(yoe + era * 400L + (m <= 2 ? 1 : 0));
}
