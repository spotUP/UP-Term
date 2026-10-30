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
 * Return code: 0 done, 5 another copy is running, 10 no memory. */
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
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
#include "../tty/qmark.h"
#include "flat_core.h"
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
    UWORD *tramp;               /* the 24-byte pass-through, MEMF_PUBLIC */
} flatshared;

struct IntuitionBase *IntuitionBase;

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
    int found = 0;
    memset(&copy, 0, sizeof copy);
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r) {
        found = 1;
        copy = r->st;
    }
    Permit();
    if (flat_status_line(&copy, sizeof copy, found, line, sizeof line)) {
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
    int rc;
    if (!slot)
        return -100;
    r->tramp = (UWORD *)AllocVec(FLAT_TRAMP_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!r->tramp)
        return -101;
    Forbid();
    /* the saved routine is read inside the same Forbid that writes the slot, so no change falls between the two */
    flat_tramp_build((unsigned char *)r->tramp, (ULONG)&r->st.calls, (ULONG)*slot);
    CacheClearU();
    rc = flat_swap_install(&r->sw, slot, (uintptr_t)r->tramp);
    Permit();
    if (rc != FLAT_OK) {
        FreeVec(r->tramp);
        r->tramp = 0;
        return rc;
    }
    r->slot = slot;
    return FLAT_OK;
}

/* Remove the swap if the slot is still ours (Forbid). The trampoline is freed only after a clean remove: while a
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
        Delay(2);                       /* a caller that already entered the trampoline finishes its tail jump */
        FreeVec(r->tramp);
        r->tramp = 0;
        r->slot = 0;
    }
    return rc;
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
    if (IntuitionBase) {
        int rc = hook_install(r);
        if (rc != FLAT_OK)
            PutStr((STRPTR)"amarchy-flat: sysiclass hook not installed\n");
    }
    print_status();
    for (;;) {
        Wait(SIGBREAKF_CTRL_C);
        if (r->sw.installed && hook_remove(r) == FLAT_STILL_CHAINED) {
            PutStr((STRPTR)"amarchy-flat: a hook is still chained on top of ours, not unloading\n");
            continue;
        }
        if (IntuitionBase) {
            CloseLibrary((struct Library *)IntuitionBase);
            IntuitionBase = 0;
        }
        if (drop_record(r))
            return 0;
        PutStr((STRPTR)"amarchy-flat: the state block is in use, not unloading\n");
    }
}
