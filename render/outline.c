/* outline: see outline.h. */
#define __NOLIBBASE__ /* BulletBase is the worker's own: one engine per window */
#include <string.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/glyph.h>
#include <diskfont/oterrors.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bullet.h>
#include "outline.h"
#include "otag.h"
#include "ttf.h"
#include "glyphcode.h"

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
    ULONG cp;            /* GLYPH */
    int cells;
    int mark;            /* GLYPH: a combining mark, placed over the cells */
    UBYTE *mask;         /* GLYPH: the caller's cleared buffer, ch rows of bpr */
    WORD bpr;
    UBYTE *buf;          /* READ: the caller's buffer, max bytes */
    LONG max, len;       /* READ: len the answer, the bytes read, -1 no such file */
    int ok;              /* the answer: 1 done / the glyph is there */
    ULONG own;           /* GLYPH's answer: the own engine's glyphs drawn so far */
};

struct vo_slot {
    ULONG cp;
    UBYTE cells, state;
    UBYTE *mask;
};

struct vo_font {
    struct MsgPort *reply;
    struct MsgPort *worker;   /* the worker's pr_MsgPort */
    struct vo_msg m;
    WORD cw, ch, base;
    int used;
    int engine;               /* 0: opened with "", the worker only (vo_read) */
    ULONG own;                /* the worker's count, from its last GLYPH answer */
    char name[64];
    struct vo_slot slot[VO_SLOTS];
};

/* ---- the worker: the engine, in a process of its own --------------------------- */

/* the worker's state, on its stack: one per window, nothing shared */
struct vo_engine {
    struct Library *BulletBase;
    struct GlyphEngine *ge;
    UBYTE *otag;
    char path[96];       /* OT_OTagPath: the engine may keep the pointer */
    WORD cw, ch, base;
    UBYTE notdef[VO_MAXH * (VO_MAXW / 8)]; /* U+0001's glyph: what "missing" looks like */
    WORD notdef_bpr;
    int have_notdef;
    int codes32;         /* the engine selects glyphs by OT_GlyphCode_32 (measured) */
    UBYTE probe[VO_MAXH * (VO_MAXW / 8)]; /* calibrate's scratch glyph */
    /* the own engine (render/ttf.c), when OT_Spec1 names a TrueType file:
     * no bullet library then (plan 2026-10-22-handler-rasterizer.md R3) */
    ttf_font *tt;
    BPTR fh;             /* the .ttf, open while tt is */
    ULONG fpos;          /* fh's position: a read there needs no Seek */
    long xscale, yscale; /* pixels per font unit, 16.16 */
    UBYTE *cov;          /* VO_MAXW x VO_MAXH coverage, fast RAM */
    ULONG own;           /* glyphs ttf_render drew: /state outline-own */
};

static ULONG set1(struct vo_engine *e, ULONG tag, ULONG data)
{
    struct Library *BulletBase = e->BulletBase;
    struct TagItem t[2];
    t[0].ti_Tag = tag;
    t[0].ti_Data = data;
    t[1].ti_Tag = TAG_DONE;
    return SetInfoA(e->ge, t);
}

/* render/glyphcode's way into the engine */
static int gc_set(void *engine, int wide, unsigned long code)
{
    return set1((struct vo_engine *)engine, wide ? OT_GlyphCode_32 : OT_GlyphCode, code) != OTERR_Success;
}

static int draw_current(struct vo_engine *e, UBYTE *mask, WORD bpr, WORD w, WORD ox);

static unsigned long gc_draw(void *engine)
{
    struct vo_engine *e = (struct vo_engine *)engine;
    unsigned long sig = 0;
    LONG i, n = (LONG)e->notdef_bpr * e->ch;
    memset(e->probe, 0, sizeof(e->probe));
    if (!draw_current(e, e->probe, e->notdef_bpr, e->cw, 0))
        return 0;
    for (i = 0; i < n; i++)
        sig = sig * 31UL + e->probe[i];
    return sig | 1UL;
}

/* cp's glyph into mask (cleared, ch rows of bpr bytes, w pixels used): the
 * glyph's current point ox pixels from the left edge (0, or the cell's
 * width for a mark that draws left of its point) and on the cell's
 * baseline. 0 when the engine has none or it is blank. Beyond the BMP only
 * on an engine measured to take 32-bit codes (render/glyphcode.h:
 * ttf.library 0.8.5 ignores OT_GlyphCode_32 and answers success). */
static int render(struct vo_engine *e, ULONG cp, UBYTE *mask, WORD bpr, WORD w, WORD ox)
{
    if (e->tt) {
        /* the own engine: every code point, .notdef (glyph 0) is missing */
        unsigned gid = ttf_glyph_index(e->tt, cp);
        if (!gid)
            return 0;
        memset(e->cov, 0, (size_t)w * e->ch);
        if (ttf_render(e->tt, gid, e->xscale, e->yscale, ox, e->base, e->cov, w, e->ch) != 1)
            return 0;
        ttf_threshold(e->cov, w, e->ch, mask, bpr);
        e->own++;
        return 1;
    }
    if (vt_gc_select(e, gc_set, e->codes32, cp) != 0)
        return 0;
    return draw_current(e, mask, bpr, w, ox);
}

/* the engine's current glyph into mask, as render */
static int draw_current(struct vo_engine *e, UBYTE *mask, WORD bpr, WORD w, WORD ox)
{
    struct Library *BulletBase = e->BulletBase;
    struct GlyphMap *gm = 0;
    struct TagItem t[2];
    WORD x, y, any = 0;
    t[0].ti_Tag = OT_GlyphMap;
    t[0].ti_Data = (ULONG)&gm;
    t[1].ti_Tag = TAG_DONE;
    if (ObtainInfoA(e->ge, t) != OTERR_Success || !gm)
        return 0;
    for (y = 0; y < (WORD)gm->glm_BlackHeight; y++) {
        WORD sy = (WORD)(gm->glm_BlackTop + y), dy = (WORD)(sy - gm->glm_Y0 + e->base);
        const UBYTE *src = gm->glm_BitMap + (LONG)sy * gm->glm_BMModulo;
        if (dy < 0 || dy >= e->ch)
            continue;
        for (x = 0; x < (WORD)gm->glm_BlackWidth; x++) {
            WORD sx = (WORD)(gm->glm_BlackLeft + x), dx = (WORD)(sx - gm->glm_X0 + ox);
            if (dx < 0 || dx >= w || !(src[sx >> 3] & (0x80 >> (sx & 7))))
                continue;
            mask[(LONG)dy * bpr + (dx >> 3)] |= (UBYTE)(0x80 >> (dx & 7));
            any = 1;
        }
    }
    t[0].ti_Data = (ULONG)gm;
    ReleaseInfoA(e->ge, t);
    return any;
}

/* the size: point height = the cell's height at 72 dpi down; across, the
 * dpi that makes the font's advance one cell (a monospaced outline font's
 * glyphs all advance alike) */
static const ULONG probe[] = { 'M', 0xE0A0, 0x2588, '0' };

/* the own engine's size, as calibrate's: ch pixels an em down, and across
 * the scale that makes the first probe's advance one cell */
static void calibrate_own(struct vo_engine *e)
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
    e->notdef_bpr = (WORD)(((e->cw + 15) >> 4) << 1);
    e->have_notdef = 0; /* glyph 0 answers missing before any drawing */
}

static void calibrate(struct vo_engine *e)
{
    struct Library *BulletBase = e->BulletBase;
    struct GlyphMap *gm = 0;
    struct TagItem t[3];
    ULONG xdpi = 72;
    int i;
    if (e->tt) {
        calibrate_own(e);
        return;
    }
    t[0].ti_Tag = OT_DeviceDPI;
    t[0].ti_Data = (72UL << 16) | 72UL;
    t[1].ti_Tag = OT_PointHeight;
    t[1].ti_Data = (ULONG)e->ch << 16;
    t[2].ti_Tag = TAG_DONE;
    SetInfoA(e->ge, t);
    for (i = 0; i < (int)(sizeof(probe) / sizeof(probe[0])); i++) {
        struct TagItem q[2];
        if (set1(e, OT_GlyphCode, probe[i]) != OTERR_Success)
            continue;
        q[0].ti_Tag = OT_GlyphMap;
        q[0].ti_Data = (ULONG)&gm;
        q[1].ti_Tag = TAG_DONE;
        if (ObtainInfoA(e->ge, q) != OTERR_Success || !gm)
            continue;
        if (gm->glm_Width > 0x1000) /* more than 1/16 em */
            xdpi = (72UL * (ULONG)e->cw * 65536UL) / ((ULONG)e->ch * (ULONG)gm->glm_Width);
        q[0].ti_Data = (ULONG)gm;
        ReleaseInfoA(e->ge, q);
        break;
    }
    if (xdpi < 10 || xdpi > 2000)
        xdpi = 72;
    t[0].ti_Data = (xdpi << 16) | 72UL;
    SetInfoA(e->ge, t);
    e->notdef_bpr = (WORD)(((e->cw + 15) >> 4) << 1);
    memset(e->notdef, 0, sizeof(e->notdef));
    e->have_notdef = render(e, 0x0001, e->notdef, e->notdef_bpr, e->cw, 0);
    e->codes32 = vt_gc_probe32(e, gc_set, gc_draw);
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

/* the file OT_Spec1 names (a path without a volume is in FONTS:), when it is
 * TrueType: the own engine on it. 0 leaves e as it was, for the library. */
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

/* the .otag read, checked and made a tag list; the own engine on the
 * TrueType file it names, else the engine its OT_Engine names */
static int engine_open(struct vo_engine *e, const char *name)
{
    char *path = e->path, lib[48];
    struct FileInfoBlock *fib;
    BPTR f;
    LONG len, got;
    otag_info in;
    ULONG *tags;
    struct TagItem t[3];
    int i, n;
    strcpy(path, "FONTS:");
    n = (int)strlen(path);
    for (i = 0; name[i] && n < (int)sizeof(e->path) - 6; i++)
        path[n++] = name[i];
    path[n] = 0;
    if (n > 5 && (!strcmp(path + n - 5, ".font") || !strcmp(path + n - 5, ".otag")))
        path[n - 5] = 0;
    strcat(path, ".otag");
    if (!(f = Open((STRPTR)path, MODE_OLDFILE)))
        return 0;
    len = 0;
    if ((fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0)) != 0) {
        if (ExamineFH(f, fib))
            len = fib->fib_Size;
        FreeDosObject(DOS_FIB, fib);
    }
    if (len <= 0 || len > 65536 || !(e->otag = (UBYTE *)AllocVec((ULONG)len, MEMF_ANY))) {
        Close(f);
        return 0;
    }
    got = Read(f, e->otag, len);
    Close(f);
    if (got != len || otag_check(e->otag, len, &in) != OTAG_OK)
        return 0;
    if (in.spec1_off >= 0 && own_open(e, (const char *)e->otag + in.spec1_off)) {
        calibrate(e);
        return 1;
    }
    /* indirect values are offsets from the file's start: pointers now */
    tags = (ULONG *)e->otag;
    for (i = 0; i < in.ntags; i++)
        if ((tags[i * 2] & TAG_USER) && (tags[i * 2] & OT_Indirect))
            tags[i * 2 + 1] += (ULONG)e->otag;
    n = 0;
    for (i = 0; ((const char *)e->otag)[in.engine_off + i] && n < (int)sizeof(lib) - 9; i++)
        lib[n++] = ((const char *)e->otag)[in.engine_off + i];
    lib[n] = 0;
    strcat(lib, ".library");
    if (!(e->BulletBase = OpenLibrary((STRPTR)lib, 0)))
        return 0;
    {
        struct Library *BulletBase = e->BulletBase;
        if (!(e->ge = OpenEngine()))
            return 0;
    }
    t[0].ti_Tag = OT_OTagPath;
    t[0].ti_Data = (ULONG)path;
    t[1].ti_Tag = OT_OTagList;
    t[1].ti_Data = (ULONG)e->otag;
    t[2].ti_Tag = TAG_DONE;
    {
        struct Library *BulletBase = e->BulletBase;
        if (SetInfoA(e->ge, t) != OTERR_Success)
            return 0;
    }
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
    if (e->ge) {
        struct Library *BulletBase = e->BulletBase;
        CloseEngine(e->ge);
    }
    if (e->BulletBase)
        CloseLibrary(e->BulletBase);
    if (e->otag)
        FreeVec(e->otag);
    e->ge = 0;
    e->BulletBase = 0;
    e->otag = 0;
}

static int same_as_notdef(struct vo_engine *e, const UBYTE *mask, WORD bpr)
{
    WORD y, x;
    if (!e->have_notdef || bpr != e->notdef_bpr)
        return 0;
    for (y = 0; y < e->ch; y++)
        for (x = 0; x < bpr; x++)
            if (mask[(LONG)y * bpr + x] != e->notdef[(LONG)y * bpr + x])
                return 0;
    return 1;
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
            if (e.ge || e.tt)
                calibrate(&e);
            m->ok = 1;
            break;
        case VO_GLYPH: {
            WORD w = (WORD)(m->cells * e.cw);
            if (!e.ge && !e.tt) {
                m->ok = 0;
                break;
            }
            /* a mark: drawn left of its point over the cell, as fonts make
             * them; one that draws nothing there is a spacing mark */
            m->ok = m->mark && render(&e, m->cp, m->mask, m->bpr, w, w);
            if (!m->ok)
                m->ok = render(&e, m->cp, m->mask, m->bpr, w, 0) &&
                        !(m->cells == 1 && same_as_notdef(&e, m->mask, m->bpr));
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
        f->slot[i].mask = 0;
        f->slot[i].state = SLOT_EMPTY;
    }
    f->used = 0;
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
    p = CreateNewProcTags(NP_Entry, (ULONG)worker, NP_Name, (ULONG)"UP-Term glyphs", NP_StackSize, 12288, /* the engine's, and calibrate's scratch glyph */
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
        if (s->cp == cp && s->cells == (cells | mark))
            return s->state == SLOT_HAVE ? s->mask : 0;
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
    f->m.op = VO_GLYPH;
    f->m.cp = cp;
    f->m.cells = cells;
    f->m.mark = mark != 0;
    f->m.mask = s->mask;
    f->m.bpr = b;
    ask(f);
    f->own = f->m.own;
    if (!f->m.ok) {
        FreeVec(s->mask);
        s->mask = 0;
        return 0;
    }
    s->state = SLOT_HAVE;
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
