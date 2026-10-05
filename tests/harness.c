#include "harness.h"
#include <stdlib.h>

int h_failures, h_checks;
char h_reply[1024];
int h_reply_len;
int h_damage_calls, h_scroll_calls, h_bells, h_layout_which, h_layout_value;

static void cb_damage(void *u, int x0, int y0, int x1, int y1)
{
    (void)u; (void)x0; (void)y0; (void)x1; (void)y1;
    h_damage_calls++;
}

static void cb_scroll(void *u, int top, int bot, int n)
{
    (void)u; (void)top; (void)bot; (void)n;
    h_scroll_calls++;
}

static void cb_reply(void *u, const vt_u8 *b, long n)
{
    (void)u;
    if (h_reply_len + n < (long)sizeof(h_reply) - 1) {
        memcpy(h_reply + h_reply_len, b, n);
        h_reply_len += (int)n;
        h_reply[h_reply_len] = 0;
    }
}

static void cb_bell(void *u)
{
    (void)u;
    h_bells++;
}

static void cb_layout(void *u, int which, int value)
{
    (void)u;
    h_layout_which = which;
    h_layout_value = value;
}

void h_reply_clear(void)
{
    h_reply_len = 0;
    h_reply[0] = 0;
}

vt_term *h_new(int cols, int rows, enum vt_personality p)
{
    vt_callbacks cb;
    vt_term *t;
    memset(&cb, 0, sizeof(cb));
    cb.damage = cb_damage;
    cb.scroll = cb_scroll;
    cb.reply = cb_reply;
    cb.bell = cb_bell;
    cb.layout = cb_layout;
    t = vt_new(cols, rows, 100, &cb, 0);
    if (!t) {
        printf("vt_new failed\n");
        exit(2);
    }
    vt_set_personality(t, p);
    h_reply_clear();
    h_damage_calls = h_scroll_calls = h_bells = 0;
    return t;
}

void h_put(vt_term *t, const char *s)
{
    vt_write(t, (const vt_u8 *)s, (long)strlen(s));
}

const char *h_row(vt_term *t, int row)
{
    static char buf[4096];
    int n, i, len = 0, keep = 0;
    const vt_cell *c = vt_row(t, row, &n);
    if (!c)
        return "<none>";
    for (i = 0; i < n; i++) {
        if (c[i].width == 0)
            continue;
        len += vt_cell_utf8(t, &c[i], buf + len);
        if (c[i].ch != ' ')
            keep = len;
    }
    buf[keep] = 0;
    return buf;
}

const char *h_screen(vt_term *t)
{
    static char buf[16384];
    int y, len = 0, keep = 0;
    for (y = 0; y < vt_rows(t); y++) {
        const char *r = h_row(t, y);
        int n = (int)strlen(r);
        if (y)
            buf[len++] = '|';
        memcpy(buf + len, r, n);
        len += n;
        if (n)
            keep = len;
    }
    buf[keep] = 0;
    return buf;
}

const vt_cell *h_cell(vt_term *t, int x, int y)
{
    int n;
    return vt_row(t, y, &n) + x;
}

/* -DVT_COUNT_ALLOC: the engine's and the line editor's blocks, counted.
 * Each block carries its size in front (aligned for anything). */
long vt_count_live, vt_count_blocks, vt_count_peak;

typedef union { unsigned long n; double d; void *p; long l; } h_block_head;

void *vt_count_malloc(unsigned long n)
{
    h_block_head *h = (h_block_head *)malloc(sizeof(h_block_head) + n);
    if (!h)
        return 0;
    h->n = n;
    vt_count_live += (long)n;
    vt_count_blocks++;
    if (vt_count_live > vt_count_peak)
        vt_count_peak = vt_count_live;
    return h + 1;
}

void vt_count_free(void *p)
{
    h_block_head *h;
    if (!p)
        return;
    h = (h_block_head *)p - 1;
    vt_count_live -= (long)h->n;
    vt_count_blocks--;
    free(h);
}
