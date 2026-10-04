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

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

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

/* cp's glyph into mask (cleared, ch rows of bpr bytes, w pixels used): the
 * glyph's current point ox pixels from the left edge (0, or the cell's
 * width for a mark that draws left of its point) and on the cell's
 * baseline. 0 when the engine has none or it is blank. Beyond the BMP the
 * 32-bit tag (diskfonttag.h: engines without it answer an error, and the
 * glyph is missing). */
static int render(struct vo_engine *e, ULONG cp, UBYTE *mask, WORD bpr, WORD w, WORD ox)
{
    struct Library *BulletBase = e->BulletBase;
    struct GlyphMap *gm = 0;
    struct TagItem t[2];
    WORD x, y, any = 0;
    if (set1(e, cp > 0xFFFF ? OT_GlyphCode_32 : OT_GlyphCode, cp) != OTERR_Success)
        return 0;
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
static void calibrate(struct vo_engine *e)
{
    struct Library *BulletBase = e->BulletBase;
    struct GlyphMap *gm = 0;
    struct TagItem t[3];
    static const ULONG probe[] = { 'M', 0xE0A0, 0x2588, '0' };
    ULONG xdpi = 72;
    int i;
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
}

/* the .otag read, checked and made a tag list; the engine its OT_Engine
 * names opened on it */
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
            if (e.ge)
                calibrate(&e);
            m->ok = 1;
            break;
        case VO_GLYPH: {
            WORD w = (WORD)(m->cells * e.cw);
            if (!e.ge) {
                m->ok = 0;
                break;
            }
            /* a mark: drawn left of its point over the cell, as fonts make
             * them; one that draws nothing there is a spacing mark */
            m->ok = m->mark && render(&e, m->cp, m->mask, m->bpr, w, w);
            if (!m->ok)
                m->ok = render(&e, m->cp, m->mask, m->bpr, w, 0) &&
                        !(m->cells == 1 && same_as_notdef(&e, m->mask, m->bpr));
            break;
        }
        case VO_READ: {
            /* a file the caller may not read itself (a DOS call; U2's
             * Unifont pages): a missing one is an answer, not a requester */
            BPTR fh = Open((STRPTR)m->name, MODE_OLDFILE);
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
    p = CreateNewProcTags(NP_Entry, (ULONG)worker, NP_Name, (ULONG)"UP-Term glyphs", NP_StackSize, 8192,
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
