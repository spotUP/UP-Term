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
 * its pixels; before every use the adapter checks the slot still holds them.
 *
 * RIP-5 text windows: the RIP text window (w) is the engine's scroll region with the
 * origin mode on, so ANSI text lands inside it and scrolls there. DECSLRM does not exist
 * in the engine, so only the rows count: x0 and x1 of w are not used (exempt, UNVERIFIED
 * against a real BBS). Image placements belong to lines, so a region scroll carries the
 * canvas tiles with the lines; after text the adapter puts the tiles back on rows 0.. of
 * the grid (rip_reanchor): the canvas does not scroll.
 * RIP-6 mouse: the fields live in rip_mouse.c; vt_rip_click and vt_rip_key give the
 * expanded host command, the way vt_encode_mouse_px gives a report. */

#define RIP_DETECT "RIPSCRIP015410"

/* Auto mode (vt_set_rip(t, 2)) holds a line that starts with `!|` until its end, then asks
 * the RIP parser whether it is valid. A line longer than RIP_AUTO_LINE bytes is given up:
 * what was held goes to the terminal as text, byte for byte, and so does the rest of that
 * line (it is not a line start any more). An over-long line therefore never turns RIP on.
 * Only a line that starts with `!|` is ever held; every other byte stays on the fast path. */
#define RIP_AUTO_LINE 4096

struct rip_ad {
    struct rip_state st;
    struct rip_gfx g;
    struct rip_pal pal;
    struct cv *cv;
    struct cv_rect_s dirty; /* canvas area changed since vt_rip_take_dirty */
    int slot;
    vt_u16 gen;
    int mode;   /* vt_set_rip argument: 1 On, 2 Auto */
    int active; /* RIP on (ESC [ 2 !) and not turned off (ESC [ 1 !) */
    int bol;    /* Auto: the stream is at a line start (the last byte passed was CR or LF) */
    int lbs;    /* Auto: backslash state of the held line, as the parser's bs (0, 1, 2) */
    int lsoh;   /* Auto: the newline byte after a backslash */
    int nline;  /* Auto: bytes of a candidate `!|` line held in line[] */
    int bang;   /* the last thing taken was a bare ESC [ ! : a `p` now is DECSTR */
    int nhold;
    vt_u8 hold[16]; /* a possible ESC [ digits ! , kept across vt_feed calls */
    int stroke;     /* host option: Hershey strokes for fonts 1..4 (vt_set_rip_stroke) */
    long took;      /* bytes the last rip_pass consumed (less than n after a feed stop) */
    long blocks;    /* level-9 transfers handed to cb.rip_block (sentinel) */
    int moved;      /* text went to the engine since the tiles were last put right */
    struct rip_mouse mouse;
    struct rip_clip clip; /* 1C / 1I clipboard */
    vt_u8 line[RIP_AUTO_LINE]; /* Auto: the `!|` line being held */
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

static void rip_dirty_all(struct rip_ad *ad)
{
    struct cv_rect_s all;
    all.x0 = 0, all.y0 = 0, all.x1 = CV_RIP_W, all.y1 = CV_RIP_H;
    cv_rect_union(&ad->dirty, &all);
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
    rip_dirty_all(ad); /* a new canvas: everything is new to the host */
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
    ad->g.stroke = ad->stroke;
    return ad->cv;
}

/* the screen of the text grid: the whole of it, every row */
static void rip_clear_grid(vt_term *t)
{
    int y;
    for (y = 0; y < t->rows; y++)
        erase_cells(t, y, 0, t->cols);
}

/* The text window commands. Coordinates are grid cells from 0; the window is the scroll
 * region (rows) with the origin mode on. Returns 1 when the command was one of them. */
static int rip_window(vt_term *t, const struct rip_cmd *c)
{
    int y, top, bot;
    switch (c->ch) {
    case 'w':
        if (c->nargs < 5)
            return 1;
        top = (int)c->arg[1];
        bot = (int)c->arg[3] + 1;
        if (top > bot - 1) {
            y = top;
            top = bot - 1;
            bot = y + 1;
        }
        if (top < 0)
            top = 0;
        if (bot > t->rows)
            bot = t->rows;
        if (top < bot - 1) {
            t->top = top;
            t->bot = bot;
            t->origin = 1;
            t->autowrap = c->arg[4] != 0;
            move_to(t, 0, t->top);
        }
        return 1;
    case '*':
        t->top = 0;
        t->bot = t->rows;
        t->origin = 0;
        t->autowrap = 1;
        rip_clear_grid(t);
        move_to(t, 0, 0);
        return 0; /* the drawing model clears its viewport and the canvas too */
    case 'e':
        for (y = t->top; y < t->bot; y++)
            erase_cells(t, y, 0, t->cols);
        move_to(t, 0, t->origin ? t->top : 0);
        return 1;
    case 'g':
        if (c->nargs >= 2)
            move_to(t, (int)c->arg[0], (t->origin ? t->top : 0) + (int)c->arg[1]);
        return 1;
    case 'H':
        move_to(t, 0, t->origin ? t->top : 0);
        return 1;
    case '>':
        erase_cells(t, t->cy, t->cx, t->cols);
        return 1;
    }
    return 0;
}

/* icon files come through the host's file hook (vt_set_file_reader) by their sanitised
 * name alone; the host decides the directory. Needs the release function too (kitty rule). */
static int rip_icon_load(void *ud, const char *name, unsigned char **buf, long *n)
{
    vt_term *t = (vt_term *)ud;
    if (!t->file_read || !t->decode_free)
        return 0;
    return t->file_read(name, 0, 0, buf, n);
}

static void rip_icon_release(void *ud, unsigned char *buf)
{
    vt_term *t = (vt_term *)ud;
    if (t->decode_free)
        t->decode_free((vt_u32 *)(void *)buf);
}

static void rip_on_cmd(void *ud, const struct rip_cmd *c)
{
    vt_term *t = (vt_term *)ud;
    struct rip_ad *ad = t->rip;
    struct cv *cv;
    if (c->level == 9 && c->ch == 27) { /* block transfer: the host runs the protocol */
        char name[RIP_TEXT_MAX + 1];
        if (t->cb.rip_block && c->nargs >= 3) {
            dl_clean_name((const char *)c->text, c->ntext, name, (int)sizeof name, 0);
            ad->blocks++;
            t->cb.rip_block(t->user, (int)c->arg[0], (int)c->arg[1], (int)c->arg[2], name);
        }
        return;
    }
    if (c->level == 0 && (c->ch == 'w' || c->ch == '*' || c->ch == 'e' || c->ch == 'g' || c->ch == 'H' ||
                          c->ch == '>')) {
        if (c->ch == '*')
            ad->mouse.n = 0; /* reset windows kills the mouse fields too */
        if (rip_window(t, c))
            return;
    } else if (rip_mouse_handles(c)) {
        cv = c->ch == 'U' ? rip_canvas(t, ad) : 0; /* only a button draws */
        rip_mouse_exec(&ad->mouse, &ad->g, cv, c);
        return;
    }
    if (rip_clip_handles(c)) {
        struct rip_icon_src src;
        src.ud = t;
        src.load = rip_icon_load;
        src.release = rip_icon_release;
        cv = rip_canvas(t, ad);
        if (cv)
            rip_clip_exec(&ad->clip, &ad->g, cv, c, &src);
        return;
    }
    if (!rip_gfx_handles(c) && !(c->level == 0 && (c->ch == 'Q' || c->ch == 'a')))
        return; /* queries, files: RIP-8 and later */
    cv = rip_canvas(t, ad);
    if (!cv)
        return;
    if (!rip_pal_exec(&ad->pal, cv, c))
        rip_gfx_exec(&ad->g, cv, c);
}

static void rip_on_text(void *ud, const unsigned char *p, long n)
{
    struct rip_ad *ad = ((vt_term *)ud)->rip;
    ad->moved = 1;
    vt_feed_core((vt_term *)ud, p, n);
}

/* The canvas tiles back on grid rows 0..: a scroll of the text window carries them with the
 * lines they were on. place_add replaces the tile of the same image on a line; the extra
 * reference keeps the image alive while a line lets go of its old placement. */
static void rip_reanchor(vt_term *t, struct rip_ad *ad)
{
    vt_image *im = t->imgs[ad->slot];
    int k, rows = (im->py0 + im->h + im->ch - 1) / im->ch;
    rip_dirty_all(ad); /* the tiles moved: the host redraws the picture whole */
    im->refs++;
    for (k = 0; k < rows && k < t->rows; k++)
        sixel_place_row(t, ad->slot, k, 0, k);
    im->refs--;
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
    if (ad->moved) {
        ad->moved = 0;
        rip_reanchor(t, ad);
    }
    if (!cv_flush(ad->cv))
        return;
    {
        struct cv_rect_s d = cv_take_dirty(ad->cv);
        cv_rect_union(&ad->dirty, &d);
    }
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
        if (ad->mode == 2)
            ad->active = 1; /* Auto: the host asked the RIP query, so it speaks RIP */
    } else if (v == 1) {
        ad->active = 0;
    } else if (v == 2) {
        ad->active = 1;
    }
    ad->bol = 0;
    ad->nhold = 0;
}

static int rip_on_halt(void *ud)
{
    return ((vt_term *)ud)->feed_stop;
}

static void rip_pass(vt_term *t, struct rip_ad *ad, const vt_u8 *p, long n)
{
    struct rip_sinks sk;
    ad->took = n;
    if (n <= 0)
        return;
    if (!ad->active) {
        vt_feed_core(t, p, n);
        return;
    }
    sk.ud = t;
    sk.text = rip_on_text;
    sk.cmd = rip_on_cmd;
    sk.halt = rip_on_halt;
    ad->took = rip_feed(&ad->st, p, n, &sk);
}

/* Auto: the held line is complete (ad->nline bytes in ad->line, ending in CR or LF). Valid =
 * the RIP parser, run over the line with no sinks, delivers at least one command and counts
 * no error and no unknown command. Valid: RIP turns on and the line goes through the
 * parser. Otherwise the line goes to the terminal as text, unchanged. */
static void rip_auto_done(vt_term *t, struct rip_ad *ad)
{
    long c0 = ad->st.n_cmd, e0 = ad->st.n_err, u0 = ad->st.n_unknown;
    int n = ad->nline, ok;
    rip_init(&ad->st);
    rip_feed(&ad->st, ad->line, n, 0);
    ok = ad->st.n_cmd > 0 && ad->st.n_err == 0 && ad->st.n_unknown == 0;
    rip_init(&ad->st); /* the trial leaves no trace: parser at a line start, counters as before */
    ad->st.n_cmd = c0;
    ad->st.n_err = e0;
    ad->st.n_unknown = u0;
    ad->nline = 0;
    ad->bol = 1;
    if (ok) {
        ad->active = 1;
        rip_pass(t, ad, ad->line, n);
    } else {
        vt_feed_core(t, ad->line, n);
    }
}

/* Auto: byte b while a `!|` line is held. Returns 1 when b was taken into the line, 0 when
 * the hold ended before b (not `!|`, or over RIP_AUTO_LINE): the held bytes went on as
 * text and the caller looks at b again. */
static int rip_auto_byte(vt_term *t, struct rip_ad *ad, vt_u8 b)
{
    int nl = b == '\r' || b == '\n';
    if ((ad->nline == 1 && b != '|') || ad->nline >= RIP_AUTO_LINE) {
        int n = ad->nline;
        ad->nline = 0;
        ad->bol = 0;
        vt_feed_core(t, ad->line, n);
        return 0;
    }
    ad->line[ad->nline++] = b;
    if (ad->nline <= 2) {
        ad->lbs = 0;
        return 1;
    }
    if (ad->lbs == 1) { /* after a backslash, as the parser reads it */
        if (b == '!' || b == '|' || b == '\\') {
            ad->lbs = 0;
        } else if (nl) {
            ad->lbs = 2;
            ad->lsoh = b;
        } else {
            ad->lbs = 0;
        }
        return 1;
    }
    if (ad->lbs == 2) {
        ad->lbs = 0;
        if (nl && b != ad->lsoh)
            return 1; /* the second byte of a CRLF continuation */
    }
    if (b == '\\')
        ad->lbs = 1;
    else if (nl)
        rip_auto_done(t, ad);
    return 1;
}

/* The bytes of vt_feed with RIP set up. An ESC at a command boundary (the parser between
 * commands, or RIP off) is held until it is known not to be ESC [ digits !. */
static void rip_run(vt_term *t, struct rip_ad *ad, const vt_u8 *buf, long len)
{
    long i = 0, from = 0;
    while (i < len) {
        vt_u8 b = buf[i];
        if (ad->nline) {
            if (rip_auto_byte(t, ad, b)) {
                i++;
                from = i;
                if (t->feed_stop) { /* the held line ran a command that took the stream */
                    t->fed = i;
                    return;
                }
                continue;
            }
        } else if (ad->mode == 2 && !ad->active && ad->bol && b == '!' && !ad->nhold) {
            rip_pass(t, ad, buf + from, i - from);
            if (t->feed_stop) {
                t->fed = from + ad->took;
                return;
            }
            ad->line[0] = b;
            ad->nline = 1;
            i++;
            from = i;
            continue;
        }
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
            if (t->feed_stop) { /* the held bytes are not command bytes: a stop here is the host's */
                t->fed = i;
                return;
            }
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
            if (t->feed_stop) {
                t->fed = from + ad->took;
                return;
            }
            ad->bol = 0;
            ad->hold[0] = b;
            ad->nhold = 1;
            i++;
            from = i;
            continue;
        }
        ad->bol = b == '\r' || b == '\n';
        i++;
    }
    rip_pass(t, ad, buf + from, len - from);
    t->fed = t->feed_stop ? from + ad->took : len;
}

void vt_feed(vt_term *t, const vt_u8 *buf, long len)
{
    struct rip_ad *ad = (struct rip_ad *)t->rip;
    t->feed_stop = 0;
    t->fed = len;
    if (!ad) {
        vt_feed_core(t, buf, len); /* RIP off: nothing here runs, so nothing can stop the feed */
        return;
    }
    if (rip_canvas_alive(t, ad))
        t->imgs[ad->slot]->refs++; /* the text may clear the lines the tile is on: the canvas stays */
    rip_run(t, ad, buf, len);
    if (rip_canvas_alive(t, ad))
        t->imgs[ad->slot]->refs--;
    rip_settle(t, ad);
}

void vt_set_rip(vt_term *t, int on)
{
    struct rip_ad *ad = t->rip;
    if (!on) {
        if (ad) {
            rip_canvas_drop(ad);
            rip_clip_free(&ad->clip);
            VT_FREE(ad);
            t->rip = 0;
        }
        return;
    }
    if (ad) { /* a mode change keeps the canvas; On turns RIP on at once */
        ad->mode = on;
        if (on == 1)
            ad->active = 1;
        return;
    }
    ad = (struct rip_ad *)VT_MALLOC(sizeof(*ad));
    if (!ad)
        return;
    memset(ad, 0, sizeof(*ad));
    rip_init(&ad->st);
    rip_gfx_init(&ad->g);
    rip_mouse_init(&ad->mouse);
    ad->mode = on;
    ad->active = on != 2; /* Auto starts inactive */
    ad->bol = 1;
    t->rip = ad;
}

void vt_set_rip_stroke(vt_term *t, int stroke)
{
    struct rip_ad *ad = t->rip;
    if (!ad)
        return;
    ad->stroke = stroke ? 1 : 0;
    ad->g.stroke = ad->stroke;
}

long vt_rip_stroke_glyphs(void)
{
    return rip_font_stroke_glyphs();
}

int vt_rip_active(const vt_term *t)
{
    return t->rip ? ((struct rip_ad *)t->rip)->active : 0;
}

long vt_rip_icons(const vt_term *t)
{
    return t->rip ? rip_icons_drawn() : 0;
}

void vt_feed_stop(vt_term *t)
{
    t->feed_stop = 1;
}

long vt_fed(const vt_term *t)
{
    return t->fed;
}

long vt_rip_blocks(const vt_term *t)
{
    return t->rip ? ((struct rip_ad *)t->rip)->blocks : 0;
}

long vt_rip_commands(const vt_term *t)
{
    return t->rip ? rip_commands(&((struct rip_ad *)t->rip)->st) : 0;
}

static void rip_free(vt_term *t)
{
    if (t->rip) {
        rip_canvas_drop((struct rip_ad *)t->rip);
        rip_clip_free(&((struct rip_ad *)t->rip)->clip);
        VT_FREE(t->rip);
        t->rip = 0;
    }
}

/* A canvas pixel of a point in the text area (pixels from its top left): the cell and the
 * position in it map to the picture's own cells, so a cell size that differs from the
 * picture's (the screen font) still lands on the right pixel. */
static int rip_canvas_xy(const vt_term *t, const struct rip_ad *ad, int px, int py, int *cx, int *cy)
{
    const vt_image *im;
    int cw = t->cell_w, ch = t->cell_h;
    if (px < 0 || py < 0 || cw <= 0 || ch <= 0 || !rip_canvas_alive(t, ad))
        return 0;
    im = t->imgs[ad->slot];
    *cx = (px / cw) * im->cw + (px % cw) * im->cw / cw;
    *cy = (py / ch) * im->ch + (py % ch) * im->ch / ch;
    return *cx < CV_RIP_W && *cy < CV_RIP_H;
}

int vt_rip_click(vt_term *t, int px, int py, vt_u8 *out)
{
    struct rip_ad *ad = t->rip;
    int x, y, n, clr = 0;
    if (!ad || !ad->active || !rip_canvas_xy(t, ad, px, py, &x, &y))
        return 0;
    n = rip_mouse_click(&ad->mouse, x, y, out, VT_RIP_OUT_MAX, &clr);
    if (n < 0)
        return 0;
    if (clr) {
        struct rip_cmd c;
        memset(&c, 0, sizeof(c));
        c.ch = '*';
        rip_on_cmd(t, &c);
        rip_settle(t, ad);
    }
    return n;
}

int vt_rip_key(vt_term *t, int key, vt_u8 *out)
{
    struct rip_ad *ad = t->rip;
    int n, clr = 0;
    if (!ad || !ad->active)
        return 0;
    n = rip_mouse_key(&ad->mouse, key, out, VT_RIP_OUT_MAX, &clr);
    if (n < 0)
        return 0;
    if (clr) {
        struct rip_cmd c;
        memset(&c, 0, sizeof(c));
        c.ch = '*';
        rip_on_cmd(t, &c);
        rip_settle(t, ad);
    }
    return n;
}

int vt_rip_take_dirty(vt_term *t, int *x0, int *y0, int *x1, int *y1)
{
    struct rip_ad *ad = t->rip;
    struct cv_rect_s d;
    memset(&d, 0, sizeof(d));
    if (ad) {
        d = ad->dirty;
        memset(&ad->dirty, 0, sizeof(ad->dirty));
    }
    if (x0) *x0 = d.x0;
    if (y0) *y0 = d.y0;
    if (x1) *x1 = d.x1;
    if (y1) *y1 = d.y1;
    return d.x1 > d.x0 && d.y1 > d.y0;
}

long vt_rip_mouse_fields(const vt_term *t)
{
    return t->rip ? ((struct rip_ad *)t->rip)->mouse.n : 0;
}
