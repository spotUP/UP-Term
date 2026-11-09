/* RIPscrip adapter (RIP-2): #included into vtengine.c after kitty.c, so it uses the image
 * store (image_install, image_place, image_mark, kit_room) and the reply path directly.
 * The pure RIP files (rip_parse.c, rip_pal.c, canvas.c, rip_draw.c) are included first and
 * know nothing of vt_term.
 *
 * With vt_set_rip(t, 1) every byte of vt_feed passes this adapter. It does two jobs:
 *  - detection and the switch, in the stream: ESC [ ! and ESC [ 0 ! answer RIPSCRIP015410,
 *    ESC [ 1 ! turns RIP off, ESC [ 2 ! on. They are taken before the engine's CSI parser
 *    because `!` is an intermediate byte there. A bare ESC [ ! also starts DECSTR (ESC [ ! p):
 *    the reply goes out at once (a BBS waits for it), and a `p` that follows is handed to
 *    the engine as the soft reset it is. The reply to a DECSTR is a stray, UNVERIFIED.
 *  - with RIP on, the bytes go through the RIP parser: text and its ANSI to the engine's
 *    normal feed, commands to the drawing model on the canvas.
 * With RIP off (the default) vt_feed is byte-identical to a build without this file.
 *
 * The canvas is an image of the store (kind 4, z = -1: under the text, index 0 opaque):
 * 640 x 350 bytes charged to the image budget (O12), created by the first command that
 * draws, placed at the top left without moving the cursor. The drawing model borrows
 * its pixels; before every use the adapter checks the slot still holds them. */

#define RIP_DETECT "RIPSCRIP015410"

struct rip_ad {
    struct rip_state st;
    struct rip_gfx g;
    struct rip_pal pal;
    struct cv *cv;
    int slot;
    vt_u16 gen;
    int active; /* RIP on (ESC [ 2 !) and not turned off (ESC [ 1 !) */
    int bang;   /* the last thing taken was a bare ESC [ ! : a `p` now is DECSTR */
    int nhold;
    vt_u8 hold[16]; /* a possible ESC [ digits ! , kept across vt_feed calls */
};

static void vt_feed_core(vt_term *t, const vt_u8 *buf, long len);

static int rip_canvas_alive(const vt_term *t, const struct rip_ad *ad)
{
    const vt_image *im;
    if (!ad->cv)
        return 0;
    im = t->imgs[ad->slot];
    return im && im->pix == ad->cv->pix && im->kind == 4 && im->gen == ad->gen;
}

static void rip_canvas_drop(struct rip_ad *ad)
{
    if (ad->cv) {
        cv_close(ad->cv); /* the pixels are the image's */
        ad->cv = 0;
    }
}

/* the canvas for drawing: made on first use, 0 when the budget or memory fails */
static struct cv *rip_canvas(vt_term *t, struct rip_ad *ad)
{
    vt_u8 *pix;
    long bytes = (long)CV_RIP_W * CV_RIP_H;
    vt_u32 pal[RIP_NPAL];
    int slot, i;
    vt_image *im;
    if (rip_canvas_alive(t, ad))
        return ad->cv;
    rip_canvas_drop(ad);
    if (!kit_room(t, bytes))
        return 0;
    pix = (vt_u8 *)VT_MALLOC(bytes);
    if (!pix)
        return 0;
    ad->cv = cv_open_pix(CV_RIP_W, CV_RIP_H, RIP_NPAL, pix);
    if (!ad->cv) {
        VT_FREE(pix);
        return 0;
    }
    rip_pal_reset(&ad->pal, ad->cv);
    for (i = 0; i < RIP_NPAL; i++)
        pal[i] = (vt_u32)ad->cv->pal[i];
    slot = image_install(t, pix, bytes, CV_RIP_W, CV_RIP_H, RIP_NPAL, pal); /* takes pix, frees it on failure */
    if (slot < 0) {
        rip_canvas_drop(ad);
        return 0;
    }
    im = t->imgs[slot];
    im->kind = 4;
    im->kz = -1;
    ad->slot = slot;
    ad->gen = im->gen;
    image_place(t, slot, 1);
    if (!rip_canvas_alive(t, ad)) { /* nowhere on the screen: the store dropped it */
        rip_canvas_drop(ad);
        return 0;
    }
    rip_gfx_init(&ad->g);
    return ad->cv;
}

static void rip_on_cmd(void *ud, const struct rip_cmd *c)
{
    vt_term *t = (vt_term *)ud;
    struct rip_ad *ad = t->rip;
    struct cv *cv;
    if (!rip_gfx_handles(c) && !(c->level == 0 && (c->ch == 'Q' || c->ch == 'a')))
        return; /* windows, text, buttons, queries: RIP-5 and later */
    cv = rip_canvas(t, ad);
    if (!cv)
        return;
    if (!rip_pal_exec(&ad->pal, cv, c))
        rip_gfx_exec(&ad->g, cv, c);
}

static void rip_on_text(void *ud, const unsigned char *p, long n)
{
    vt_feed_core((vt_term *)ud, p, n);
}

/* the canvas changed: its image is a new picture and its cells are drawn again */
static void rip_settle(vt_term *t, struct rip_ad *ad)
{
    vt_image *im;
    int i;
    if (!ad->cv)
        return;
    if (!rip_canvas_alive(t, ad)) {
        rip_canvas_drop(ad);
        return;
    }
    if (!cv_flush(ad->cv))
        return;
    (void)cv_take_dirty(ad->cv);
    im = t->imgs[ad->slot];
    for (i = 0; i < RIP_NPAL; i++)
        im->pal[i] = (vt_u32)ad->cv->pal[i];
    im->serial = ++t->img_serial;
    image_mark(t, ad->slot);
}

/* ESC [ digits ! complete in hold[0..n): take it. */
static void rip_switch(vt_term *t, struct rip_ad *ad, int n)
{
    long v = 0;
    int i;
    for (i = 2; i < n - 1; i++)
        v = v * 10 + (ad->hold[i] - '0');
    if (n == 3 || v == 0) {
        reply(t, RIP_DETECT, (int)sizeof(RIP_DETECT) - 1);
        ad->bang = n == 3; /* ESC [ ! may be the head of DECSTR */
    } else if (v == 1) {
        ad->active = 0;
    } else if (v == 2) {
        ad->active = 1;
    }
    ad->nhold = 0;
}

static void rip_pass(vt_term *t, struct rip_ad *ad, const vt_u8 *p, long n)
{
    struct rip_sinks sk;
    if (n <= 0)
        return;
    if (!ad->active) {
        vt_feed_core(t, p, n);
        return;
    }
    sk.ud = t;
    sk.text = rip_on_text;
    sk.cmd = rip_on_cmd;
    rip_feed(&ad->st, p, n, &sk);
}

/* The bytes of vt_feed with RIP set up. An ESC at a command boundary (the parser between
 * commands, or RIP off) is held until it is known not to be ESC [ digits !. */
static void rip_run(vt_term *t, struct rip_ad *ad, const vt_u8 *buf, long len)
{
    long i = 0, from = 0;
    while (i < len) {
        vt_u8 b = buf[i];
        if (ad->nhold) {
            int n = ad->nhold;
            if ((n == 1 && b == '[') || (n >= 2 && n < 8 && b >= '0' && b <= '9') || (n >= 2 && b == '!')) {
                ad->hold[ad->nhold++] = b;
                i++;
                from = i;
                if (b == '!')
                    rip_switch(t, ad, ad->nhold);
                continue;
            }
            n = ad->nhold; /* not ours: the held bytes go on as they came */
            ad->nhold = 0;
            rip_pass(t, ad, ad->hold, n);
            continue;
        }
        if (ad->bang) {
            ad->bang = 0;
            if (b == 'p') { /* ESC [ ! p was DECSTR */
                vt_feed_core(t, (const vt_u8 *)"\033[!p", 4);
                i++;
                from = i;
                continue;
            }
        }
        if (b == 0x1B && (!ad->active || rip_in_text(&ad->st))) {
            rip_pass(t, ad, buf + from, i - from);
            ad->hold[0] = b;
            ad->nhold = 1;
            i++;
            from = i;
            continue;
        }
        i++;
    }
    rip_pass(t, ad, buf + from, len - from);
}

void vt_feed(vt_term *t, const vt_u8 *buf, long len)
{
    struct rip_ad *ad = (struct rip_ad *)t->rip;
    if (!ad) {
        vt_feed_core(t, buf, len); /* RIP off: nothing here runs */
        return;
    }
    rip_run(t, ad, buf, len);
    rip_settle(t, ad);
}

void vt_set_rip(vt_term *t, int on)
{
    struct rip_ad *ad = t->rip;
    if (!on) {
        if (ad) {
            rip_canvas_drop(ad);
            VT_FREE(ad);
            t->rip = 0;
        }
        return;
    }
    if (ad)
        return;
    ad = (struct rip_ad *)VT_MALLOC(sizeof(*ad));
    if (!ad)
        return;
    memset(ad, 0, sizeof(*ad));
    rip_init(&ad->st);
    rip_gfx_init(&ad->g);
    ad->active = 1;
    t->rip = ad;
}

long vt_rip_commands(const vt_term *t)
{
    return t->rip ? rip_commands(&((struct rip_ad *)t->rip)->st) : 0;
}

static void rip_free(vt_term *t)
{
    if (t->rip) {
        rip_canvas_drop((struct rip_ad *)t->rip);
        VT_FREE(t->rip);
        t->rip = 0;
    }
}
