/* WBRESET-EXT pure core, see wbreset_core.h. */
#include <string.h>
#include "wbreset_core.h"

void wbr_init(wbr_rec *r)
{
    memset(r, 0, sizeof *r);
    r->phase = WBR_OPEN;
}

static int find(const wbr_rec *r, void *task)
{
    int i;
    for (i = 0; i < WBR_MAX; i++)
        if (r->c[i].task == task)
            return i;
    return -1;
}

int wbr_set(wbr_rec *r, void *task, uint32_t sig, int mode, int has_win)
{
    int i;
    if (!task)
        return -1;
    i = find(r, task);
    if (i < 0) {
        i = find(r, 0);
        if (i < 0)
            return -1;
        memset(&r->c[i], 0, sizeof r->c[i]);
        r->c[i].task = task;
    }
    r->c[i].sig = sig;
    r->c[i].mode = (uint8_t)mode;
    r->c[i].has_win = has_win ? 1 : 0;
    return i;
}

void wbr_drop(wbr_rec *r, void *task)
{
    int i;
    if (task && (i = find(r, task)) >= 0)
        memset(&r->c[i], 0, sizeof r->c[i]);
}

int wbr_close_begin(wbr_rec *r, void *self, wbr_sig *out)
{
    int i, n = 0;
    r->phase = WBR_CLOSING;
    r->closes++;
    for (i = 0; i < WBR_MAX; i++) {
        wbr_client *c = &r->c[i];
        if (c->task && c->task != self && c->has_win) {
            out[n].task = c->task;
            out[n].sig = c->sig;
            n++;
        }
    }
    return n;
}

int wbr_pending(const wbr_rec *r, void *self)
{
    int i, n = 0;
    for (i = 0; i < WBR_MAX; i++)
        if (r->c[i].task && r->c[i].task != self && r->c[i].has_win)
            n++;
    return n;
}

static int hidden_list(wbr_rec *r, wbr_sig *out)
{
    int i, n = 0;
    for (i = 0; i < WBR_MAX; i++)
        if (r->c[i].task && r->c[i].mode == WBR_HIDE && r->c[i].hidden) {
            out[n].task = r->c[i].task;
            out[n].sig = r->c[i].sig;
            n++;
        }
    return n;
}

int wbr_close_end(wbr_rec *r, int ok, int timed_out, wbr_sig *out)
{
    if (timed_out)
        r->timeouts++;
    if (ok) {
        r->phase = WBR_CLOSED;
        return 0;
    }
    r->failed++;
    r->phase = WBR_OPEN;                /* another program's window kept the Workbench: ours come back now */
    return hidden_list(r, out);
}

int wbr_open_end(wbr_rec *r, int ok, wbr_sig *out)
{
    if (!ok)
        return 0;
    r->opens++;
    r->phase = WBR_OPEN;
    return hidden_list(r, out);
}

int wbr_client_step(wbr_rec *r, void *task)
{
    int i = find(r, task);
    wbr_client *c;
    if (!task || i < 0)
        return WBR_DO_NONE;
    c = &r->c[i];
    if (r->phase != WBR_OPEN) {
        if (!c->has_win)
            return WBR_DO_NONE;
        if (c->mode == WBR_HIDE)
            c->hidden = 1;
        return WBR_DO_HIDE;
    }
    if (c->hidden) {
        c->hidden = 0;
        /* The client holds the screen from its first LockPubScreen on, long before it reports the window (the bar's
         * CPU calibration alone is half a second). IPrefs retries a failed CloseWorkBench at once (3.1 with Directory
         * Opus: the first try fails while DOpus closes its windows): a retry that saw has_win 0 here neither signalled
         * nor waited, ran the original into the half-open bar's lock and failed every time. Counted as holding now,
         * the retry signals it and waits; its own report sets the real value when the open is done. */
        c->has_win = 1;
        return WBR_DO_SHOW;
    }
    return WBR_DO_NONE;
}

static void put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void wbr_gate_build(unsigned char *out, uint32_t hook, uint32_t saved)
{
    put16(out + 0, 0x2F3A);             /* move.l hook(pc),-(sp): extension word at 2, hook at 14 */
    put16(out + 2, WBR_GATE_HOOK - 2);
    put16(out + 4, 0x6606);             /* bne.s go (12 - 6) */
    put16(out + 6, 0x588F);             /* addq.l #4,sp */
    put16(out + 8, 0x2F3A);             /* move.l saved(pc),-(sp): extension word at 10, saved at 18 */
    put16(out + 10, WBR_GATE_SAVED - 10);
    put16(out + 12, 0x4E75);            /* rts */
    put32(out + WBR_GATE_HOOK, hook);
    put32(out + WBR_GATE_SAVED, saved);
}

unsigned wbr_bound_mask(const gt_model *a, const gt_model *b)
{
    unsigned m = 0;
    if (!a || !b)
        return 0;
    if (strcmp(a->look.font[GT_FONT_SCREEN], b->look.font[GT_FONT_SCREEN]))
        m |= WBR_B_SCREEN_FONT;
    if (strcmp(a->look.font[GT_FONT_TITLE], b->look.font[GT_FONT_TITLE]))
        m |= WBR_B_TITLE_FONT;
    if (a->look.bar_height != b->look.bar_height)
        m |= WBR_B_BAR_HEIGHT;
    if (a->look.gadget_size != b->look.gadget_size)
        m |= WBR_B_GADGET_SIZE;
    if (a->look.gadget_pad != b->look.gadget_pad)
        m |= WBR_B_GADGET_PAD;
    if (a->look.gadget_order != b->look.gadget_order)
        m |= WBR_B_GADGET_ORDER;
    if (a->look.scroll_width != b->look.scroll_width)
        m |= WBR_B_SCROLL_WIDTH;
    return m;
}
