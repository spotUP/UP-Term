/* muiredir - MUI-REDIRECT-SPIKE (2026-11-24, thoughts/shared/mui-flat-plan.md "Owner decision"): while running, a stock
 * MUI app that asks muimaster for "Cycle.mui" gets UP-Cycle.mcc instead, but only while patina-flat says flat is on
 * ("Patina.Flat" semaphore, st.enabled and not held). Ctrl-C restores the vector and exits.
 *
 * A name rewrite alone is not enough: UP-Cycle reads UPA_Cycle_* tags, not MUIA_Cycle_*. So the redirect creates a
 * private subclass of UP-Cycle.mcc that aliases the stock attributes both ways (OM_NEW/OM_SET/OM_GET), so the app's
 * MUIA_Cycle_Active set/get/notify keep working and UP's own UPA_Cycle_Active changes also fire MUIA_Cycle_Active.
 *
 * shortcut: plain SetFunction, no in-flight gate; the production patch goes through flat_swap + gate_make like
 * flat.c cg_install, and objects made from the subclass pin this program in memory (spike: never exit while a
 * redirected app runs). */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <exec/semaphores.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <libraries/mui.h>
#include "../../render/flat_shared.h"

long __stack = 16384;
const char muiredir_stack_cookie[] = "$STACK: 16384";

#define UPA_Cycle_Active 0xD3410201UL  /* UP_TAG(UP_CLS_CYCLE, 1), MusicPlayer mcc/up.h */
#define UPA_Cycle_Entries 0xD3410202UL /* UP_TAG(UP_CLS_CYCLE, 2) */
#define LVO_NEWOBJECTA (-30)

struct Library *MUIMasterBase;
struct Library *UtilityBase;
struct IntuitionBase *IntuitionBase;
static struct MUI_CustomClass *upc;
static volatile ULONG n_redir, n_pass;

extern ULONG DoSuperMethodA(struct IClass *, Object *, Msg);
extern struct MUI_CustomClass *mui_ccc(__reg("a6") struct Library *, __reg("a0") struct Library *, __reg("a1") char *,
                                       __reg("a2") struct MUI_CustomClass *, __reg("d0") LONG, __reg("a3") APTR) = "\tjsr\t-108(a6)";
extern void mui_dcc(__reg("a6") struct Library *, __reg("a0") struct MUI_CustomClass *) = "\tjsr\t-114(a6)";

typedef APTR (*newfn)(__reg("a0") char *, __reg("a1") struct TagItem *, __reg("a6") struct Library *);
static newfn orig_new;

static int flat_on(void)
{
    struct SignalSemaphore *s;
    int on = 0;
    Forbid();
    s = FindSemaphore((STRPTR)"Patina.Flat");
    if (s) {
        const flatshared *r = (const flatshared *)s;
        on = r->st.enabled && !r->st.held;
    }
    Permit();
    return on;
}

/* The alias: one tag pair (the missing twin of whichever Active tag is present), then TAG_MORE the caller's list. */
static ULONG alias_set(struct IClass *cl, Object *obj, struct opSet *m, ULONG method)
{
    struct TagItem t[4], *mu, *up;
    struct opSet copy = *m;
    int k = 0;
    mu = FindTagItem(MUIA_Cycle_Active, m->ops_AttrList);
    up = FindTagItem(UPA_Cycle_Active, m->ops_AttrList);
    if (mu && !up) {
        t[k].ti_Tag = UPA_Cycle_Active, t[k++].ti_Data = mu->ti_Data;
    } else if (up && !mu) {
        t[k].ti_Tag = MUIA_Cycle_Active, t[k++].ti_Data = up->ti_Data;
    }
    if (method == OM_NEW && (mu = FindTagItem(MUIA_Cycle_Entries, m->ops_AttrList)) && !FindTagItem(UPA_Cycle_Entries, m->ops_AttrList))
        t[k].ti_Tag = UPA_Cycle_Entries, t[k++].ti_Data = mu->ti_Data;
    if (!k)
        return DoSuperMethodA(cl, obj, (Msg)m);
    t[k].ti_Tag = TAG_MORE, t[k].ti_Data = (ULONG)m->ops_AttrList;
    copy.ops_AttrList = t;
    return DoSuperMethodA(cl, obj, (Msg)&copy);
}

static ULONG disp(__reg("a0") struct IClass *cl, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (msg->MethodID == OM_NEW || msg->MethodID == OM_SET)
        return alias_set(cl, obj, (struct opSet *)msg, msg->MethodID);
    if (msg->MethodID == OM_GET && ((struct opGet *)msg)->opg_AttrID == MUIA_Cycle_Active) {
        struct opGet g = *(struct opGet *)msg;
        g.opg_AttrID = UPA_Cycle_Active;
        return DoSuperMethodA(cl, obj, (Msg)&g);
    }
    return DoSuperMethodA(cl, obj, msg);
}

static APTR new_patch(__reg("a0") char *cls, __reg("a1") struct TagItem *tags, __reg("a6") struct Library *base)
{
    if (cls && upc && !strcmp(cls, MUIC_Cycle) && flat_on()) {
        n_redir++;
        return NewObjectA(upc->mcc_Class, NULL, tags);
    }
    n_pass++;
    return orig_new(cls, tags, base);
}

int main(void)
{
    APTR old;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    UtilityBase = OpenLibrary((STRPTR)"utility.library", 37);
    MUIMasterBase = OpenLibrary((STRPTR)"muimaster.library", 19);
    if (!IntuitionBase || !UtilityBase || !MUIMasterBase) { puts("[ERROR] libraries"); return 20; }
    upc = mui_ccc(MUIMasterBase, NULL, (char *)"UP-Cycle.mcc", NULL, 0, (APTR)disp);
    if (!upc) { puts("[ERROR] UP-Cycle.mcc did not open (LIBS:mui/)"); CloseLibrary(MUIMasterBase); return 20; }
    Forbid();
    orig_new = (newfn)SetFunction(MUIMasterBase, LVO_NEWOBJECTA, (APTR)new_patch);
    Permit();
    printf("[OK] muiredir: Cycle.mui -> UP-Cycle.mcc while flat on (now flat=%s); Ctrl-C to stop\n", flat_on() ? "on" : "off");
    fflush(stdout);
    Wait(SIGBREAKF_CTRL_C);
    Forbid();
    old = SetFunction(MUIMasterBase, LVO_NEWOBJECTA, (APTR)orig_new);
    if (old != (APTR)new_patch) {           /* someone chained on top: leave both in place, never free the subclass */
        SetFunction(MUIMasterBase, LVO_NEWOBJECTA, old);
        Permit();
        puts("[WARN] chained over; staying resident");
        Wait(0);
    }
    Permit();
    printf("[OK] muiredir: removed, redirected %lu, passed %lu\n", n_redir, n_pass);
    mui_dcc(MUIMasterBase, upc);
    CloseLibrary(MUIMasterBase);
    CloseLibrary(UtilityBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
