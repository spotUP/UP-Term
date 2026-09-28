/* The cooked line editor; see lineedit.h.
 *
 * The line is drawn by writing to the engine, like any output: it knows
 * where the line started (an absolute row, so it survives the screen
 * scrolling under it), and after an edit it rewrites the line from the
 * first changed character, blanks what got shorter, and puts the cursor
 * back with CUP. A line longer than the window wraps like any text. */
#include "lineedit.h"
#include <string.h>

static void out(le_line *le, const void *b, long n)
{
    if (n > 0)
        le->out(le->user, (const unsigned char *)b, n);
}

static int is_cont(const le_line *le, unsigned char b)
{
    return le->utf8 && (b & 0xC0) == 0x80;
}

static int next_char(const le_line *le, int p)
{
    if (p >= le->len)
        return le->len;
    p++;
    while (p < le->len && is_cont(le, le->buf[p]))
        p++;
    return p;
}

static int prev_char(const le_line *le, int p)
{
    if (p <= 0)
        return 0;
    p--;
    while (p > 0 && is_cont(le, le->buf[p]))
        p--;
    return p;
}

/* cells (characters) in buf[0..p) */
static int cells(const le_line *le, int p)
{
    int i, n = 0;
    for (i = 0; i < p; i++)
        if (!is_cont(le, le->buf[i]))
            n++;
    return n;
}

static void start(le_line *le)
{
    int x, y;
    if (le->started)
        return;
    vt_cursor(le->t, &x, &y);
    le->start_row = y + vt_lines_scrolled(le->t);
    le->start_col = x;
    le->started = 1;
    le->shown = 0;
}

static void cup(le_line *le, long row, int col)
{
    char b[24];
    int n = 0;
    long v;
    char d[12];
    int k;
    b[n++] = 0x1B;
    b[n++] = '[';
    for (v = row + 1, k = 0; v || !k; v /= 10)
        d[k++] = (char)('0' + v % 10);
    while (k)
        b[n++] = d[--k];
    b[n++] = ';';
    for (v = col + 1, k = 0; v || !k; v /= 10)
        d[k++] = (char)('0' + v % 10);
    while (k)
        b[n++] = d[--k];
    b[n++] = 'H';
    out(le, b, n);
}

/* Put the cursor before byte p of the line. */
static void go(le_line *le, int p)
{
    int cols = vt_cols(le->t), rows = vt_rows(le->t);
    long cell = (long)le->start_col + cells(le, p);
    long row = le->start_row + cell / cols - vt_lines_scrolled(le->t);
    int col = (int)(cell % cols);
    if (p > 0 && col == 0) {
        /* Right after a row's last character: rewrite that character, so
         * the engine stands where typing it would have left it (the
         * deferred wrap), even on the bottom row. */
        int q = prev_char(le, p);
        go(le, q);
        out(le, le->buf + q, p - q);
        return;
    }
    if (row < 0)
        row = 0;
    if (row > rows - 1)
        row = rows - 1;
    cup(le, row, col);
}

/* Show the line from byte p on, blank the cells it no longer covers, and
 * leave the cursor at le->pos. */
static void redraw_from(le_line *le, int p)
{
    int now = cells(le, le->len), i;
    go(le, p);
    out(le, le->buf + p, le->len - p);
    for (i = now; i < le->shown; i++)
        out(le, " ", 1);
    le->shown = now;
    go(le, le->pos);
}

static void erase(le_line *le, int a, int b)
{
    if (b <= a)
        return;
    memmove(le->buf + a, le->buf + b, le->len - b);
    le->len -= b - a;
    if (le->pos > b)
        le->pos -= b - a;
    else if (le->pos > a)
        le->pos = a;
    redraw_from(le, a);
}

static void set_line(le_line *le, const unsigned char *s)
{
    int n = (int)strlen((const char *)s);
    if (n > LE_MAX - 2)
        n = LE_MAX - 2;
    memcpy(le->buf, s, n);
    le->len = n;
    le->pos = n;
    redraw_from(le, 0);
}

static void hist_add(le_line *le)
{
    int n = le->len, i;
    if (!n)
        return;
    if (n > LE_MAX / 4 - 1)
        n = LE_MAX / 4 - 1;
    if (le->hist_n && !memcmp(le->hist[le->hist_n - 1], le->buf, n) &&
        le->hist[le->hist_n - 1][n] == 0)
        return; /* the same line again */
    if (le->hist_n == LE_HIST) {
        for (i = 1; i < LE_HIST; i++)
            memcpy(le->hist[i - 1], le->hist[i], LE_MAX / 4);
        le->hist_n--;
    }
    memcpy(le->hist[le->hist_n], le->buf, n);
    le->hist[le->hist_n][n] = 0;
    le->hist_n++;
}

static void history(le_line *le, int dir, int search)
{
    int i = le->hist_pos;
    int plen = search ? le->pos : 0;
    for (;;) {
        i += dir;
        if (i < 0 || i > le->hist_n)
            return;
        if (i == le->hist_n || !plen || !memcmp(le->hist[i], le->buf, plen))
            break;
    }
    le->hist_pos = i;
    if (i == le->hist_n) {
        if (!search)
            set_line(le, (const unsigned char *)"");
        return;
    }
    set_line(le, le->hist[i]);
    if (search) {
        le->pos = plen; /* the searched prefix stays under the cursor */
        go(le, le->pos);
    }
}

void le_init(le_line *le, vt_term *t, void (*o)(void *, const unsigned char *, long), void *user)
{
    memset(le, 0, sizeof(*le));
    le->t = t;
    le->out = o;
    le->user = user;
    le->utf8 = 1;
}

void le_reset(le_line *le)
{
    le->len = 0;
    le->pos = 0;
    le->started = 0;
    le->shown = 0;
    le->hist_pos = le->hist_n;
}

int le_key(le_line *le, long key, int mods, const unsigned char *b, int n)
{
    int shift = (mods & VT_MOD_SHIFT) != 0;
    start(le);
    if (key == VT_KEY_RETURN || key == VT_KEY_KP_ENTER) {
        le->pos = le->len;
        go(le, le->pos);
        out(le, "\r\n", 2);
        hist_add(le);
        le->buf[le->len++] = '\n';
        return 1;
    }
    switch (key) {
    case VT_KEY_LEFT:
        le->pos = shift ? 0 : prev_char(le, le->pos);
        go(le, le->pos);
        return 0;
    case VT_KEY_RIGHT:
        le->pos = shift ? le->len : next_char(le, le->pos);
        go(le, le->pos);
        return 0;
    case VT_KEY_HOME:
        le->pos = 0;
        go(le, 0);
        return 0;
    case VT_KEY_END:
        le->pos = le->len;
        go(le, le->pos);
        return 0;
    case VT_KEY_BACKSPACE:
        erase(le, prev_char(le, le->pos), le->pos);
        return 0;
    case VT_KEY_DELETE:
        erase(le, le->pos, next_char(le, le->pos));
        return 0;
    case VT_KEY_UP:
        history(le, -1, shift);
        return 0;
    case VT_KEY_DOWN:
        history(le, 1, shift);
        return 0;
    default:
        break;
    }
    if (key >= 0x110000)
        return 0; /* other special keys: nothing in a cooked line */
    if (n == 1 && b[0] < 0x20 && b[0] != '\t') {
        int p;
        switch (b[0]) {
        case 0x01: /* Ctrl-A */
            le->pos = 0;
            go(le, 0);
            break;
        case 0x05: /* Ctrl-E */
            le->pos = le->len;
            go(le, le->pos);
            break;
        case 0x0B: /* Ctrl-K */
            erase(le, le->pos, le->len);
            break;
        case 0x15: /* Ctrl-U */
            erase(le, 0, le->pos);
            break;
        case 0x17: /* Ctrl-W: back over blanks, then over a word */
            p = le->pos;
            while (p > 0 && le->buf[p - 1] == ' ')
                p--;
            while (p > 0 && le->buf[p - 1] != ' ')
                p--;
            erase(le, p, le->pos);
            break;
        case 0x18: /* Ctrl-X */
            erase(le, 0, le->len);
            break;
        default:
            break;
        }
        return 0;
    }
    if (le->len + n > LE_MAX - 2)
        return 0;
    if (le->pos == le->len) {
        /* typing at the end: just echo, the engine wraps and scrolls */
        memcpy(le->buf + le->len, b, n);
        le->len += n;
        le->pos = le->len;
        le->shown = cells(le, le->len);
        out(le, b, n);
        return 0;
    }
    memmove(le->buf + le->pos + n, le->buf + le->pos, le->len - le->pos);
    memcpy(le->buf + le->pos, b, n);
    le->len += n;
    le->pos += n;
    redraw_from(le, le->pos - n);
    return 0;
}
