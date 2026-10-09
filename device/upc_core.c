/* upc_core -- see upc_core.h. Portable C89, no OS calls, no statics. */
#include "upc_core.h"

/* ---- read queue -------------------------------------------------------- */

void upc_rq_init(upc_rq *q, upc_reply_fn reply, upc_paste_fn paste_done, void *user)
{
    unsigned char *p = (unsigned char *)q;
    unsigned long i;
    for (i = 0; i < sizeof(*q); i++)
        p[i] = 0;
    q->reply = reply;
    q->paste_done = paste_done;
    q->user = user;
}

long upc_rq_available(const upc_rq *q)
{
    return q->count + q->paste_len;
}

static void paste_end(upc_rq *q)
{
    const unsigned char *t = q->paste_text;
    q->paste = q->paste_text = 0;
    q->paste_len = 0;
    q->before = 0;
    if (t && q->paste_done)
        q->paste_done(q->user, t);
}

/* Up to max bytes of the stream in order: the ring bytes that came before
 * the paste, the paste, then the ring bytes that came after it. */
static long take(upc_rq *q, unsigned char *dst, long max)
{
    long n = 0;
    while (n < max) {
        if (q->paste_len > 0 && q->before == 0) {
            long k = max - n < q->paste_len ? max - n : q->paste_len;
            long i;
            for (i = 0; i < k; i++)
                dst[n + i] = q->paste[i];
            q->paste += k;
            q->paste_len -= k;
            n += k;
            if (q->paste_len == 0)
                paste_end(q);
            continue;
        }
        if (q->count == 0)
            break;
        dst[n++] = q->in[q->head];
        q->head = (q->head + 1) % UPC_INBUF;
        q->count--;
        if (q->before > 0)
            q->before--;
    }
    return n;
}

/* Answer queued reads, oldest first, while input is there. */
static void serve(upc_rq *q)
{
    while (q->nreads > 0 && upc_rq_available(q) > 0) {
        upc_read r = q->reads[0];
        long got;
        int i;
        for (i = 1; i < q->nreads; i++)
            q->reads[i - 1] = q->reads[i];
        q->nreads--;
        got = take(q, r.buf, r.len);
        q->answered++;
        if (q->reply)
            q->reply(q->user, r.req, got);
    }
}

int upc_rq_read(upc_rq *q, void *req, unsigned char *buf, long len, long *actual)
{
    *actual = 0;
    if (len <= 0) {
        q->answered++;
        return UPC_READ_DONE;
    }
    if (q->nreads == 0 && upc_rq_available(q) > 0) {
        *actual = take(q, buf, len);
        q->answered++;
        return UPC_READ_DONE;
    }
    if (q->nreads >= UPC_MAXREADS)
        return UPC_READ_FULL;
    q->reads[q->nreads].req = req;
    q->reads[q->nreads].buf = buf;
    q->reads[q->nreads].len = len;
    q->nreads++;
    return UPC_READ_QUEUED;
}

long upc_rq_input(upc_rq *q, const unsigned char *s, long n)
{
    long kept = 0;
    while (kept < n) {
        long room = UPC_INBUF - q->count;
        if (room == 0) {
            serve(q);
            room = UPC_INBUF - q->count;
            if (room == 0)
                break;
        }
        while (room > 0 && kept < n) {
            q->in[(q->head + q->count) % UPC_INBUF] = s[kept++];
            q->count++;
            room--;
        }
    }
    q->dropped += n - kept;
    serve(q);
    return kept;
}

int upc_rq_paste(upc_rq *q, const unsigned char *text, long n)
{
    if (q->paste_len > 0)
        return 0;
    if (n <= 0) {
        if (q->paste_done && text)
            q->paste_done(q->user, text);
        return 1;
    }
    q->paste = q->paste_text = text;
    q->paste_len = n;
    q->before = q->count;
    serve(q);
    return 1;
}

int upc_rq_abort(upc_rq *q, void *req)
{
    int i, j;
    for (i = 0; i < q->nreads; i++)
        if (q->reads[i].req == req) {
            for (j = i + 1; j < q->nreads; j++)
                q->reads[j - 1] = q->reads[j];
            q->nreads--;
            return 1;
        }
    return 0;
}

void *upc_rq_take(upc_rq *q)
{
    void *req;
    int i;
    if (q->nreads == 0)
        return 0;
    req = q->reads[0].req;
    for (i = 1; i < q->nreads; i++)
        q->reads[i - 1] = q->reads[i];
    q->nreads--;
    return req;
}

void upc_rq_clear(upc_rq *q)
{
    q->head = q->count = 0;
    if (q->paste_len > 0)
        paste_end(q);
    q->before = 0;
}

/* ---- event ring -------------------------------------------------------- */

void upc_ev_init(upc_evring *r)
{
    r->head = r->tail = 0;
    r->dropped = 0;
}

int upc_ev_count(const upc_evring *r)
{
    return (unsigned short)(r->head - r->tail);
}

int upc_ev_push(upc_evring *r, const upc_event *e)
{
    unsigned short h = r->head;
    if ((unsigned short)(h - r->tail) >= UPC_EVRING) {
        r->dropped++;
        return 0;
    }
    r->ev[h % UPC_EVRING] = *e;
    r->head = (unsigned short)(h + 1);      /* published after the copy */
    return 1;
}

int upc_ev_pop(upc_evring *r, upc_event *e)
{
    unsigned short t = r->tail;
    if (t == r->head)
        return 0;
    *e = r->ev[t % UPC_EVRING];
    r->tail = (unsigned short)(t + 1);
    return 1;
}

/* ---- routing ------------------------------------------------------------ */

static int find(const void *w, const void *const *wins, int nwins)
{
    int i;
    if (!w)
        return -1;
    for (i = 0; i < nwins; i++)
        if (wins[i] == w)
            return i;
    return -1;
}

int upc_route(int cls, const void *evaddr, const void *active, const void *const *wins, int nwins)
{
    int i;
    switch (cls) {
    case UPC_IE_RAWKEY:
    case UPC_IE_RAWMOUSE:
    case UPC_IE_POINTERPOS:
    case UPC_IE_NEWPOINTERPOS:
    case UPC_IE_TIMER:
        return find(active, wins, nwins);
    case UPC_IE_CLOSEWINDOW:
    case UPC_IE_SIZEWINDOW:
    case UPC_IE_REFRESHWINDOW:
    case UPC_IE_ACTIVEWINDOW:
    case UPC_IE_INACTIVEWINDOW:
    case UPC_IE_CHANGEWINDOW:
        i = find(evaddr, wins, nwins);
        return i >= 0 ? i : find(active, wins, nwins);
    default:
        return -1;
    }
}

/* ---- struct ConUnit ------------------------------------------------------ */

static void put8(unsigned char *cu, int off, int v)
{
    cu[off] = (unsigned char)v;
}

static void put16(unsigned char *cu, int off, int v)
{
    cu[off] = (unsigned char)((unsigned)v >> 8);
    cu[off + 1] = (unsigned char)v;
}

static void put32(unsigned char *cu, int off, unsigned long v)
{
    cu[off] = (unsigned char)(v >> 24);
    cu[off + 1] = (unsigned char)(v >> 16);
    cu[off + 2] = (unsigned char)(v >> 8);
    cu[off + 3] = (unsigned char)v;
}

static void setbit(unsigned char *bytes, int n)
{
    bytes[n >> 3] |= (unsigned char)(1 << (n & 7));
}

void upc_conunit_fill(unsigned char *cu, const upc_cu_state *s)
{
    int cols = s->cols > 0 ? s->cols : 1, rows = s->rows > 0 ? s->rows : 1;
    int x = s->x < 0 ? 0 : s->x >= cols ? cols - 1 : s->x;
    int y = s->y < 0 ? 0 : s->y >= rows ? rows - 1 : s->y;
    int i, n = 0;

    put32(cu, UPC_CU_WINDOW, s->window);
    put16(cu, UPC_CU_XCP, x);
    put16(cu, UPC_CU_YCP, y);
    put16(cu, UPC_CU_XMAX, cols - 1);
    put16(cu, UPC_CU_YMAX, rows - 1);
    put16(cu, UPC_CU_XRSIZE, s->cw);
    put16(cu, UPC_CU_YRSIZE, s->ch);
    put16(cu, UPC_CU_XRORIGIN, s->ox);
    put16(cu, UPC_CU_YRORIGIN, s->oy);
    put16(cu, UPC_CU_XREXTANT, s->ox + cols * s->cw - 1);
    put16(cu, UPC_CU_YREXTANT, s->oy + rows * s->ch - 1);
    put16(cu, UPC_CU_XMINSHRINK, s->minshrink_x);
    put16(cu, UPC_CU_YMINSHRINK, s->minshrink_y);
    put16(cu, UPC_CU_XCCP, x);
    put16(cu, UPC_CU_YCCP, y);

    /* tab stops: the columns, then 0xFFFF; at most MAXTABS entries in all */
    if (s->tabs) {
        for (i = 0; i < s->ntabs && n < UPC_CU_MAXTABS - 1; i++)
            put16(cu, UPC_CU_TABSTOPS + 2 * n++, s->tabs[i]);
    } else {
        /* the ROM's default list: every 8 from 0 for all but the last entry,
         * whatever the width (measured, D3.2 cudump, KS 40.63) */
        for (i = 0; n < UPC_CU_MAXTABS - 1; i += 8)
            put16(cu, UPC_CU_TABSTOPS + 2 * n++, i);
    }
    while (n < UPC_CU_MAXTABS)
        put16(cu, UPC_CU_TABSTOPS + 2 * n++, 0xFFFF);

    put8(cu, UPC_CU_MASK, s->mask);
    put8(cu, UPC_CU_FGPEN, s->fg);
    put8(cu, UPC_CU_BGPEN, s->bg);
    put8(cu, UPC_CU_AOLPEN, s->aol);
    put8(cu, UPC_CU_DRAWMODE, s->drawmode);
    put32(cu, UPC_CU_FONT, s->font);
    put8(cu, UPC_CU_ALGOSTYLE, s->algostyle);
    put8(cu, UPC_CU_TXFLAGS, s->txflags);
    put16(cu, UPC_CU_TXHEIGHT, s->txheight);
    put16(cu, UPC_CU_TXWIDTH, s->txwidth);
    put16(cu, UPC_CU_TXBASELINE, s->txbaseline);
    put16(cu, UPC_CU_TXSPACING, s->txspacing);

    for (i = 0; i < 3; i++) {
        cu[UPC_CU_MODES + i] = 0;
        cu[UPC_CU_RAWEVENTS + i] = 0;
    }
    if (s->lnm)
        setbit(cu + UPC_CU_MODES, UPC_CU_MODE_LNM);
    if (s->asm_)
        setbit(cu + UPC_CU_MODES, UPC_CU_MODE_ASM);
    if (s->awm)
        setbit(cu + UPC_CU_MODES, UPC_CU_MODE_AWM);
    for (i = 0; i < 24; i++)
        if (s->rawevents & (1UL << i))
            setbit(cu + UPC_CU_RAWEVENTS, i);
}

int upc_rom_cu_mask(int rom_version)
{
    return rom_version == 45 ? 0xFF : 1;
}
