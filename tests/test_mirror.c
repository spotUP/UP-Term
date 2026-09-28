/* The renderer contract, on every recorded stream: a model renderer that
 * knows only what the callbacks tell it (scroll moves its rows, damage
 * copies cells from the grid) must equal the engine's grid after every
 * write. This is what lets the engine batch scrolls into one blit and
 * lets render/amiga_render.c draw only damaged cells. Streams are fed in
 * pseudo-random chunk sizes, in all three personalities. */
#include "harness.h"
#include <dirent.h>
#include <stdlib.h>

typedef struct mirror {
    vt_term *t;
    int cols, rows;
    vt_cell *pix;          /* the model's "screen" */
    long scrolls, damages;
} mirror;

static void m_damage(void *u, int x0, int y0, int x1, int y1)
{
    mirror *m = (mirror *)u;
    int x, y, n;
    if (!m->t)
        return; /* vt_new's own reset; vt_set_personality damages all again */
    m->damages++;
    for (y = y0; y < y1; y++) {
        const vt_cell *c = vt_row(m->t, y, &n);
        for (x = x0; x < x1; x++)
            m->pix[y * m->cols + x] = c[x];
    }
}

static void blank(vt_cell *c)
{
    c->ch = ' ';
    c->fg = VT_COLOR_DEFAULT;
    c->bg = VT_COLOR_DEFAULT;
    c->attr = 0;
    c->width = 1;
}

static void m_scroll(void *u, int top, int bot, int n)
{
    mirror *m = (mirror *)u;
    int y, x;
    m->scrolls++;
    if (n > 0) {
        for (y = top; y < bot - n; y++)
            memcpy(&m->pix[y * m->cols], &m->pix[(y + n) * m->cols], m->cols * sizeof(vt_cell));
        for (y = bot - n; y < bot; y++)
            for (x = 0; x < m->cols; x++)
                blank(&m->pix[y * m->cols + x]); /* vacated: default blank (the contract) */
    } else {
        n = -n;
        for (y = bot - 1; y >= top + n; y--)
            memcpy(&m->pix[y * m->cols], &m->pix[(y - n) * m->cols], m->cols * sizeof(vt_cell));
        for (y = top; y < top + n; y++)
            for (x = 0; x < m->cols; x++)
                blank(&m->pix[y * m->cols + x]);
    }
}

static int same(const mirror *m, int *bx, int *by)
{
    int x, y, n;
    for (y = 0; y < m->rows; y++) {
        const vt_cell *c = vt_row(m->t, y, &n);
        for (x = 0; x < m->cols; x++) {
            const vt_cell *p = &m->pix[y * m->cols + x];
            if (p->ch != c[x].ch || p->fg != c[x].fg || p->bg != c[x].bg || p->attr != c[x].attr) {
                *bx = x;
                *by = y;
                return 0;
            }
        }
    }
    return 1;
}

static void run_stream(const char *path, int cols, int rows, enum vt_personality pers)
{
    FILE *f = fopen(path, "rb");
    static vt_u8 data[1 << 20];
    long len, off = 0;
    unsigned long seed = 12345;
    mirror m;
    vt_callbacks cb;
    int bx, by, bad = 0;
    if (!f)
        return;
    len = (long)fread(data, 1, sizeof(data), f);
    fclose(f);
    memset(&cb, 0, sizeof(cb));
    cb.damage = m_damage;
    cb.scroll = m_scroll;
    memset(&m, 0, sizeof(m));
    m.cols = cols;
    m.rows = rows;
    m.pix = (vt_cell *)calloc((size_t)cols * rows, sizeof(vt_cell));
    m.t = vt_new(cols, rows, 50, &cb, &m);
    vt_set_personality(m.t, pers); /* its reset damages everything */
    while (off < len) {
        long n;
        seed = seed * 1103515245UL + 12345UL;
        n = 1 + (long)((seed >> 16) % 97);
        if (off + n > len)
            n = len - off;
        vt_write(m.t, data + off, n);
        off += n;
        if (!same(&m, &bx, &by)) {
            bad = 1;
            break;
        }
    }
    h_checks++;
    if (bad) {
        h_failures++;
        printf("  FAIL %s (personality %d): model differs at %d,%d after byte %ld\n", path, pers,
               bx, by, off);
    }
    vt_free(m.t);
    free(m.pix);
}

void suite_mirror(void)
{
    DIR *d = opendir("tests/streams");
    struct dirent *e;
    int streams = 0;
    if (!d) {
        h_checks++;
        h_failures++;
        printf("  FAIL cannot open tests/streams (run from the repo root)\n");
        return;
    }
    while ((e = readdir(d))) {
        char path[512];
        int cols = 80, rows = 24;
        const char *dot;
        size_t l = strlen(e->d_name);
        if (l < 5 || strcmp(e->d_name + l - 4, ".bin"))
            continue;
        dot = strrchr(e->d_name, 'x');
        if (dot) {
            const char *p = dot;
            while (p > e->d_name && p[-1] >= '0' && p[-1] <= '9')
                p--;
            cols = atoi(p);
            rows = atoi(dot + 1);
            if (cols < 2 || rows < 2) {
                cols = 80;
                rows = 24;
            }
        }
        if (l > sizeof(path) - 20)
            continue;
        strcpy(path, "tests/streams/");
        strcat(path, e->d_name);
        run_stream(path, cols, rows, VT_XTERM);
        run_stream(path, cols, rows, VT_AMIGA);
        run_stream(path, cols, rows, VT_PCANSI);
        streams++;
    }
    closedir(d);
    h_checks++;
    if (streams < 100) {
        h_failures++;
        printf("  FAIL only %d streams found\n", streams);
    }
}
