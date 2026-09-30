/* The console.device core (device/upc_core.c): read queue, event ring,
 * routing, struct ConUnit fill. */
#include "harness.h"
#include "../device/upc_core.h"

/* ---- read queue --------------------------------------------------------- */

static struct { void *req; char data[700]; long actual; } replies[16];
static int n_replies;
static const unsigned char *pastes_done[4];
static int n_pastes_done;
static unsigned char bufs[10][700];     /* a read's buffer; req = &bufs[i] */

static void on_reply(void *user, void *req, long actual)
{
    (void)user;
    if (n_replies < 16) {
        replies[n_replies].req = req;
        replies[n_replies].actual = actual;
        memcpy(replies[n_replies].data, req, (size_t)actual);
        replies[n_replies].data[actual] = 0;
        n_replies++;
    }
}

static void on_paste_done(void *user, const unsigned char *text)
{
    (void)user;
    if (n_pastes_done < 4)
        pastes_done[n_pastes_done++] = text;
}

static upc_rq Q;

static void fresh(void)
{
    upc_rq_init(&Q, on_reply, on_paste_done, 0);
    n_replies = 0;
    n_pastes_done = 0;
    memset(bufs, 0, sizeof(bufs));
}

static void in(const char *s)
{
    upc_rq_input(&Q, (const unsigned char *)s, (long)strlen(s));
}

/* A read of len into bufs[i]: the bytes it got at once, or "<queued>" /
 * "<full>". */
static const char *rd(int i, long len)
{
    static char out[700];
    long actual;
    int r = upc_rq_read(&Q, bufs[i], bufs[i], len, &actual);
    if (r == UPC_READ_QUEUED)
        return "<queued>";
    if (r == UPC_READ_FULL)
        return "<full>";
    memcpy(out, bufs[i], (size_t)actual);
    out[actual] = 0;
    return out;
}

static void quick_reads(void)
{
    fresh();
    in("abc");
    CHECK_INT(upc_rq_available(&Q), 3);
    CHECK_STR(rd(0, 16), "abc");                /* partial satisfy: what is there */
    CHECK_INT(upc_rq_available(&Q), 0);
    in("hello");
    CHECK_STR(rd(0, 2), "he");                  /* never more than io_Length */
    CHECK_STR(rd(1, 16), "llo");                /* the rest, in order */
    CHECK_INT(Q.answered, 3);
    CHECK_INT(n_replies, 0);                    /* quick: no callback */
    fresh();
    CHECK_STR(rd(0, 0), "");                    /* length 0: answered at once */
    CHECK_INT(Q.nreads, 0);
}

static void queued_reads(void)
{
    fresh();
    CHECK_STR(rd(0, 16), "<queued>");           /* no input: waits */
    CHECK_INT(n_replies, 0);
    in("ab");
    CHECK_INT(n_replies, 1);
    CHECK(replies[0].req == bufs[0]);
    CHECK_INT(replies[0].actual, 2);
    CHECK_STR(replies[0].data, "ab");
    CHECK_INT(Q.nreads, 0);

    fresh();                                    /* two queued: oldest first */
    CHECK_STR(rd(0, 2), "<queued>");
    CHECK_STR(rd(1, 10), "<queued>");
    in("hello");
    CHECK_INT(n_replies, 2);
    CHECK(replies[0].req == bufs[0]);
    CHECK_STR(replies[0].data, "he");
    CHECK(replies[1].req == bufs[1]);
    CHECK_STR(replies[1].data, "llo");

    fresh();                                    /* one input byte answers one read */
    CHECK_STR(rd(0, 4), "<queued>");
    CHECK_STR(rd(1, 4), "<queued>");
    in("x");
    CHECK_INT(n_replies, 1);
    CHECK_INT(Q.nreads, 1);
    CHECK_STR(rd(2, 4), "<queued>");            /* a newer read waits behind the older */
    in("yz");
    CHECK_INT(n_replies, 2);
    CHECK(replies[1].req == bufs[1]);
    CHECK_STR(replies[1].data, "yz");
    CHECK_INT(Q.nreads, 1);
    CHECK_STR(rd(3, 0), "");                    /* length 0 does not queue behind them */

    fresh();                                    /* the queue has a limit */
    {
        int i;
        for (i = 0; i < UPC_MAXREADS; i++)
            CHECK_STR(rd(i, 4), "<queued>");
        CHECK_STR(rd(9, 4), "<full>");
        CHECK_INT(Q.nreads, UPC_MAXREADS);
    }
}

static void abort_take_clear(void)
{
    fresh();
    rd(0, 4);
    rd(1, 4);
    rd(2, 4);
    CHECK_INT(upc_rq_abort(&Q, bufs[1]), 1);    /* AbortIO of a queued read */
    CHECK_INT(Q.nreads, 2);
    CHECK_INT(upc_rq_abort(&Q, bufs[1]), 0);    /* not queued any more */
    CHECK_INT(upc_rq_abort(&Q, bufs[7]), 0);
    in("abcdefgh");
    CHECK_INT(n_replies, 2);
    CHECK(replies[0].req == bufs[0]);
    CHECK(replies[1].req == bufs[2]);           /* the aborted one got nothing */
    CHECK_STR(replies[1].data, "efgh");

    fresh();                                    /* CloseDevice: take them all, oldest first */
    rd(0, 4);
    rd(1, 4);
    CHECK(upc_rq_take(&Q) == bufs[0]);
    CHECK(upc_rq_take(&Q) == bufs[1]);
    CHECK(upc_rq_take(&Q) == 0);
    in("z");
    CHECK_INT(n_replies, 0);
    CHECK_INT(upc_rq_available(&Q), 1);

    fresh();                                    /* CMD_CLEAR drops input, keeps reads */
    in("stale");
    CHECK_INT(upc_rq_available(&Q), 5);
    upc_rq_clear(&Q);
    CHECK_INT(upc_rq_available(&Q), 0);
    CHECK_STR(rd(0, 8), "<queued>");
    upc_rq_clear(&Q);
    CHECK_INT(Q.nreads, 1);
    in("new");
    CHECK_INT(n_replies, 1);
    CHECK_STR(replies[0].data, "new");
}

static void overflow(void)
{
    static unsigned char big[600];
    int i;
    for (i = 0; i < 600; i++)
        big[i] = (unsigned char)('a' + i % 26);
    fresh();                                    /* no reader: the buffer's size is kept */
    CHECK_INT(upc_rq_input(&Q, big, 600), UPC_INBUF);
    CHECK_INT(Q.dropped, 600 - UPC_INBUF);
    CHECK_INT(upc_rq_available(&Q), UPC_INBUF);
    rd(0, 700);
    CHECK(!memcmp(bufs[0], big, UPC_INBUF));    /* the oldest bytes, in order */
    CHECK_INT(upc_rq_available(&Q), 0);

    fresh();                                    /* a waiting reader makes room mid-input */
    rd(0, 100);
    CHECK_INT(upc_rq_input(&Q, big, 600), 600);
    CHECK_INT(Q.dropped, 0);
    CHECK_INT(replies[0].actual, 100);
    CHECK(!memcmp(replies[0].data, big, 100));
    CHECK_INT(upc_rq_available(&Q), 500);
    rd(1, 700);
    CHECK(!memcmp(bufs[1], big + 100, 500));
}

static void paste(void)
{
    static const unsigned char text[] = "PASTE";
    fresh();                                    /* between what was typed before and after */
    in("ab");
    CHECK_INT(upc_rq_paste(&Q, text, 5), 1);
    in("cd");
    CHECK_INT(upc_rq_available(&Q), 9);
    CHECK_STR(rd(0, 3), "abP");
    CHECK_INT(n_pastes_done, 0);                /* still referenced */
    CHECK_STR(rd(1, 3), "AST");
    in("e");                                    /* typed while it drains: still after it */
    CHECK_STR(rd(2, 3), "Ecd");
    CHECK_INT(n_pastes_done, 1);
    CHECK(pastes_done[0] == text);
    CHECK_STR(rd(3, 3), "e");

    fresh();                                    /* one read takes it all */
    in("x");
    upc_rq_paste(&Q, text, 5);
    in("y");
    CHECK_STR(rd(0, 50), "xPASTEy");
    CHECK_INT(n_pastes_done, 1);

    fresh();                                    /* a waiting read gets it at once */
    rd(0, 50);
    CHECK_INT(upc_rq_paste(&Q, text, 5), 1);
    CHECK_INT(n_replies, 1);
    CHECK_STR(replies[0].data, "PASTE");
    CHECK_INT(n_pastes_done, 1);

    fresh();                                    /* one paste at a time */
    CHECK_INT(upc_rq_paste(&Q, text, 5), 1);
    CHECK_INT(upc_rq_paste(&Q, (const unsigned char *)"other", 5), 0);
    CHECK_STR(rd(0, 50), "PASTE");
    CHECK_INT(upc_rq_paste(&Q, (const unsigned char *)"other", 5), 1);  /* drained: next one */
    CHECK_STR(rd(0, 50), "other");

    fresh();                                    /* CMD_CLEAR drops it and lets it go */
    in("k");
    upc_rq_paste(&Q, text, 5);
    upc_rq_clear(&Q);
    CHECK_INT(n_pastes_done, 1);
    CHECK_INT(upc_rq_available(&Q), 0);
    in("j");
    CHECK_STR(rd(0, 50), "j");

    fresh();                                    /* empty paste: nothing in the stream */
    CHECK_INT(upc_rq_paste(&Q, text, 0), 1);
    CHECK_INT(n_pastes_done, 1);
    CHECK_INT(upc_rq_available(&Q), 0);
}

/* ---- event ring ---------------------------------------------------------- */

static upc_event ev(int code)
{
    upc_event e;
    memset(&e, 0, sizeof(e));
    e.cls = UPC_IE_RAWKEY;
    e.code = (unsigned short)code;
    return e;
}

static void event_ring(void)
{
    static upc_evring R;
    upc_event e, got;
    int i, ok = 1;
    upc_ev_init(&R);
    CHECK_INT(upc_ev_pop(&R, &got), 0);         /* empty */
    e = ev(0x20);
    e.qual = 0x8001;
    e.x = -3;
    e.addr = &R;
    CHECK_INT(upc_ev_push(&R, &e), 1);
    CHECK_INT(upc_ev_count(&R), 1);
    CHECK_INT(upc_ev_pop(&R, &got), 1);
    CHECK_INT(got.code, 0x20);                  /* copied whole */
    CHECK_INT(got.qual, 0x8001);
    CHECK_INT(got.x, -3);
    CHECK(got.addr == &R);

    upc_ev_init(&R);                            /* 64 fit, the 65th is dropped and counted */
    for (i = 0; i < UPC_EVRING; i++) {
        e = ev(i);
        ok &= upc_ev_push(&R, &e);
    }
    CHECK(ok);
    e = ev(999);
    CHECK_INT(upc_ev_push(&R, &e), 0);
    CHECK_INT(R.dropped, 1);
    CHECK_INT(upc_ev_count(&R), UPC_EVRING);
    upc_ev_pop(&R, &got);
    CHECK_INT(got.code, 0);                     /* oldest first */
    e = ev(64);
    CHECK_INT(upc_ev_push(&R, &e), 1);          /* room again */
    for (i = 1; i <= UPC_EVRING; i++) {
        ok &= upc_ev_pop(&R, &got) && got.code == i;
    }
    CHECK(ok);
    CHECK_INT(upc_ev_count(&R), 0);
    CHECK_INT(R.dropped, 1);

    upc_ev_init(&R);                            /* the 16-bit counters wrap */
    R.head = R.tail = 65530;
    for (i = 0; i < 20; i++) {
        e = ev(i);
        ok &= upc_ev_push(&R, &e);
    }
    CHECK(ok);
    CHECK_INT(upc_ev_count(&R), 20);
    for (i = 0; i < 20; i++)
        ok &= upc_ev_pop(&R, &got) && got.code == i;
    CHECK(ok);
    CHECK_INT(upc_ev_pop(&R, &got), 0);
    CHECK_INT(R.dropped, 0);
}

/* ---- routing ------------------------------------------------------------- */

static void route(void)
{
    int w0, w1, w2, foreign;            /* stand-ins for struct Window */
    const void *wins[4];
    wins[0] = &w0;
    wins[1] = 0;                        /* a free slot */
    wins[2] = &w1;
    wins[3] = &w2;
    /* keys, mouse, timer: the active window's unit */
    CHECK_INT(upc_route(UPC_IE_RAWKEY, 0, &w1, wins, 4), 2);
    CHECK_INT(upc_route(UPC_IE_RAWMOUSE, &w0, &w2, wins, 4), 3);  /* the address does not matter */
    CHECK_INT(upc_route(UPC_IE_TIMER, 0, &w0, wins, 4), 0);
    CHECK_INT(upc_route(UPC_IE_RAWKEY, 0, &foreign, wins, 4), -1);
    CHECK_INT(upc_route(UPC_IE_RAWKEY, 0, 0, wins, 4), -1);        /* no active window */
    /* window events: the addressed window, else the active one */
    CHECK_INT(upc_route(UPC_IE_SIZEWINDOW, &w2, &w0, wins, 4), 3);
    CHECK_INT(upc_route(UPC_IE_REFRESHWINDOW, &w1, &foreign, wins, 4), 2);
    CHECK_INT(upc_route(UPC_IE_CLOSEWINDOW, &w0, 0, wins, 4), 0);
    CHECK_INT(upc_route(UPC_IE_ACTIVEWINDOW, &w1, &w1, wins, 4), 2);
    CHECK_INT(upc_route(UPC_IE_INACTIVEWINDOW, &w2, &w0, wins, 4), 3);
    CHECK_INT(upc_route(UPC_IE_CHANGEWINDOW, &w1, &w0, wins, 4), 2);
    CHECK_INT(upc_route(UPC_IE_SIZEWINDOW, &foreign, &w2, wins, 4), 3); /* not ours: active */
    CHECK_INT(upc_route(UPC_IE_SIZEWINDOW, 0, &w0, wins, 4), 0);
    CHECK_INT(upc_route(UPC_IE_SIZEWINDOW, &foreign, &foreign, wins, 4), -1);
    CHECK_INT(upc_route(UPC_IE_ACTIVEWINDOW, 0, 0, wins, 4), -1);  /* a free slot never matches 0 */
    /* anything else: no unit */
    CHECK_INT(upc_route(0x04, &w0, &w0, wins, 4), -1);             /* POINTERPOS */
    CHECK_INT(upc_route(0x07, &w0, &w0, wins, 4), -1);             /* GADGETDOWN */
    CHECK_INT(upc_route(0x0A, &w0, &w0, wins, 4), -1);             /* MENULIST */
    CHECK_INT(upc_route(UPC_IE_RAWKEY, 0, &w0, wins, 0), -1);      /* no units */
}

/* ---- struct ConUnit ------------------------------------------------------ */

/* Offsets written out again from the conformance matrix 6.4 table (not
 * from upc_core.h): a typo on either side fails here. */
static unsigned char CU[0x128 + 16];

static long be16(int off)
{
    return (long)(CU[off] << 8 | CU[off + 1]);
}

static long sbe16(int off)
{
    long v = be16(off);
    return v >= 0x8000 ? v - 0x10000 : v;
}

static unsigned long be32(int off)
{
    return (unsigned long)CU[off] << 24 | (unsigned long)CU[off + 1] << 16
         | (unsigned long)CU[off + 2] << 8 | CU[off + 3];
}

static int untouched(int from, int to)
{
    int i;
    for (i = from; i < to; i++)
        if (CU[i] != 0xAA)
            return 0;
    return 1;
}

static upc_cu_state state(void)
{
    upc_cu_state s;
    memset(&s, 0, sizeof(s));
    s.window = 0x00C01234UL;
    s.font = 0x00200ABCUL;
    s.cols = 80;
    s.rows = 25;
    s.x = 5;
    s.y = 7;
    s.cw = 8;
    s.ch = 8;
    s.ox = 4;
    s.oy = 11;
    s.minshrink_x = 79;
    s.minshrink_y = 24;
    s.mask = 3;
    s.fg = 1;
    s.bg = 0;
    s.aol = 1;
    s.drawmode = 1;
    s.algostyle = 2;
    s.txflags = 0x41;
    s.txheight = 8;
    s.txwidth = 8;
    s.txbaseline = 6;
    s.txspacing = -1;
    s.asm_ = 1;
    s.awm = 1;
    return s;
}

static void conunit_fill(void)
{
    upc_cu_state s = state();
    int i, ok = 1;
    memset(CU, 0xAA, sizeof(CU));
    upc_conunit_fill(CU, &s);
    CHECK_INT(be32(0x022), 0x00C01234L);        /* cu_Window */
    CHECK_INT(be16(0x026), 5);                  /* cu_XCP */
    CHECK_INT(be16(0x028), 7);                  /* cu_YCP */
    CHECK_INT(be16(0x02A), 79);                 /* cu_XMax */
    CHECK_INT(be16(0x02C), 24);                 /* cu_YMax */
    CHECK_INT(be16(0x02E), 8);                  /* cu_XRSize */
    CHECK_INT(be16(0x030), 8);                  /* cu_YRSize */
    CHECK_INT(be16(0x032), 4);                  /* cu_XROrigin */
    CHECK_INT(be16(0x034), 11);                 /* cu_YROrigin */
    CHECK_INT(be16(0x036), 4 + 80 * 8 - 1);     /* cu_XRExtant */
    CHECK_INT(be16(0x038), 11 + 25 * 8 - 1);    /* cu_YRExtant */
    CHECK_INT(be16(0x03A), 79);                 /* cu_XMinShrink */
    CHECK_INT(be16(0x03C), 24);                 /* cu_YMinShrink */
    CHECK_INT(be16(0x03E), 5);                  /* cu_XCCP */
    CHECK_INT(be16(0x040), 7);                  /* cu_YCCP */
    for (i = 0; i < 79; i++)                    /* default tabs: 79 stops every 8 from 0, */
        ok &= be16(0x062 + 2 * i) == 8 * i;     /* whatever the width (the ROM's, D3.2 cudump) */
    ok &= be16(0x062 + 2 * 79) == 0xFFFF;       /* then 0xFFFF */
    CHECK(ok);
    CHECK_INT(CU[0x102], 3);                    /* cu_Mask */
    CHECK_INT(CU[0x103], 1);                    /* cu_FgPen */
    CHECK_INT(CU[0x104], 0);                    /* cu_BgPen */
    CHECK_INT(CU[0x105], 1);                    /* cu_AOLPen */
    CHECK_INT(CU[0x106], 1);                    /* cu_DrawMode */
    CHECK_INT(be32(0x114), 0x00200ABCL);        /* cu_Font */
    CHECK_INT(CU[0x118], 2);                    /* cu_AlgoStyle */
    CHECK_INT(CU[0x119], 0x41);                 /* cu_TxFlags */
    CHECK_INT(be16(0x11A), 8);                  /* cu_TxHeight */
    CHECK_INT(be16(0x11C), 8);                  /* cu_TxWidth */
    CHECK_INT(be16(0x11E), 6);                  /* cu_TxBaseline */
    CHECK_INT(sbe16(0x120), -1);                /* cu_TxSpacing, signed */
    CHECK_INT(CU[0x122], 0);                    /* cu_Modes: bits 20-22 are in byte 2 */
    CHECK_INT(CU[0x123], 0);
    CHECK_INT(CU[0x124], 0x60);                 /* ASM bit 21, AWM bit 22; LNM off */
    CHECK_INT(CU[0x125], 0);                    /* no raw events asked */
    CHECK_INT(CU[0x126], 0);
    CHECK_INT(CU[0x127], 0);
    /* what the fill must not write */
    CHECK(untouched(0x000, 0x022));             /* cu_MP */
    CHECK(untouched(0x042, 0x062));             /* cu_KeyMapStruct: CD_SETKEYMAP's */
    CHECK(untouched(0x107, 0x114));             /* cu_Obsolete1/2, cu_Minterms */
    CHECK(untouched(0x128, 0x128 + 16));        /* nothing past sizeof(struct ConUnit) */

    s = state();                                /* modes and raw events, bit per number */
    s.lnm = 1;
    s.asm_ = 0;
    s.awm = 0;
    s.rawevents = (1UL << 1) | (1UL << 12) | (1UL << 23) | (1UL << 24) | (1UL << 31);
    memset(CU, 0xAA, sizeof(CU));
    upc_conunit_fill(CU, &s);
    CHECK_INT(CU[0x124], 0x10);                 /* LNM bit 20 */
    CHECK_INT(CU[0x125], 0x02);                 /* class 1 RAWKEY */
    CHECK_INT(CU[0x126], 0x10);                 /* class 12 SIZEWINDOW */
    CHECK_INT(CU[0x127], 0x80);                 /* class 23, the last that fits */
    CHECK(untouched(0x128, 0x128 + 16));        /* classes 24+ are dropped, not spilled */

    s = state();                                /* the cursor is clamped to the grid */
    s.x = 200;
    s.y = -4;
    s.cols = 40;
    s.rows = 10;
    memset(CU, 0xAA, sizeof(CU));
    upc_conunit_fill(CU, &s);
    CHECK_INT(be16(0x026), 39);
    CHECK_INT(be16(0x028), 0);
    CHECK_INT(be16(0x03E), 39);
    CHECK_INT(be16(0x040), 0);
    CHECK_INT(be16(0x062 + 2 * 5), 40);         /* default tabs do not stop at the grid's width */
    CHECK_INT(be16(0x062 + 2 * 78), 624);

    s = state();                                /* the unit's own tab stops */
    {
        static const unsigned short tabs[] = { 0, 4, 10, 30 };
        s.tabs = tabs;
        s.ntabs = 4;
        memset(CU, 0xAA, sizeof(CU));
        upc_conunit_fill(CU, &s);
        CHECK_INT(be16(0x062), 0);
        CHECK_INT(be16(0x064), 4);
        CHECK_INT(be16(0x066), 10);
        CHECK_INT(be16(0x068), 30);
        CHECK_INT(be16(0x06A), 0xFFFF);
    }
    {
        static unsigned short many[120];        /* more than fit: 79 kept, 0xFFFF last */
        for (i = 0; i < 120; i++)
            many[i] = (unsigned short)(2 * i);
        s.tabs = many;
        s.ntabs = 120;
        s.cols = 240;
        memset(CU, 0xAA, sizeof(CU));
        upc_conunit_fill(CU, &s);
        CHECK_INT(be16(0x062 + 2 * 78), 156);
        CHECK_INT(be16(0x062 + 2 * 79), 0xFFFF);
        CHECK_INT(CU[0x102], 3);                /* the list did not run into cu_Mask */
    }
}

void suite_upcon(void)
{
    quick_reads();
    queued_reads();
    abort_take_clear();
    overflow();
    paste();
    event_ring();
    route();
    conunit_fill();
}
