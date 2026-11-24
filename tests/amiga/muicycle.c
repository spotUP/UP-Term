/* muicycle - MUI-REDIRECT-SPIKE fixture: a stock MUI app with two cycles, one from MUI_NewObject(MUIC_Cycle), one from
 * MUI_MakeObject(MUIO_Cycle). Prints which class each got (UP answers UPA_Cycle_Active, stock does not) and whether the
 * app's MUIA_Cycle_Active set/get/notify still work, then opens the window and prints PICK <cycle> <index> per change. */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <libraries/mui.h>

long __stack = 65536;
const char muicycle_stack_cookie[] = "$STACK: 65536";

#define UPA_Cycle_Active 0xD3410201UL

struct Library *MUIMasterBase;
struct IntuitionBase *IntuitionBase;

extern ULONG DoMethod(Object *obj, ULONG MethodID, ...);
extern Object *mui_newobj(__reg("a6") struct Library *, __reg("a0") char *, __reg("a1") ULONG *) = "\tjsr\t-30(a6)";
extern Object *mui_makeobj(__reg("a6") struct Library *, __reg("d0") LONG, __reg("a0") ULONG *) = "\tjsr\t-120(a6)";
extern void mui_disp(__reg("a6") struct Library *, __reg("a0") Object *) = "\tjsr\t-36(a6)";
void MUI_DisposeObject(Object *o) { mui_disp(MUIMasterBase, o); }
Object *MUI_NewObject(char *cls, ULONG tag1, ...) { return mui_newobj(MUIMasterBase, cls, &tag1); }
Object *MUI_MakeObject(LONG type, ULONG p1, ...) { return mui_makeobj(MUIMasterBase, type, &p1); }

static char *ents[] = {"First", "Second", "Third", NULL};
static ULONG seen[2];

static void probe(Object *c, int k)
{
    ULONG v = 99, u = 99, isup;
    isup = GetAttr(UPA_Cycle_Active, c, &u);
    seen[k] = 77;
    set(c, MUIA_Cycle_Active, 2);
    GetAttr(MUIA_Cycle_Active, c, &v);
    printf("C%d class=%s app-set=2 get=%lu notify=%lu\n", k, isup ? "UP" : "STOCK", v, seen[k]);
    if (isup) {               /* what UP-Cycle itself does on a click / key: set UPA_Cycle_Active */
        seen[k] = 77;
        set(c, UPA_Cycle_Active, 1);
        GetAttr(MUIA_Cycle_Active, c, &v);
        printf("C%d up-set=1 get=%lu notify=%lu\n", k, v, seen[k]);
    }
    fflush(stdout);
}

int main(void)
{
    Object *app, *win, *c[2];
    ULONG sigs = 0;
    int k;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase || !(MUIMasterBase = OpenLibrary((STRPTR)"muimaster.library", 19))) { puts("no muimaster"); return 20; }
    c[0] = CycleObject, MUIA_Cycle_Entries, ents, MUIA_Cycle_Active, 1, End;
    c[1] = MUI_MakeObject(MUIO_Cycle, (ULONG)"_Mode", ents);
    app = ApplicationObject, MUIA_Application_Title, "MuiCycle", MUIA_Application_Base, "MUICYCLE",
        SubWindow, win = WindowObject, MUIA_Window_Title, "MUI Cycle Redirect Test", MUIA_Window_ID, 1,
            WindowContents, VGroup, Child, c[0], Child, c[1], End,
        End,
    End;
    if (!c[0] || !c[1] || !app) { printf("c0 %lx c1 %lx app %lx\n", (ULONG)c[0], (ULONG)c[1], (ULONG)app); return 20; }
    for (k = 0; k < 2; k++) {
        DoMethod(c[k], MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, c[k], 3, MUIM_WriteLong, MUIV_TriggerValue, &seen[k]);
        DoMethod(c[k], MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, app, 2, MUIM_Application_ReturnID, 100 + k);
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    set(win, MUIA_Window_Open, TRUE);
    for (k = 0; k < 2; k++)
        probe(c[k], k);
    puts("READY");
    fflush(stdout);
    for (;;) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (id == MUIV_Application_ReturnID_Quit) break;
        if (id == 100 || id == 101) { printf("PICK %lu %lu\n", id - 100, seen[id - 100]); fflush(stdout); }
        if (sigs && Wait(sigs | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
