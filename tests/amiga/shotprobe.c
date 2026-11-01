/* shotprobe: the R4.1 probe of thoughts/shared/plans/2026-10-29-amarchy-p4-capture-notify-power.md (a probe tool, not
 * shipped). One line per question, to stdout: shotprobe >RAM:shot.txt
 *  a  ReadPixelArray8 of an own chipset screen row equals the WritePixelArray8 input
 *  b  the front (Workbench) screen: depth, cybergraphics.library, ReadPixelArray8 and ReadPixelArray RECTFMT_RGB
 *  c  a PNG datatype: the class file, NewDTObject with a png base name
 *  d  clipboard.device takes a ~320 KB FORM ILBM (made by amarchy/shot_core.c) with CMD_WRITE, reads it back whole */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/displayinfo.h>
#include <devices/clipboard.h>
#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include "../../amarchy/shot_core.h"

long __stack = 65536;
const char *shotprobe_cookie = "$STACK:65536";

struct Library *CyberGfxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *DataTypesBase;
/* cybergraphics.library V41, LVOs as render/aa_ink.h uses -150 for FillPixelArray */
ULONG cgx_read(__reg("a6") struct Library *, __reg("a0") APTR dest, __reg("d0") UWORD dx, __reg("d1") UWORD dy,
               __reg("d2") UWORD dmod, __reg("a1") struct RastPort *rp, __reg("d3") UWORD sx,
               __reg("d4") UWORD sy, __reg("d5") UWORD w, __reg("d6") UWORD h,
               __reg("d7") UBYTE fmt) = "\tjsr\t-120(a6)";
void cgx_fill(__reg("a6") struct Library *, __reg("a1") struct RastPort *rp, __reg("d0") UWORD x, __reg("d1") UWORD y,
              __reg("d2") UWORD w, __reg("d3") UWORD h, __reg("d4") ULONG argb) = "\tjsr\t-150(a6)";
ULONG cgx_is(__reg("a6") struct Library *, __reg("d0") ULONG id) = "\tjsr\t-54(a6)";

static void say(const char *tag, const char *s) { printf("%s %s\n", tag, s); }

static void probe_a(void)
{
    struct Screen *sc;
    struct RastPort trp;
    struct BitMap *tbm;
    static UBYTE in[32 * 16], out[32 * 16];
    int i, bad = 0;
    long cgx;

    sc = OpenScreenTags(0, SA_Width, 320, SA_Height, 200, SA_Depth, 4, SA_DisplayID, PAL_MONITOR_ID | LORES_KEY,
                        SA_Quiet, TRUE, SA_Title, (ULONG)"shotprobe", TAG_END);
    if (!sc) { say("R4.1a", "[FAIL] OpenScreen chipset 320x200x4 failed"); return; }
    cgx = CyberGfxBase ? (long)cgx_is(CyberGfxBase, GetVPModeID(&sc->ViewPort)) : -1;
    for (i = 0; i < 32 * 16; i++) in[i] = (UBYTE)((i * 7 + i / 32) & 15);
    InitRastPort(&trp);
    tbm = AllocBitMap(32, 1, 8, 0, 0);
    trp.BitMap = tbm;
    WritePixelArray8(&sc->RastPort, 10, 10, 41, 25, in, &trp);
    ReadPixelArray8(&sc->RastPort, 10, 10, 41, 25, out, &trp);
    /* ReadPixelArray8 writes the rect packed with width 32 here (xmax-xmin+1 = 32) */
    for (i = 0; i < 32 * 16; i++) if (in[i] != out[i]) bad++;
    printf("R4.1a %s chipset screen modeid=0x%08lx depth=%d IsCyberModeID=%ld; WritePixelArray8 then ReadPixelArray8 32x16: %d of 512 differ\n",
           bad ? "[FAIL]" : "[OK]", (unsigned long)GetVPModeID(&sc->ViewPort), (int)sc->RastPort.BitMap->Depth, cgx, bad);
    FreeBitMap(tbm);
    CloseScreen(sc);
}

static void probe_b(void)
{
    struct Screen *sc;
    struct RastPort trp;
    struct BitMap *tbm;
    static UBYTE out8[16 * 4];
    static ULONG rgb[16];
    int depth, i, nonzero = 0;
    ULONG lock, id;

    lock = (ULONG)LockPubScreen(0);
    sc = (struct Screen *)lock;
    if (!sc) { say("R4.1b", "[FAIL] LockPubScreen"); return; }
    id = GetVPModeID(&sc->ViewPort);
    depth = (int)GetBitMapAttr(sc->RastPort.BitMap, BMA_DEPTH);
    printf("R4.1b [INFO] Workbench screen %dx%d depth=%d modeid=0x%08lx cybergraphics.library=%s IsCyberModeID=%ld\n",
           (int)sc->Width, (int)sc->Height, depth, (unsigned long)id, CyberGfxBase ? "open" : "ABSENT",
           CyberGfxBase ? (long)cgx_is(CyberGfxBase, id) : -1L);
    InitRastPort(&trp);
    tbm = AllocBitMap(16, 1, 8, 0, 0);
    trp.BitMap = tbm;
    memset(out8, 0xEE, sizeof out8);
    ReadPixelArray8(&sc->RastPort, 100, 100, 115, 100, out8, &trp);
    for (i = 0; i < 16; i++) if (out8[i] != 0xEE) nonzero++;
    printf("R4.1b [INFO] ReadPixelArray8 16x1 at 100,100 changed %d of 16 bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
           nonzero, out8[0], out8[1], out8[2], out8[3], out8[4], out8[5], out8[6], out8[7]);
    if (CyberGfxBase) {
        ULONG n;
        int f;
        for (f = 0; f < 3; f++) {
            memset(rgb, 0x11, sizeof rgb);
            n = cgx_read(CyberGfxBase, rgb, 0, 0, 16 * 4, &sc->RastPort, 100, 100, 16, 1, (UBYTE)f);
            printf("R4.1b [INFO] cgx ReadPixelArray fmt=%d (0 RGB 1 RGBA 2 ARGB) 16x1 at 100,100 rc=%ld: %08lx %08lx %08lx %08lx\n",
                   f, (long)n, (unsigned long)rgb[0], (unsigned long)rgb[1], (unsigned long)rgb[2], (unsigned long)rgb[3]);
        }
        cgx_fill(CyberGfxBase, &sc->RastPort, 100, 100, 16, 1, 0x00FF8040UL);
        memset(rgb, 0x11, sizeof rgb);
        n = cgx_read(CyberGfxBase, rgb, 0, 0, 16 * 4, &sc->RastPort, 100, 100, 16, 1, 2);
        printf("R4.1b [INFO] after FillPixelArray 00ff8040 then ReadPixelArray ARGB rc=%ld: %08lx %08lx\n", (long)n,
               (unsigned long)rgb[0], (unsigned long)rgb[15]);
        SetAPen(&sc->RastPort, 1);
    }
    FreeBitMap(tbm);
    UnlockPubScreen(0, sc);
}

static void probe_c(void)
{
    static const char *const f[] = { "SYS:Classes/DataTypes/png.datatype", "SYS:Classes/Datatypes/png.datatype",
                                     "DEVS:DataTypes/PNG", "SYS:Classes/DataTypes/picture.datatype" };
    BPTR l;
    int i;
    struct Library *dt = OpenLibrary((STRPTR)"datatypes.library", 39);
    Object *o;

    for (i = 0; i < 4; i++) {
        l = Lock((STRPTR)f[i], ACCESS_READ);
        printf("R4.1c [INFO] %s %s\n", f[i], l ? "present" : "absent");
        if (l) UnLock(l);
    }
    if (!dt) { say("R4.1c", "[INFO] datatypes.library absent"); return; }
    DataTypesBase = dt;
    {
        struct BitMap *bm = AllocBitMap(16, 8, 8, BMF_CLEAR, 0);
        static struct ColorRegister cr[256];
        static ULONG cregs[768];
        struct dtWrite dw;
        BPTR fh;
        long rc = -99;
        Object *o2;
        o2 = NewDTObject((APTR)"png", DTA_SourceType, DTST_RAM, DTA_BaseName, (ULONG)"png", DTA_GroupID, GID_PICTURE,
                         PDTA_BitMap, (ULONG)bm, PDTA_ColorRegisters, (ULONG)cr, PDTA_CRegs, (ULONG)cregs,
                         PDTA_NumColors, 256, TAG_END);
        printf("R4.1c [INFO] NewDTObject(baseName png, RAM source, bitmap 16x8x8) %s\n", o2 ? "created" : "failed");
        if (o2) {
            fh = Open((STRPTR)"RAM:probe.png", MODE_NEWFILE);
            dw.MethodID = DTM_WRITE; dw.dtw_GInfo = 0; dw.dtw_FileHandle = fh; dw.dtw_Mode = DTWM_RAW; dw.dtw_AttrList = 0;
            rc = (long)DoDTMethodA(o2, 0, 0, (Msg)&dw);
            Close(fh);
            printf("R4.1c [INFO] DTM_WRITE DTWM_RAW rc=%ld\n", rc);
            DisposeDTObject(o2);
        }
        FreeBitMap(bm);
    }
    o = 0;
    CloseLibrary(dt);
}

typedef struct { unsigned char *b; long n, cap; } msink;
static int m_write(void *ud, const void *buf, long n)
{
    msink *m = (msink *)ud;
    if (m->n + n > m->cap) return 1;
    memcpy(m->b + m->n, buf, (size_t)n);
    m->n += n;
    return 0;
}
static int m_patch(void *ud, long off, unsigned long v)
{
    msink *m = (msink *)ud;
    m->b[off] = (unsigned char)(v >> 24); m->b[off + 1] = (unsigned char)(v >> 16);
    m->b[off + 2] = (unsigned char)(v >> 8); m->b[off + 3] = (unsigned char)v;
    return 0;
}

static void probe_d(void)
{
    enum { W = 800, H = 400, CAP = 700000 };
    static UBYTE cmap[768], row[W];
    msink m;
    shot_sink s;
    shot_writer w;
    struct MsgPort *port;
    struct IOClipReq *io;
    unsigned long seed = 1, got = 0, ok = 1;
    long y, x, off, chunk;
    unsigned char *back;

    m.b = (unsigned char *)AllocMem(CAP, MEMF_PUBLIC); m.n = 0; m.cap = CAP;
    back = (unsigned char *)AllocMem(CAP, MEMF_PUBLIC);
    if (!m.b || !back) { say("R4.1d", "[FAIL] no memory"); return; }
    s.ud = &m; s.write = m_write; s.patch32 = m_patch;
    shot_begin(&w, &s, W, H, 8, cmap, 0x8000);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) { seed = seed * 1103515245UL + 12345UL; row[x] = (UBYTE)(seed >> 16); }
        shot_row(&w, row);
    }
    shot_end(&w);
    port = CreateMsgPort();
    io = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    if (!io || OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)io, 0)) {
        say("R4.1d", "[FAIL] clipboard.device open"); return;
    }
    io->io_Offset = 0;
    for (off = 0; off < m.n; off += chunk) {
        chunk = m.n - off > 32768 ? 32768 : m.n - off;
        io->io_Command = CMD_WRITE; io->io_Data = (STRPTR)(m.b + off); io->io_Length = chunk;
        if (DoIO((struct IORequest *)io) || io->io_Actual != (ULONG)chunk) { ok = 0; break; }
    }
    io->io_Command = CMD_UPDATE;
    ok = ok && !DoIO((struct IORequest *)io);
    io->io_Offset = 0; io->io_ClipID = 0;
    while (ok) {
        io->io_Command = CMD_READ; io->io_Data = (STRPTR)(back + got); io->io_Length = 32768;
        if (DoIO((struct IORequest *)io) || io->io_Actual == 0) break;
        got += io->io_Actual;
    }
    printf("R4.1d %s clipboard.device CMD_WRITE %ld bytes FORM ILBM %dx%d d8 (written ok=%lu), read back %lu bytes, equal=%d\n",
           (ok && got == (unsigned long)m.n && !memcmp(back, m.b, (size_t)m.n)) ? "[OK]" : "[FAIL]", m.n, W, H, ok,
           got, got == (unsigned long)m.n && !memcmp(back, m.b, (size_t)m.n));
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    FreeMem(m.b, CAP); FreeMem(back, CAP);
}

int main(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) return 20;
    CyberGfxBase = OpenLibrary((STRPTR)"cybergraphics.library", 41);
    probe_a();
    probe_b();
    probe_c();
    probe_d();
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
