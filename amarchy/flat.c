/* amarchy-flat: the resident flat-UI program (plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0 / C1, OD-F8).
 *
 *   amarchy-flat            start: make the public state block "Amarchy.Flat", print its STATUS, wait for Ctrl-C
 *   amarchy-flat STATUS     print the STATUS line of the running program and return (flat=on|off|absent|held)
 *
 * C1 swaps the h_SubEntry of sysiclass's dispatcher Hook (never h_Entry: one routine serves five classes) for a
 * pass-through trampoline that counts the call and tail-jumps to the saved routine, so every register and the
 * stack arrive unchanged (3.2 takes a0 / a2 / a1, 3.1 the C stack: one code path for both). No glyph is drawn yet
 * (C2). Ctrl-C removes the swap only if the slot is still ours; otherwise it reports still_chained and stays
 * inert. The swap model and the STATUS text are flat_core.c (host-tested).
 *
 * FLAT-D4 (runtime DOpus patch): when dopus5.library is resident and its version, revision and routine checksums are in
 * the table, the JMP operand of the DrawBox (LVO -696) and DrawFieldBox (-702) jump table entries is swapped with the
 * same flat_swap model for a routine that draws nothing while flat is on and the pens are flat (no shadow L), and
 * calls the saved routine otherwise. Any other build is inert (STATUS "dopus=<v.r> inert=..."). Not reachable by an
 * LVO, so not patched: path field (button_class.c:900 calls draw_field_box inside the library), scroller knob and
 * arrows (dopusiclass IM_BBOX / arrows, image_class.c:261, :109), sort arrow (lister_title.c:972-987, inline in the
 * DirectoryOpus program). `amarchy-flat DOPUS` makes the running program look for DOpus again (Ctrl-D signal).
 * Return code: 0 done, 5 another copy is running, 10 no memory. */
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <graphics/rastport.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <utility/hooks.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "../tty/qmark.h"
#include "flat_core.h"
#include "flat_glyph.h"
#include "taskalive.h"

static const char ver[] = "$VER: amarchy-flat 0.1 (30.10.2026) (UP-Term)";

/* RULES.md rule 6: the stack the program asks for, and the cookie the Shell and Workbench read. */
long __stack = 16384;
const char amarchy_flat_stack_cookie[] = "$STACK: 16384";

/* The public record. ss is first so AddSemaphore's node is the allocation's start (as ixshared, ixwatch.h). */
typedef struct flatshared {
    struct SignalSemaphore ss;
    char name[16];
    struct Task *task;          /* the resident program; only compared, never dereferenced alone */
    char tname[32];
    flat_state st;
    flat_swap sw;
    uintptr_t *slot;            /* &sysiclass->cl_Dispatcher.h_SubEntry while installed */
    /* FLAT-D4 */
    struct Library *dlib;       /* dopus5.library, opened while patched so it cannot expunge under the patch */
    flat_swap dsw[FLAT_EL_COUNT];
    uintptr_t *dslot[FLAT_EL_COUNT];
    unsigned dver, drev;
    int dreason, dmask, dfound;
    uint32_t dsum[FLAT_EL_COUNT]; /* flat_sum of each routine at the last scan (printed by STATUS) */
    ULONG dcalls, dflat;        /* every patched call / every call that drew flat (the sentinels) */
} flatshared;

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
static flatshared *g_r;

/* ---- FLAT-C2: the sysiclass dispatcher. It replaces the C1 pass-through: count the call, and for IM_DRAW of a
 * SYSIA_Which that flat_glyph.c knows, draw the flat glyph when the switch is on; every other call (other method,
 * other Which, switch off, held, box too small, OM_GET failing) goes to the saved routine with the arguments as
 * received. 3.2 passes hook / object / message in a0 / a2 / a1, 3.1 on the C stack (flat_conv), so there are two
 * entries over one body. The image is the object itself (struct Image: Width, Height); Which is read with GetAttr, never
 * from the instance. GetAttr(OM_GET) comes back through this dispatcher with a non-draw method: no recursion. */
typedef ULONG (*oldfn_reg)(__reg("a0") struct Hook *, __reg("a2") Object *, __reg("a1") Msg);
typedef ULONG (*oldfn_stk)(struct Hook *, Object *, Msg);

static void c2_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct RastPort *rp = (struct RastPort *)ctx;
    SetAPen(rp, pen);
    RectFill(rp, x0, y0, x1, y1);
}

static int c2_draw(Object *obj, Msg msg)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    ULONG which = 0xFFFF;
    r->st.calls++;
    if (msg->MethodID != FLAT_IM_DRAW || !r->st.enabled || r->st.held || !d->imp_RPort)
        return 0;
    if (GetAttr(SYSIA_Which, obj, &which) == 0) {
        /* Kickstart 3.1 sysiclass does not answer OM_GET: find the window gadget that renders this image. The layer
         * of the rastport belongs to the window being drawn; reading its gadget list from Intuition's own drawing
         * context needs no Forbid (the list only changes through Add/RemoveGList by the window's owner). */
        struct Layer *lay = d->imp_RPort->Layer;
        struct Window *w = lay ? (struct Window *)lay->Window : NULL;
        struct Gadget *g;
        int n = 0, wh = -1;
        for (g = w ? w->FirstGadget : NULL; g && n < 64 && wh < 0; g = g->NextGadget, n++)
            if ((g->GadgetRender == (APTR)obj || g->SelectRender == (APTR)obj) && (g->GadgetType & GTYP_SYSGADGET))
                wh = flat_which_from_gtype(g->GadgetType);
        if (wh < 0)
            return 0;
        which = (ULONG)wh;
    }
    return flat_glyph_draw(&r->st, which, msg->MethodID, d->imp_State, im->LeftEdge + d->imp_Offset.X,
                           im->TopEdge + d->imp_Offset.Y, im->Width, im->Height, c2_fill, d->imp_RPort);
}

ULONG c2_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (c2_draw(obj, msg))
        return TRUE;
    return ((oldfn_reg)g_r->sw.saved)(h, obj, msg);
}

ULONG c2_disp_stk(struct Hook *h, Object *obj, Msg msg)
{
    if (c2_draw(obj, msg))
        return TRUE;
    return ((oldfn_stk)g_r->sw.saved)(h, obj, msg);
}

/* ---- FLAT-D4: the replacement routines. They run on the caller's task with the caller's registers (a0 rp, a1 rect,
 * a2 info, d0 recessed). Flat on and flat pens: draw nothing. Otherwise the saved routine, same arguments. */
typedef void (*boxfn)(__reg("a0") struct RastPort *, __reg("a1") struct Rectangle *, __reg("a2") struct DrawInfo *,
                      __reg("d0") LONG);
typedef void (*fieldfn)(__reg("a0") struct RastPort *, __reg("a1") struct Rectangle *, __reg("a2") struct DrawInfo *);

static int d4_flat(int el, struct DrawInfo *info, LONG recessed)
{
    flatshared *r = g_r;
    r->dcalls++;
    if (flat_dopus_decide(&r->st, el, recessed ? 1 : 0, info->dri_Pens[SHINEPEN], info->dri_Pens[BACKGROUNDPEN]) != FLAT_DRAW_SKIP)
        return 0;
    r->dflat++;
    return 1;
}

static void d4_drawbox(__reg("a0") struct RastPort *rp, __reg("a1") struct Rectangle *rect, __reg("a2") struct DrawInfo *info,
                       __reg("d0") LONG recessed)
{
    if (!d4_flat(FLAT_EL_DRAWBOX, info, recessed))
        ((boxfn)g_r->dsw[FLAT_EL_DRAWBOX].saved)(rp, rect, info, recessed);
}

static void d4_fieldbox(__reg("a0") struct RastPort *rp, __reg("a1") struct Rectangle *rect, __reg("a2") struct DrawInfo *info)
{
    if (!d4_flat(FLAT_EL_FIELDBOX, info, 0))
        ((fieldfn)g_r->dsw[FLAT_EL_FIELDBOX].saved)(rp, rect, info);
}

static const LONG d4_lvo[FLAT_EL_COUNT] = { FLAT_LVO_DRAWBOX, FLAT_LVO_FIELDBOX };

static flatshared *find_record(void)
{
    flatshared *r;
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    Permit();
    return r;
}

/* Print the STATUS line of the record in memory (a copy taken under Forbid; the layout is checked before use). */
static void print_status(void)
{
    flat_state copy;
    char line[96];
    flatshared *r;
    int found = 0, dfound = 0, dreason = 0, dmask = 0, dinst = 0;
    unsigned dver = 0, drev = 0;
    unsigned long dcalls = 0, dflat = 0;
    uint32_t dsum[FLAT_EL_COUNT] = { 0, 0 };
    memset(&copy, 0, sizeof copy);
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r) {
        found = 1;
        copy = r->st;
        dfound = r->dfound; dver = r->dver; drev = r->drev; dreason = r->dreason; dmask = r->dmask;
        dinst = r->dsw[0].installed || r->dsw[1].installed; dcalls = r->dcalls; dflat = r->dflat;
        dsum[0] = r->dsum[0]; dsum[1] = r->dsum[1];
    }
    Permit();
    if (flat_status_line(&copy, sizeof copy, found, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found && flat_dopus_status_line(dfound, dver, drev, dreason, dmask, dinst, dcalls, dflat, dsum, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
}

static int is_status(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return (a[0] == 'S' || a[0] == 's') && (a[1] == 'T' || a[1] == 't') && (a[2] == 'A' || a[2] == 'a') &&
           (a[3] == 'T' || a[3] == 't') && (a[4] == 'U' || a[4] == 'u') && (a[5] == 'S' || a[5] == 's') &&
           (a[6] == 0 || a[6] == ' ' || a[6] == '\t' || a[6] == '\n');
}

static int is_dopus(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return (a[0] == 'D' || a[0] == 'd') && (a[1] == 'O' || a[1] == 'o') && (a[2] == 'P' || a[2] == 'p') &&
           (a[3] == 'U' || a[3] == 'u') && (a[4] == 'S' || a[4] == 's') && (a[5] == 0 || a[5] == ' ' || a[5] == '\t' || a[5] == '\n');
}

static int is_help(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return qmark_line(a) || (a[0] == '-' && ((a[1] == 'h' && (a[2] == 0 || a[2] == ' ' || a[2] == '\n')) || (a[1] == '-' && a[2] == 'h')));
}

/* The slot to swap: h_SubEntry of the class that every system image (SYSIA_Which 0..0x2C) shares. The class is found
 * through a throw-away sysiclass object. 0 when it cannot be made. */
static uintptr_t *sysiclass_slot(void)
{
    struct TagItem t[3];
    struct Screen *scr = LockPubScreen(NULL);
    struct DrawInfo *di = scr ? GetScreenDrawInfo(scr) : NULL;
    uintptr_t *slot = 0;
    if (di) {
        Object *o;
        t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)di;
        t[1].ti_Tag = SYSIA_Which;    t[1].ti_Data = CLOSEIMAGE;
        t[2].ti_Tag = TAG_DONE;       t[2].ti_Data = 0;
        o = NewObjectA(NULL, (UBYTE *)"sysiclass", t);
        if (o) {
            struct IClass *cl = OCLASS(o);
            slot = (uintptr_t *)&cl->cl_Dispatcher.h_SubEntry;     /* the class outlives the object: it is a public class */
            DisposeObject(o);
        }
    }
    if (di)
        FreeScreenDrawInfo(scr, di);
    if (scr)
        UnlockPubScreen(NULL, scr);
    return slot;
}

/* Install the pass-through under Forbid. Returns FLAT_OK or a negative flat_swap code / -100 (no class) / -101 (memory). */
static int hook_install(flatshared *r)
{
    uintptr_t *slot = sysiclass_slot();
    uintptr_t ours = flat_conv(IntuitionBase->LibNode.lib_Version) == FLAT_CONV_REG ? (uintptr_t)c2_disp_reg : (uintptr_t)c2_disp_stk;
    int rc;
    if (!slot)
        return -100;
    g_r = r;                            /* the dispatcher reads it from its first call on */
    Forbid();
    rc = flat_swap_install(&r->sw, slot, ours);     /* r->sw.saved is the old routine before the first dispatcher call */
    Permit();
    if (rc != FLAT_OK)
        return rc;
    r->slot = slot;
    return FLAT_OK;
}

/* Remove the swap if the slot is still ours (Forbid). The dispatcher stays reachable while a
 * foreign hook is chained on top of it, its code is still reachable and must stay. */
static int hook_remove(flatshared *r)
{
    int rc = FLAT_NOT_INSTALLED;
    if (!r->sw.installed)
        return rc;
    Forbid();
    rc = flat_swap_remove(&r->sw, r->slot, &r->st);
    Permit();
    if (rc == FLAT_REMOVED) {
        Delay(5);                       /* a caller that already entered the dispatcher finishes its draw */
        r->slot = 0;
    }
    return rc;
}

/* ---- FLAT-D4: find dopus5.library among the resident libraries (never load it), gate it, swap the two entries ---- */
static void dopus_unpatch(flatshared *r)
{
    int el;
    Forbid();
    for (el = 0; el < FLAT_EL_COUNT; el++)
        if (r->dsw[el].installed)
            flat_swap_remove(&r->dsw[el], r->dslot[el], &r->st);
    if (r->dlib)
        SumLibrary(r->dlib);
    CacheClearU();
    Permit();
    if (!r->dsw[0].installed && !r->dsw[1].installed && r->dlib) {
        Delay(2);                       /* a caller that already entered the replacement leaves it */
        CloseLibrary(r->dlib);
        r->dlib = 0;
        r->dmask = 0;
    }
}

static void dopus_scan(flatshared *r)
{
    struct ExecBase *eb = *(struct ExecBase **)4;
    struct Library *lib;
    uint32_t sums[FLAT_EL_COUNT];
    int n, el, mask;
    const flat_dopus_build *tab = flat_dopus_table(&n);
    if (r->dsw[0].installed || r->dsw[1].installed)
        return;                         /* already patched */
    Forbid();
    lib = (struct Library *)FindName(&eb->LibList, (STRPTR)"dopus5.library");
    if (lib)
        lib = OpenLibrary((STRPTR)"dopus5.library", 0);     /* resident: no disk load; keeps it from expunging */
    Permit();
    r->dfound = lib != 0;
    if (!lib)
        return;
    r->dver = lib->lib_Version;
    r->drev = lib->lib_Revision;
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        uint32_t t = flat_jmp_target((const unsigned char *)lib + d4_lvo[el]);
        sums[el] = t ? flat_sum_reloc((const void *)t, FLAT_FUNC_BYTES) : 0;
        r->dsum[el] = sums[el];
    }
    mask = flat_dopus_gate(tab, n, r->dver, r->drev, sums, &r->dreason);
    if (!mask) {
        CloseLibrary(lib);
        return;
    }
    g_r = r;
    Forbid();
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        unsigned char *entry = (unsigned char *)lib + d4_lvo[el];
        uint32_t t = flat_jmp_target(entry);
        if (!(mask & (1 << el)) || !t || flat_sum_reloc((const void *)t, FLAT_FUNC_BYTES) != sums[el]) {
            mask &= ~(1 << el);         /* changed since the gate (a foreign patch landed): leave it alone */
            continue;
        }
        flat_swap_init(&r->dsw[el]);
        r->dslot[el] = (uintptr_t *)(entry + 2);
        if (flat_swap_install(&r->dsw[el], r->dslot[el], (uintptr_t)(el == FLAT_EL_DRAWBOX ? (void *)d4_drawbox : (void *)d4_fieldbox)) != FLAT_OK)
            mask &= ~(1 << el);
    }
    SumLibrary(lib);
    CacheClearU();
    Permit();
    r->dmask = mask;
    if (mask)
        r->dlib = lib;
    else
        CloseLibrary(lib);
}

/* Take the record out of the list and free it; 0 when someone holds it (never pull it from under a user). */
static int drop_record(flatshared *r)
{
    int ok = 0;
    Forbid();
    if (AttemptSemaphore(&r->ss)) {
        RemSemaphore(&r->ss);
        ReleaseSemaphore(&r->ss);
        ok = 1;
    }
    Permit();
    if (ok)
        FreeVec(r);
    return ok;
}

int main(void)
{
    const char *args = (const char *)GetArgStr();
    flatshared *r;
    (void)ver;
    if (is_help(args)) {
        PutStr((STRPTR)"amarchy-flat [STATUS]: the flat-UI resident program; prints flat=on|off|absent|held. Ctrl-C unloads it\n");
        return 0;
    }
    if (is_status(args)) {
        print_status();
        return 0;
    }
    if (is_dopus(args)) {                               /* ask the running program to look for DOpus again */
        flatshared *q = find_record();
        if (q && q->task)
            Signal(q->task, SIGBREAKF_CTRL_D);
        else
            PutStr((STRPTR)"amarchy-flat: not running\n");
        return q ? 0 : 5;
    }
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r && task_alive(r->task, r->tname)) {
        Permit();
        PutStr((STRPTR)"amarchy-flat: already running\n");
        print_status();
        return 5;
    }
    if (r) {                                            /* a dead owner left its record: take it over, never leak it */
        r->task = 0;
        Permit();
        if (!drop_record(r)) {
            PutStr((STRPTR)"amarchy-flat: a stale record is held\n");
            return 5;
        }
        Forbid();
    }
    r = (flatshared *)AllocVec(sizeof(flatshared), MEMF_PUBLIC | MEMF_CLEAR);
    if (r) {
        struct Task *me = FindTask(0);
        int i;
        for (i = 0; FLAT_SEM_NAME[i] && i < (int)sizeof r->name - 1; i++)
            r->name[i] = FLAT_SEM_NAME[i];
        r->ss.ss_Link.ln_Name = r->name;
        flat_init(&r->st);
        r->task = me;
        if (me->tc_Node.ln_Name) {
            for (i = 0; me->tc_Node.ln_Name[i] && i < (int)sizeof r->tname - 1; i++)
                r->tname[i] = me->tc_Node.ln_Name[i];
        }
        AddSemaphore(&r->ss);
    }
    Permit();
    if (!r) {
        PutStr((STRPTR)"amarchy-flat: out of memory\n");
        return 10;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    if (IntuitionBase && GfxBase) {
        int rc = hook_install(r);
        if (rc != FLAT_OK)
            PutStr((STRPTR)"amarchy-flat: sysiclass hook not installed\n");
    }
    g_r = r;
    dopus_scan(r);
    print_status();
    for (;;) {
        ULONG got = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
        if (got & SIGBREAKF_CTRL_D) {
            dopus_scan(r);
            print_status();
        }
        if (!(got & SIGBREAKF_CTRL_C))
            continue;
        dopus_unpatch(r);
        if (r->dsw[0].installed || r->dsw[1].installed) {
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on a DOpus entry, not unloading\n");
            continue;
        }
        if (r->sw.installed && hook_remove(r) == FLAT_STILL_CHAINED) {
            PutStr((STRPTR)"amarchy-flat: a hook is still chained on top of ours, not unloading\n");
            continue;
        }
        if (IntuitionBase) {
            CloseLibrary((struct Library *)IntuitionBase);
            IntuitionBase = 0;
        }
        if (GfxBase) {
            CloseLibrary((struct Library *)GfxBase);
            GfxBase = 0;
        }
        if (drop_record(r))
            return 0;
        PutStr((STRPTR)"amarchy-flat: the state block is in use, not unloading\n");
    }
}
