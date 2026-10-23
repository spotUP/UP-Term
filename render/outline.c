/* outline: see outline.h. */
#define __NOLIBBASE__ /* SysBase and DOSBase: the handler's, declared below */
#include <string.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "outline.h"
#include "otag.h"
#include "ttf.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
#define UPASSIGN_DOS
#include "../config/upassign.h"

#define VO_SLOTS 1024      /* a power of two */
#define VO_FILL 768        /* a full table is emptied and filled again */
#define VO_MAXW 64         /* widest mask in pixels: two cells of a 32-pixel font */
#define VO_MAXH 64

enum { VO_OPEN = 1, VO_CELL, VO_GLYPH, VO_CLOSE, VO_READ };
enum { SLOT_EMPTY = 0, SLOT_HAVE, SLOT_MISSING };

struct vo_msg {
    struct Message msg;
    int op;
    char name[64];       /* OPEN: the font ("": no engine, the worker only); READ: the file */
    WORD cw, ch, base;   /* OPEN, CELL */
    WORD rx, ry;         /* OPEN, CELL: the screen's pixel in ticks (vo_set_aspect) */
    ULONG cp;            /* GLYPH */
    int cells;
    int mark;            /* GLYPH: a combining mark, placed over the cells */
    UBYTE *mask;         /* GLYPH: the caller's cleared buffer, ch rows of bpr */
    WORD bpr;
    UBYTE *cov;          /* GLYPH: the caller's buffer for the coverage, cells * cw x ch
                          * bytes (0: none wanted); its answer: cov_ok */
    int cov_ok;          /* GLYPH's answer: cov holds the glyph's coverage (the own engine) */
    UBYTE *buf;          /* READ: the caller's buffer, max bytes */
    LONG max, len;       /* READ: len the answer, the bytes read, -1 no such file */
    int ok;              /* the answer: 1 done / the glyph is there */
    ULONG own;           /* GLYPH's answer: the own engine's glyphs drawn so far */
};

struct vo_slot {
    ULONG cp;
    UBYTE cells, state;
    UBYTE *mask;
    UBYTE *cov;   /* the glyph's coverage, cells * cw x ch bytes in fast RAM; 0: none kept */
};

struct vo_font {
    struct MsgPort *reply;
    struct MsgPort *worker;   /* the worker's pr_MsgPort */
    struct vo_msg m;
    WORD cw, ch, base;
    int used;
    int engine;               /* 0: opened with "", the worker only (vo_read) */
    ULONG own;                /* the worker's count, from its last GLYPH answer */
    const UBYTE *last_mask;   /* the mask glyph() returned last, and its coverage (vo_coverage) */
    const UBYTE *last_cov;
    char name[64];
    struct vo_slot slot[VO_SLOTS];
};

/* ---- the worker: the engine, in a process of its own --------------------------- */

/* the worker's state, on its stack: one per window, nothing shared */
struct vo_engine {
    WORD cw, ch, base;
    /* UP-Term's own engine (render/ttf.c) on the font's TrueType file
     * (plan 2026-10-22-handler-rasterizer.md) */
    ttf_font *tt;
    BPTR fh;             /* the .ttf, open while tt is */
    ULONG fpos;          /* fh's position: a read there needs no Seek */
    long xscale, yscale; /* pixels per font unit, 16.16: the cell's scale */
    WORD rx, ry;         /* the screen's pixel in ticks */
    UBYTE *cov;          /* VO_MAXW x VO_MAXH coverage, fast RAM */
    ULONG own;           /* glyphs ttf_render drew: /state outline-own */
};

/* cp's glyph into mask (cleared, ch rows of bpr bytes, w pixels used) and
 * its coverage into e->cov. how: TTF_SHAPE or TTF_FILL, placed by
 * ttf_place; or VO_AT_CELL, at the cell's scale with its current point ox
 * pixels from the left edge (0, or the cell's width for a mark that draws
 * left of its point) and on the cell's baseline. Every code point; 0 when
 * the font has none (.notdef, glyph 0, is missing) or it is blank. */
#define VO_AT_CELL (-1)

static int render(struct vo_engine *e, ULONG cp, UBYTE *mask, WORD bpr, WORD w, WORD ox, int how)
{
    unsigned gid = ttf_glyph_index(e->tt, cp);
    long xs = e->xscale, ys = e->yscale;
    int baseline = e->base;
    if (!gid)
        return 0;
    if (how != VO_AT_CELL) {
        ttf_place_out pl;
        ttf_place(e->tt, gid, how, w, e->ch, e->base, e->rx, e->ry, &pl);
        xs = pl.xscale;
        ys = pl.yscale;
        baseline = pl.baseline;
        ox = (WORD)pl.ox;
    }
    memset(e->cov, 0, (size_t)w * e->ch);
    if (ttf_render(e->tt, gid, xs, ys, ox, baseline, e->cov, w, e->ch) != 1)
        return 0;
    ttf_threshold(e->cov, w, e->ch, mask, bpr);
    e->own++;
    return 1;
}

/* the scale combining marks are drawn at (VO_AT_CELL): ch pixels an em
 * down; across, the scale that makes the first probe's advance one cell (a
 * monospaced outline font's glyphs all advance alike). Other glyphs are
 * placed one by one by ttf_place. */
static const ULONG probe[] = { 'M', 0xE0A0, 0x2588, '0' };

static void calibrate(struct vo_engine *e)
{
    long upem = ttf_units_per_em(e->tt), adv = 0;
    int i;
    e->yscale = e->xscale = ((long)e->ch << 16) / upem;
    for (i = 0; i < (int)(sizeof(probe) / sizeof(probe[0])) && adv * 16 <= upem; i++) {
        unsigned gid = ttf_glyph_index(e->tt, probe[i]);
        adv = gid ? ttf_advance(e->tt, gid) : 0;
    }
    if (adv * 16 > upem) /* more than 1/16 em */
        e->xscale = ((long)e->cw << 16) / adv;
}

/* glyphs made to fill the cell (TTF_FILL), whatever its shape on the
 * screen: box drawing, block elements, legacy computing and
 * the powerline separators (frames and prompts without gaps) */
static int fills_cell(ULONG cp)
{
    return (cp >= 0x2500 && cp <= 0x259F) || (cp >= 0x1FB00 && cp <= 0x1FBFF) ||
           (cp >= 0xE0B0 && cp <= 0xE0D7);
}

/* render/ttf's reads and memory: dos.library, so the worker's only */
static long tt_read(void *ctx, unsigned long off, void *buf, long len)
{
    struct vo_engine *e = (struct vo_engine *)ctx;
    LONG got;
    if (off != e->fpos && Seek(e->fh, (LONG)off, OFFSET_BEGINNING) < 0) {
        e->fpos = (ULONG)-1;
        return -1;
    }
    got = Read(e->fh, buf, len);
    e->fpos = got > 0 ? off + (ULONG)got : (ULONG)-1;
    return got;
}

static void *tt_alloc(void *ctx, unsigned long size)
{
    (void)ctx;
    return AllocVec((ULONG)size, MEMF_ANY | MEMF_CLEAR);
}

static void tt_free(void *ctx, void *p)
{
    (void)ctx;
    FreeVec(p);
}

/* the TrueType file spec (a path without a volume is in FONTS:): the own
 * engine on it. 0 leaves e as it was. */
static int own_open(struct vo_engine *e, const char *spec)
{
    char p[128];
    UBYTE head[4];
    ttf_io io;
    int i, n = 0;
    if (!strchr(spec, ':'))
        for (i = 0; "FONTS:"[i]; i++)
            p[n++] = "FONTS:"[i];
    for (i = 0; spec[i] && n < (int)sizeof(p) - 1; i++)
        p[n++] = spec[i];
    p[n] = 0;
    if (!(e->fh = Open((STRPTR)p, MODE_OLDFILE)))
        return 0;
    e->fpos = 0;
    /* TrueType: 0x00010000 or 'true' (ttf_open also refuses the rest) */
    if (tt_read(e, 0, head, 4) == 4 &&
        ((head[0] == 0 && head[1] == 1 && head[2] == 0 && head[3] == 0) || !memcmp(head, "true", 4)) &&
        (e->cov = (UBYTE *)AllocVec(VO_MAXW * VO_MAXH, MEMF_ANY)) != 0) {
        io.read = tt_read;
        io.alloc = tt_alloc;
        io.free = tt_free;
        io.ctx = e;
        if (ttf_open(&io, &e->tt) == TTF_OK)
            return 1;
    }
    if (e->cov)
        FreeVec(e->cov);
    Close(e->fh);
    e->cov = 0;
    e->fh = 0;
    e->tt = 0;
    return 0;
}

/* the font `name`: FONTS:<name>.otag (ttfinstall's, for ttf.library) when
 * its OT_Spec1 names a TrueType file, else FONTS:<name>.ttf itself. The
 * .otag's engine is not opened: render/ttf.c draws every TrueType font. */
static int engine_open(struct vo_engine *e, const char *name)
{
    char path[96];
    UBYTE *otag;
    struct FileInfoBlock *fib;
    BPTR f;
    LONG len = 0, got;
    otag_info in;
    int i, n, ok = 0;
    strcpy(path, "FONTS:");
    n = (int)strlen(path);
    for (i = 0; name[i] && n < (int)sizeof(path) - 6; i++)
        path[n++] = name[i];
    path[n] = 0;
    if (n > 5 && (!strcmp(path + n - 5, ".font") || !strcmp(path + n - 5, ".otag")))
        path[n - 5] = 0;
    n = (int)strlen(path);
    strcat(path, ".otag");
    if ((f = Open((STRPTR)path, MODE_OLDFILE)) != 0) {
        if ((fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0)) != 0) {
            if (ExamineFH(f, fib))
                len = fib->fib_Size;
            FreeDosObject(DOS_FIB, fib);
        }
        if (len > 0 && len <= 65536 && (otag = (UBYTE *)AllocVec((ULONG)len, MEMF_ANY)) != 0) {
            got = Read(f, otag, len);
            ok = got == len && otag_check(otag, len, &in) == OTAG_OK && in.spec1_off >= 0 &&
                 own_open(e, (const char *)otag + in.spec1_off);
            FreeVec(otag);
        }
        Close(f);
    }
    path[n] = 0;
    strcat(path, ".ttf");
    if (!ok && !own_open(e, path))
        return 0;
    calibrate(e);
    return 1;
}

static void engine_close(struct vo_engine *e)
{
    if (e->tt)
        ttf_close(e->tt);
    if (e->fh)
        Close(e->fh);
    if (e->cov)
        FreeVec(e->cov);
    e->tt = 0;
    e->fh = 0;
    e->cov = 0;
}

static void worker(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct vo_engine e;
    struct vo_msg *m;
    memset(&e, 0, sizeof(e));
    me->pr_WindowPtr = (APTR)-1; /* a missing font file is an answer, not a requester */
    for (;;) {
        WaitPort(&me->pr_MsgPort);
        m = (struct vo_msg *)GetMsg(&me->pr_MsgPort);
        if (!m)
            continue;
        switch (m->op) {
        case VO_OPEN:
            e.cw = m->cw;
            e.ch = m->ch;
            e.base = m->base;
            e.rx = m->rx;
            e.ry = m->ry;
            m->ok = !m->name[0] || engine_open(&e, m->name); /* "": the worker alone */
            if (!m->ok) {
                engine_close(&e);
                Forbid(); /* the opener frees what m is in: end before it runs on */
                ReplyMsg(&m->msg);
                return;
            }
            break;
        case VO_CELL:
            e.cw = m->cw;
            e.ch = m->ch;
            e.base = m->base;
            e.rx = m->rx;
            e.ry = m->ry;
            if (e.tt)
                calibrate(&e);
            m->ok = 1;
            break;
        case VO_GLYPH: {
            WORD w = (WORD)(m->cells * e.cw);
            if (!e.tt) {
                m->ok = 0;
                break;
            }
            /* a mark: drawn left of its point over the cell, as fonts make
             * them; one that draws nothing there is a spacing mark */
            m->ok = m->mark && render(&e, m->cp, m->mask, m->bpr, w, w, VO_AT_CELL);
            if (!m->ok)
                m->ok = render(&e, m->cp, m->mask, m->bpr, w, 0,
                               m->mark ? VO_AT_CELL : fills_cell(m->cp) ? TTF_FILL : TTF_SHAPE);
            m->cov_ok = m->ok && m->cov; /* render left the glyph's coverage in e.cov */
            if (m->cov_ok)
                memcpy(m->cov, e.cov, (size_t)w * e.ch);
            m->own = e.own;
            break;
        }
        case VO_READ: {
            /* a file the caller may not read itself (a DOS call; U2's
             * Unifont pages): a missing one is an answer, not a requester */
            BPTR fh;
            /* the kit's own pages: through the assign UP-Term:, which a window
             * opened before S:User-Startup ran does not have yet (upassign.h) */
            if (!strncmp(m->name, UPASSIGN_NAME ":", sizeof(UPASSIGN_NAME)))
                upassign_ensure();
            fh = Open((STRPTR)m->name, MODE_OLDFILE);
            m->len = -1;
            if (fh) {
                m->len = Read(fh, m->buf, m->max);
                Close(fh);
                if (m->len < 0)
                    m->len = -1;
            }
            m->ok = 1;
            break;
        }
        case VO_CLOSE:
            engine_close(&e);
            Forbid();
            ReplyMsg(&m->msg);
            return;
        }
        ReplyMsg(&m->msg);
    }
}

/* ---- the caller's side ------------------------------------------------------------ */

static void ask(vo_font *f)
{
    f->m.msg.mn_ReplyPort = f->reply;
    f->m.msg.mn_Length = sizeof(f->m);
    PutMsg(f->worker, &f->m.msg);
    WaitPort(f->reply);
    GetMsg(f->reply);
}

static void clear_cache(vo_font *f)
{
    int i;
    for (i = 0; i < VO_SLOTS; i++) {
        if (f->slot[i].mask)
            FreeVec(f->slot[i].mask);
        if (f->slot[i].cov)
            FreeVec(f->slot[i].cov);
        f->slot[i].mask = 0;
        f->slot[i].cov = 0;
        f->slot[i].state = SLOT_EMPTY;
    }
    f->used = 0;
    f->last_mask = f->last_cov = 0;
}

vo_font *vo_open(const char *name, WORD cw, WORD ch, WORD base)
{
    vo_font *f;
    struct Process *p;
    if (!name || cw < 1 || ch < 1 || 2 * cw > VO_MAXW || ch > VO_MAXH)
        return 0;
    f = (vo_font *)AllocVec(sizeof(*f), MEMF_PUBLIC | MEMF_CLEAR);
    if (!f)
        return 0;
    if (!(f->reply = CreateMsgPort())) {
        FreeVec(f);
        return 0;
    }
    p = CreateNewProcTags(NP_Entry, (ULONG)worker, NP_Name, (ULONG)"UP-Term glyphs", NP_StackSize, 12288, /* render/ttf.c's rasterizer */
                          TAG_DONE);
    if (!p) {
        DeleteMsgPort(f->reply);
        FreeVec(f);
        return 0;
    }
    f->worker = &p->pr_MsgPort;
    strncpy(f->name, name, sizeof(f->name) - 1);
    strncpy(f->m.name, name, sizeof(f->m.name) - 1);
    f->m.op = VO_OPEN;
    f->cw = f->m.cw = cw;
    f->ch = f->m.ch = ch;
    f->base = f->m.base = base;
    f->m.rx = f->m.ry = 1; /* square until vo_set_aspect */
    ask(f);
    if (!f->m.ok) {
        /* the worker has ended */
        DeleteMsgPort(f->reply);
        FreeVec(f);
        return 0;
    }
    f->engine = name[0] != 0;
    return f;
}

void vo_close(vo_font *f)
{
    if (!f)
        return;
    f->m.op = VO_CLOSE;
    ask(f);
    clear_cache(f);
    DeleteMsgPort(f->reply);
    FreeVec(f);
}

void vo_set_cell(vo_font *f, WORD cw, WORD ch, WORD base)
{
    if (!f || (cw == f->cw && ch == f->ch && base == f->base))
        return;
    clear_cache(f);
    if (2 * cw > VO_MAXW || ch > VO_MAXH)
        ch = 0; /* too big for a mask: every glyph missing */
    f->cw = f->m.cw = cw;
    f->ch = f->m.ch = ch;
    f->base = f->m.base = base;
    if (ch && f->engine) {
        f->m.op = VO_CELL;
        ask(f);
    }
}

void vo_set_aspect(vo_font *f, WORD rx, WORD ry)
{
    if (!f || rx < 1 || ry < 1 || (rx == f->m.rx && ry == f->m.ry))
        return;
    clear_cache(f);
    f->m.rx = rx;
    f->m.ry = ry;
    if (f->ch && f->engine) {
        f->m.op = VO_CELL;
        ask(f);
    }
}

/* cells: 1 or 2, plus 4 for a mark (a slot of its own) */
static const UBYTE *glyph(vo_font *f, ULONG cp, int cells, WORD *bpr)
{
    ULONG h;
    struct vo_slot *s;
    WORD b;
    int mark = cells & 4;
    if (!f || f->ch < 1 || !f->engine)
        return 0;
    cells = (cells & 3) == 2 ? 2 : 1;
    b = (WORD)(((cells * f->cw + 15) >> 4) << 1);
    *bpr = b;
    h = ((cp * 2654435761UL) >> 16) & (VO_SLOTS - 1);
    for (;;) {
        s = &f->slot[h];
        if (s->state == SLOT_EMPTY)
            break;
        if (s->cp == cp && s->cells == (cells | mark)) {
            if (s->state != SLOT_HAVE)
                return 0;
            f->last_mask = s->mask;
            f->last_cov = s->cov;
            return s->mask;
        }
        h = (h + 1) & (VO_SLOTS - 1);
    }
    if (f->used >= VO_FILL) {
        clear_cache(f);
        return glyph(f, cp, cells | mark, bpr);
    }
    s->cp = cp;
    s->cells = (UBYTE)(cells | mark);
    s->state = SLOT_MISSING;
    f->used++;
    s->mask = (UBYTE *)AllocVec((ULONG)b * f->ch, MEMF_CHIP | MEMF_CLEAR);
    if (!s->mask)
        return 0;
    s->cov = (UBYTE *)AllocVec((ULONG)cells * f->cw * f->ch, MEMF_ANY); /* none: drawn 1-bit */
    f->m.op = VO_GLYPH;
    f->m.cp = cp;
    f->m.cells = cells;
    f->m.mark = mark != 0;
    f->m.mask = s->mask;
    f->m.bpr = b;
    f->m.cov = s->cov;
    ask(f);
    f->own = f->m.own;
    if (s->cov && (!f->m.ok || !f->m.cov_ok)) {
        FreeVec(s->cov);
        s->cov = 0;
    }
    if (!f->m.ok) {
        FreeVec(s->mask);
        s->mask = 0;
        return 0;
    }
    s->state = SLOT_HAVE;
    f->last_mask = s->mask;
    f->last_cov = s->cov;
    return s->mask;
}

const UBYTE *vo_glyph(vo_font *f, ULONG cp, int cells, WORD *bpr)
{
    return glyph(f, cp, cells == 2 ? 2 : 1, bpr);
}

const UBYTE *vo_mark(vo_font *f, ULONG cp, int cells, WORD *bpr)
{
    return glyph(f, cp, (cells == 2 ? 2 : 1) | 4, bpr);
}

const char *vo_name(const vo_font *f)
{
    return f ? f->name : "";
}

int vo_has_engine(const vo_font *f)
{
    return f && f->engine;
}

const UBYTE *vo_coverage(const vo_font *f, const UBYTE *mask)
{
    return f && mask && mask == f->last_mask ? f->last_cov : 0;
}

ULONG vo_own(const vo_font *f)
{
    return f ? f->own : 0;
}

LONG vo_read(vo_font *f, const char *path, UBYTE *buf, LONG max)
{
    if (!f || !path || strlen(path) >= sizeof(f->m.name) || max <= 0)
        return -1;
    strcpy(f->m.name, path);
    f->m.op = VO_READ;
    f->m.buf = buf;
    f->m.max = max;
    ask(f);
    return f->m.len;
}
