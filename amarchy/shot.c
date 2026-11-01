/* shot: see shot.h. */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/layers.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <string.h>
#include "shot.h"
#include "shot_core.h"
#include "win.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "../handler/clip.h"

extern struct IntuitionBase *IntuitionBase;

/* cybergraphics.library V41 ReadPixelArray (LVO -120, NOT -126 which is WritePixelArray), as tests/amiga/shotprobe.c
 * proved on rig 4; RECTFMT_ARGB is 2 */
#define SHOT_RECTFMT_ARGB 2
static ULONG cgx_read(__reg("a6") struct Library *, __reg("a0") APTR dest, __reg("d0") UWORD dx, __reg("d1") UWORD dy,
                      __reg("d2") UWORD dmod, __reg("a1") struct RastPort *rp, __reg("d3") UWORD sx,
                      __reg("d4") UWORD sy, __reg("d5") UWORD w, __reg("d6") UWORD h,
                      __reg("d7") UBYTE fmt) = "\tjsr\t-120(a6)";

static long g_count;
static int g_rc;
static char g_last[128];
static struct Window *g_rw;             /* the region window */
static long g_x0, g_y0, g_x1, g_y1;
static int g_down, g_framed;
static const char *g_rdir;
static char g_rdirbuf[96];

static int sink_write(void *ud, const void *buf, long n)
{
    return Write((BPTR)ud, (APTR)buf, n) == n ? 0 : 1;
}

static int sink_patch32(void *ud, long off, unsigned long v)
{
    unsigned char b[4];
    LONG here = Seek((BPTR)ud, 0, OFFSET_CURRENT);
    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)v;
    if (here < 0 || Seek((BPTR)ud, off, OFFSET_BEGINNING) < 0 || Write((BPTR)ud, b, 4) != 4)
        return 1;
    return Seek((BPTR)ud, here, OFFSET_BEGINNING) < 0;
}

/* the shot-dir key of the Amarchy settings file (amconf), read at every capture so a Prefs edit applies at once */
const char *shot_default_dir(void)
{
    static char dir[UC_MAX_VALUE];
    am_prefs p;
    amconf_load(&p);
    strncpy(dir, amconf_shot_dir(&p), sizeof dir - 1);
    dir[sizeof dir - 1] = 0;
    return dir;
}
long shot_count(void) { return g_count; }
const char *shot_last(void) { return g_last; }
int shot_last_rc(void) { return g_rc; }

/* dir/name; a second shot within the same second gets -2, -3 ... before the suffix */
static BPTR open_new(const char *dir, char *path, long cap)
{
    struct DateStamp ds;
    char name[32];
    BPTR l;
    long n, dl = (long)strlen(dir);
    int k;
    l = Lock((STRPTR)dir, ACCESS_READ);
    if (l)
        UnLock(l);
    else if ((l = CreateDir((STRPTR)dir)) != 0)
        UnLock(l);
    DateStamp(&ds);
    shot_stamp_name(ds.ds_Days, ds.ds_Minute, ds.ds_Tick, name);
    for (k = 1; k < 100; k++) {
        n = dl;
        if (n + 32 >= cap)
            return 0;
        memcpy(path, dir, (size_t)dl);
        if (dl && dir[dl - 1] != ':' && dir[dl - 1] != '/')
            path[n++] = '/';
        memcpy(path + n, name, 19);                 /* "shot-YYYYMMDD-HHMMSS" */
        n += 19;
        if (k > 1) {
            path[n++] = '-';
            path[n++] = (char)('0' + k / 10);
            path[n++] = (char)('0' + k % 10);
        }
        memcpy(path + n, ".iff", 5);
        l = Lock((STRPTR)path, ACCESS_READ);
        if (l) {
            UnLock(l);
            continue;
        }
        return Open((STRPTR)path, MODE_NEWFILE);
    }
    return 0;
}

/* Truecolour screen (D-RTG): 24-bit deep ILBM, rows read with cgx ReadPixelArray as ARGB, streamed like the 8-bit path */
static int shot_take_deep(struct Screen *sc, struct Library *cgx, long x, long y, long w, long h, const char *dir,
                          int clip, char *path, long cap)
{
    shot_writer sw;
    shot_sink sk;
    unsigned char *row;
    BPTR fh;
    int rc = SHOT_OK;
    long r;
    row = (unsigned char *)AllocVec((ULONG)w * 4 + 32, MEMF_ANY);
    if (!row)
        return SHOT_IO;
    fh = open_new(dir, path, cap);
    if (!fh) {
        FreeVec(row);
        return SHOT_IO;
    }
    sk.ud = (void *)fh;
    sk.write = sink_write;
    sk.patch32 = sink_patch32;
    if (shot_begin_deep(&sw, &sk, w, h, GetVPModeID(&sc->ViewPort)) != 0)
        rc = SHOT_IO;
    for (r = 0; rc == SHOT_OK && r < h; r++) {
        cgx_read(cgx, row, 0, 0, (UWORD)(w * 4), &sc->RastPort, (UWORD)x, (UWORD)(y + r), (UWORD)w, 1, SHOT_RECTFMT_ARGB);
        if (shot_row_argb(&sw, row) != 0)
            rc = SHOT_IO;
    }
    if (rc == SHOT_OK && shot_end(&sw) != 0)
        rc = SHOT_IO;
    Close(fh);
    if (rc == SHOT_OK && clip && !clip_write_form(path))
        rc = SHOT_IO;
    FreeVec(row);
    return rc;
}

int shot_take(struct Screen *sc, long x, long y, long w, long h, const char *dir, int clip, char *path, long cap)
{
    shot_writer sw;
    shot_sink sk;
    struct RastPort trp;
    struct BitMap *tbm;
    unsigned char *row, cmap[256 * 3];
    ULONG rgb[256 * 3];
    BPTR fh;
    int depth, ncol, i, rc = SHOT_OK;
    long r;
    if (!sc)
        return SHOT_NOSCR;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sc->Width) w = sc->Width - x;
    if (y + h > sc->Height) h = sc->Height - y;
    if (w <= 0 || h <= 0)
        return SHOT_BAD;
    depth = (int)GetBitMapAttr(sc->RastPort.BitMap, BMA_DEPTH);
    if (depth > 8) {
        struct Library *cgx = OpenLibrary((STRPTR)"cybergraphics.library", 41);
        if (!cgx)
            return SHOT_DEEP;
        rc = shot_take_deep(sc, cgx, x, y, w, h, dir, clip, path, cap);
        CloseLibrary(cgx);
        return rc;
    }
    ncol = 1 << depth;
    GetRGB32(sc->ViewPort.ColorMap, 0, (ULONG)ncol, rgb);
    for (i = 0; i < ncol * 3; i++)
        cmap[i] = (unsigned char)(rgb[i] >> 24);
    row = (unsigned char *)AllocVec((ULONG)w + 32, MEMF_ANY);
    InitRastPort(&trp);
    tbm = AllocBitMap((ULONG)((w + 15) & ~15L), 1, 8, 0, 0);
    if (!row || !tbm) {
        rc = SHOT_IO;
        goto out;
    }
    trp.BitMap = tbm;
    fh = open_new(dir, path, cap);
    if (!fh) {
        rc = SHOT_IO;
        goto out;
    }
    sk.ud = (void *)fh;
    sk.write = sink_write;
    sk.patch32 = sink_patch32;
    if (shot_begin(&sw, &sk, w, h, depth, cmap, GetVPModeID(&sc->ViewPort)) != 0)
        rc = SHOT_IO;
    for (r = 0; rc == SHOT_OK && r < h; r++) {
        ReadPixelArray8(&sc->RastPort, (UWORD)x, (UWORD)(y + r), (UWORD)(x + w - 1), (UWORD)(y + r), row, &trp);
        if (shot_row(&sw, row) != 0)
            rc = SHOT_IO;
    }
    if (rc == SHOT_OK && shot_end(&sw) != 0)
        rc = SHOT_IO;
    Close(fh);
    if (rc == SHOT_OK && clip && !clip_write_form(path))
        rc = SHOT_IO;
out:
    if (tbm)
        FreeBitMap(tbm);
    if (row)
        FreeVec(row);
    return rc;
}

static int finish(int rc, const char *path)
{
    g_rc = rc;
    if (rc == SHOT_OK) {
        g_count++;
        strncpy(g_last, path, sizeof g_last - 1);
        g_last[sizeof g_last - 1] = 0;
    }
    return rc;
}

static struct Screen *front_screen(void)
{
    struct Screen *s;
    ULONG lock;
    if (!IntuitionBase)
        return 0;
    lock = LockIBase(0);
    s = IntuitionBase->FirstScreen;
    UnlockIBase(lock);
    return s;
}

int shot_screen(const char *dir)
{
    char path[128];
    struct Screen *sc = front_screen();
    if (!sc)
        return finish(SHOT_NOSCR, "");
    return finish(shot_take(sc, 0, 0, sc->Width, sc->Height, dir, 1, path, sizeof path), path);
}

int shot_window(const char *dir)
{
    char path[128];
    struct Screen *sc = 0;
    long x, y, w, h;
    if (!win_target_rect(&sc, &x, &y, &w, &h))
        return finish(SHOT_NOSCR, "");
    return finish(shot_take(sc, x, y, w, h, dir, 1, path, sizeof path), path);
}

/* ---- region ---- */

static void frame(void)
{
    long xa = g_x0 < g_x1 ? g_x0 : g_x1, xb = g_x0 < g_x1 ? g_x1 : g_x0;
    long ya = g_y0 < g_y1 ? g_y0 : g_y1, yb = g_y0 < g_y1 ? g_y1 : g_y0;
    struct RastPort *rp = g_rw->RPort;
    SetDrMd(rp, COMPLEMENT);
    Move(rp, xa, ya);
    Draw(rp, xb, ya);
    Draw(rp, xb, yb);
    Draw(rp, xa, yb);
    Draw(rp, xa, ya);
    g_framed = !g_framed;
}

static void clear_frame(void)
{
    if (g_framed)
        frame();
}

int shot_region(const char *dir)
{
    struct Screen *sc = front_screen();
    if (g_rw)
        return SHOT_OK;
    if (!sc)
        return finish(SHOT_NOSCR, "");
    strncpy(g_rdirbuf, dir, sizeof g_rdirbuf - 1);
    g_rdirbuf[sizeof g_rdirbuf - 1] = 0;
    g_rdir = g_rdirbuf;
    g_down = g_framed = 0;
    g_rw = OpenWindowTags(0, WA_CustomScreen, (ULONG)sc, WA_Left, 0, WA_Top, 0, WA_Width, sc->Width, WA_Height, sc->Height,
                          WA_Borderless, TRUE, WA_RMBTrap, TRUE, WA_Activate, TRUE, WA_SimpleRefresh, TRUE,
                          WA_NoCareRefresh, TRUE, WA_BackFill, (ULONG)LAYERS_NOBACKFILL, WA_ReportMouse, TRUE,
                          WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY, TAG_END);
    if (!g_rw)
        return finish(SHOT_IO, "");
    return SHOT_OK;
}

int shot_region_open_p(void) { return g_rw != 0; }

unsigned long shot_region_sigmask(void)
{
    return g_rw ? 1UL << g_rw->UserPort->mp_SigBit : 0;
}

static void region_close(void)
{
    clear_frame();
    CloseWindow(g_rw);
    g_rw = 0;
}

void shot_region_serve(void)
{
    struct IntuiMessage *m;
    while (g_rw && (m = (struct IntuiMessage *)GetMsg(g_rw->UserPort)) != 0) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        long mx = m->MouseX, my = m->MouseY;
        ReplyMsg((struct Message *)m);
        if (cls == IDCMP_RAWKEY && code == 0x45) {           /* Esc: no file, no count */
            region_close();
        } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
            g_x0 = g_x1 = mx;
            g_y0 = g_y1 = my;
            g_down = 1;
            frame();
        } else if (cls == IDCMP_MOUSEMOVE && g_down) {
            clear_frame();
            g_x1 = mx;
            g_y1 = my;
            frame();
        } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTUP && g_down) {
            struct Screen *sc = g_rw->WScreen;
            long xa, ya, w, h;
            char path[128];
            g_x1 = mx;
            g_y1 = my;
            xa = g_x0 < g_x1 ? g_x0 : g_x1;
            ya = g_y0 < g_y1 ? g_y0 : g_y1;
            w = (g_x0 < g_x1 ? g_x1 - g_x0 : g_x0 - g_x1) + 1;     /* inclusive */
            h = (g_y0 < g_y1 ? g_y1 - g_y0 : g_y0 - g_y1) + 1;
            region_close();                                       /* the frame must not be in the shot */
            finish(shot_take(sc, xa, ya, w, h, g_rdir, 1, path, sizeof path), path);
        }
    }
}
