/* palprobe: who holds which pen of the Workbench screen (vtcon L7, the Patina icon pens 32..47).
 *   palprobe            the screen ColorMap's PalExtra: free / shared lists, the refcount of every pen, and per pen
 *                       F (free), S<n> (shared, n users) or X (exclusive: in neither list)
 *   palprobe WATCHALL <s>  as WATCH, but every ColorMap (cm logged) and no screen lock: run it before IPrefs
 *                       (re-patches ObtainBestPenA on top whenever a later patch replaced palprobe's: event P, cm = the old vector, ret = tick)
 *   palprobe WATCH <s>  for <s> seconds patch ObtainPen / ObtainBestPenA / ReleasePen and log every call that touches
 *                       the Workbench ColorMap: task name, call, pen in / out. Then the PalExtra dump.
 * Patched calls never do I/O: they append to a ring under Disable and the main task prints it. */
#include <exec/types.h>
#include <exec/tasks.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <stdlib.h>

long __stack = 16384;
const char palprobe_stack_cookie[] = "$STACK: 16384";

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#define LVO_OBTAINBESTPENA (-840)
#define LVO_RELEASEPEN     (-948)
#define LVO_OBTAINPEN      (-954)

typedef struct { char task[20]; char call; long pen; long ret; ULONG rgb; void *cm; } ev;
#define NEV 4096
static ev evs[NEV];
static volatile int nev;
static struct ColorMap *watch_cm;
static int watch_all;              /* WATCHALL: every ColorMap, the screen lock is not held (an IPrefs reset can reopen the Workbench) */

static LONG (*o_best)(__reg("a0") struct ColorMap *, __reg("d1") ULONG, __reg("d2") ULONG, __reg("d3") ULONG,
                      __reg("a1") struct TagItem *, __reg("a6") struct GfxBase *);
static LONG (*o_obtain)(__reg("a0") struct ColorMap *, __reg("d0") ULONG, __reg("d1") ULONG, __reg("d2") ULONG,
                        __reg("d3") ULONG, __reg("d4") ULONG, __reg("a6") struct GfxBase *);
static void (*o_release)(__reg("a0") struct ColorMap *, __reg("d0") ULONG, __reg("a6") struct GfxBase *);

static void log_ev(void *cm, char call, long pen, long ret, ULONG rgb)
{
    struct Task *t = FindTask(NULL);
    const char *n = t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
    int i;
    Disable();
    i = nev;
    if (i < NEV)
        nev = i + 1;
    Enable();
    if (i >= NEV)
        return;
    strncpy(evs[i].task, n, sizeof evs[i].task - 1);
    evs[i].task[sizeof evs[i].task - 1] = 0;
    evs[i].call = call;
    evs[i].pen = pen;
    evs[i].ret = ret;
    evs[i].rgb = rgb;
    evs[i].cm = cm;
}

static LONG __saveds p_best(__reg("a0") struct ColorMap *cm, __reg("d1") ULONG r, __reg("d2") ULONG g, __reg("d3") ULONG b,
                            __reg("a1") struct TagItem *tags, __reg("a6") struct GfxBase *gb)
{
    LONG ret = o_best(cm, r, g, b, tags, gb);
    if (watch_all || cm == watch_cm)
        log_ev(cm, 'B', -1, ret, ((r >> 8) & 0xff0000) | ((g >> 16) & 0xff00) | (b >> 24));
    return ret;
}

static LONG __saveds p_obtain(__reg("a0") struct ColorMap *cm, __reg("d0") ULONG n, __reg("d1") ULONG r, __reg("d2") ULONG g,
                              __reg("d3") ULONG b, __reg("d4") ULONG f, __reg("a6") struct GfxBase *gb)
{
    LONG ret = o_obtain(cm, n, r, g, b, f, gb);
    if (watch_all || cm == watch_cm)
        log_ev(cm, (f & PENF_EXCLUSIVE) ? 'X' : 'O', (long)n, ret, ((r >> 8) & 0xff0000) | ((g >> 16) & 0xff00) | (b >> 24));
    return ret;
}

static void __saveds p_release(__reg("a0") struct ColorMap *cm, __reg("d0") ULONG n, __reg("a6") struct GfxBase *gb)
{
    if (watch_all || cm == watch_cm)
        log_ev(cm, 'R', (long)n, 0, 0);
    o_release(cm, n, gb);
}

/* The JMP target of a graphics.library vector: palprobe's stub while its patch is the newest, else whoever patched after it */
static APTR vec(LONG lvo)
{
    return *(APTR *)((UBYTE *)GfxBase + lvo + 2);
}

static void vecs(void)
{
    Printf("vec -840 %08lx (p_best %08lx) -954 %08lx (p_obtain %08lx) -948 %08lx (p_release %08lx)\n", (ULONG)vec(LVO_OBTAINBESTPENA),
           (ULONG)p_best, (ULONG)vec(LVO_OBTAINPEN), (ULONG)p_obtain, (ULONG)vec(LVO_RELEASEPEN), (ULONG)p_release);
}

/* self-check: an exact ObtainBestPen of pen 0's colour on the Workbench (a match, no pen allocated) must log one B event */
static void best_selfcheck(void)
{
    struct Screen *s = LockPubScreen("Workbench");
    ULONG c[3];
    LONG got;
    if (!s)
        return;
    GetRGB32(s->ViewPort.ColorMap, 0, 1, c);
    got = ObtainBestPen(s->ViewPort.ColorMap, c[0], c[1], c[2], OBP_Precision, PRECISION_EXACT, OBP_FailIfBad, TRUE, TAG_DONE);
    if (got >= 0)
        ReleasePen(s->ViewPort.ColorMap, (ULONG)got);
    Printf("selfcheck ObtainBestPen -> %ld\n", (long)got);
    UnlockPubScreen(NULL, s);
}

static void dump(struct ColorMap *cm)
{
    struct PaletteExtra *pe = cm->PalExtra;
    UBYTE st[256];
    int i, p, n = cm->Count > 256 ? 256 : cm->Count;
    if (!pe) {
        Printf("no PalExtra\n");
        return;
    }
    ObtainSemaphore(&pe->pe_Semaphore);
    memset(st, 'X', sizeof st);
    Printf("count %ld sharable %ld nfree %ld firstfree %ld nshared %ld firstshared %ld\n", (long)cm->Count,
           (long)pe->pe_SharableColors, (long)pe->pe_NFree, (long)pe->pe_FirstFree, (long)pe->pe_NShared,
           (long)pe->pe_FirstShared);
    for (i = 0, p = pe->pe_FirstFree; i < pe->pe_NFree && p < n; i++, p = pe->pe_AllocList[p])
        st[p] = 'F';
    Printf("free order:");
    for (i = 0, p = pe->pe_FirstFree; i < pe->pe_NFree && i < 24 && p < n; i++, p = pe->pe_AllocList[p])
        Printf(" %ld", (long)p);
    Printf("\n");
    for (i = 0, p = pe->pe_FirstShared; i < pe->pe_NShared && p < n; i++, p = pe->pe_AllocList[p])
        st[p] = 'S';
    for (i = 0; i < n; i++) {
        if (st[i] == 'S')
            Printf("%ld:S%ld ", (long)i, (long)pe->pe_RefCnt[i]);
        else
            Printf("%ld:%lc ", (long)i, (long)st[i]);
        if ((i & 15) == 15)
            Printf("\n");
    }
    ReleaseSemaphore(&pe->pe_Semaphore);
    Printf("rgb");                      /* the colour of the first 16 pens: a rig reads the theme role pens 4-7 here */
    for (i = 0; i < n && i < 16; i++) {
        ULONG c[3];
        GetRGB32(cm, (ULONG)i, 1, c);
        Printf(" %ld:%06lx", (long)i, (long)(((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24)));
    }
    Printf("\n");
}

int main(int argc, char **argv)
{
    struct Screen *s;
    int rc = 0, i, secs = 0;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    if (argc >= 3 && (argv[1][0] == 'W' || argv[1][0] == 'w'))
        secs = atoi(argv[2]);
    if (secs > 0 && strlen(argv[1]) > 5)
        watch_all = 1;
    if (watch_all) {                    /* WATCHALL <s>: from the Startup-Sequence before IPrefs; the Workbench may not exist yet */
        Forbid();
        o_best = (void *)SetFunction((struct Library *)GfxBase, LVO_OBTAINBESTPENA, (APTR)p_best);
        o_obtain = (void *)SetFunction((struct Library *)GfxBase, LVO_OBTAINPEN, (APTR)p_obtain);
        o_release = (void *)SetFunction((struct Library *)GfxBase, LVO_RELEASEPEN, (APTR)p_release);
        Permit();
        for (i = 0; i < secs * 50; i++) {    /* a later SetFunction (rtg.library's ObtainBestPenA) may not chain: go on top again */
            Delay(1);
            Forbid();
            if (vec(LVO_OBTAINBESTPENA) != (APTR)p_best) {
                o_best = (void *)SetFunction((struct Library *)GfxBase, LVO_OBTAINBESTPENA, (APTR)p_best);
                log_ev((void *)o_best, 'P', -1, i, 0);
            }
            Permit();
        }
        best_selfcheck();
        vecs();
        Forbid();
        SetFunction((struct Library *)GfxBase, LVO_OBTAINBESTPENA, (APTR)o_best);
        SetFunction((struct Library *)GfxBase, LVO_OBTAINPEN, (APTR)o_obtain);
        SetFunction((struct Library *)GfxBase, LVO_RELEASEPEN, (APTR)o_release);
        Permit();
        Delay(25);
        for (i = 0; i < nev && i < NEV; i++)
            Printf("ev cm %08lx %s %lc pen %ld -> %ld rgb %06lx\n", (ULONG)evs[i].cm, evs[i].task, (long)evs[i].call, evs[i].pen,
                   evs[i].ret, evs[i].rgb);
        Printf("events %ld\n", (long)nev);
        secs = 0;
    }
    s = LockPubScreen("Workbench");
    if (!s) {
        Printf("no Workbench screen\n");
        rc = 10;
    } else {
        if (secs > 0) {
            watch_cm = s->ViewPort.ColorMap;
            Forbid();
            o_best = (void *)SetFunction((struct Library *)GfxBase, LVO_OBTAINBESTPENA, (APTR)p_best);
            o_obtain = (void *)SetFunction((struct Library *)GfxBase, LVO_OBTAINPEN, (APTR)p_obtain);
            o_release = (void *)SetFunction((struct Library *)GfxBase, LVO_RELEASEPEN, (APTR)p_release);
            Permit();
            Printf("watching %ld s\n", (long)secs);
            Delay(secs * 50);
            Forbid();
            SetFunction((struct Library *)GfxBase, LVO_OBTAINBESTPENA, (APTR)o_best);
            SetFunction((struct Library *)GfxBase, LVO_OBTAINPEN, (APTR)o_obtain);
            SetFunction((struct Library *)GfxBase, LVO_RELEASEPEN, (APTR)o_release);
            Permit();
            Delay(25);                                   /* a caller still inside a patch returns before we exit */
            for (i = 0; i < nev && i < NEV; i++)
                Printf("ev %s %lc pen %ld -> %ld rgb %06lx\n", evs[i].task, (long)evs[i].call, evs[i].pen, evs[i].ret,
                       evs[i].rgb);
            Printf("events %ld\n", (long)nev);
        }
        Printf("depth %ld modes %08lx cm %08lx\n", (long)s->BitMap.Depth, (ULONG)s->ViewPort.Modes, (ULONG)s->ViewPort.ColorMap);
        dump(s->ViewPort.ColorMap);
        UnlockPubScreen(NULL, s);
    }
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
