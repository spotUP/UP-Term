/* Tiny MUI program whose buttons are Group.mui objects with MUIA_InputMode (not Text.mui):
 * proves what the wasabid patch "list clicked MUI groups as buttons" changes in `wasabi ui list`.
 * Build note: see Makefile target build/amiga/muigroupbtn (needs MUI 3.8 libraries/mui.h from
 * Aminet dev/mui/mui38dev.lha in $(MUI_INC)). Run: `muigroupbtn` on a rig with MUI. */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <libraries/mui.h>

long __stack = 65536;
const char muigroupbtn_stack_cookie[] = "$STACK: 65536";

struct Library *MUIMasterBase;
struct IntuitionBase *IntuitionBase;
#ifdef __GNUC__
/* Bebbo gcc: libamiga has DoMethod; the muimaster calls are register-argument stubs here, so the MUI SDK's proto/inline
 * headers are not needed (only libraries/mui.h from $(MUI_INC)). */
#include <clib/alib_protos.h>
static Object *MUI_NewObject(const char *cls, ULONG tag1, ...)
{
    register Object *_r __asm("d0");
    register struct Library *_a6 __asm("a6") = MUIMasterBase;
    register const char *_a0 __asm("a0") = cls;
    register ULONG *_a1 __asm("a1") = &tag1;
    __asm volatile("jsr -30(%%a6)" : "=r"(_r) : "r"(_a6), "r"(_a0), "r"(_a1) : "d1", "memory", "cc");
    return _r;
}
static Object *MUI_MakeObject(LONG type, ...)
{
    register Object *_r __asm("d0");
    register struct Library *_a6 __asm("a6") = MUIMasterBase;
    register LONG _d0 __asm("d0") = type;
    register ULONG *_a0 __asm("a0") = (ULONG *)(&type + 1);
    __asm volatile("jsr -120(%%a6)" : "+r"(_d0) : "r"(_a6), "r"(_a0) : "a1", "d1", "memory", "cc");
    _r = (Object *)_d0;
    return _r;
}
static void MUI_DisposeObject(Object *o)
{
    register struct Library *_a6 __asm("a6") = MUIMasterBase;
    register Object *_a0 __asm("a0") = o;
    __asm volatile("jsr -36(%%a6)" : : "r"(_a6), "r"(_a0) : "d0", "d1", "a1", "memory", "cc");
}
#else
extern ULONG DoMethod(Object *obj, ULONG MethodID, ...);

/* MUI_NewObject / MUI_MakeObject without the SDK's proto stubs: vbcc passes varargs on the
 * stack, so the address of the first tag is the tag list. */
extern Object *mui_newobj(__reg("a6") struct Library *, __reg("a0") char *, __reg("a1") ULONG *) = "\tjsr\t-30(a6)";
extern Object *mui_makeobj(__reg("a6") struct Library *, __reg("d0") LONG, __reg("a0") ULONG *) = "\tjsr\t-120(a6)";
extern void mui_disp(__reg("a6") struct Library *, __reg("a0") Object *) = "\tjsr\t-36(a6)";
void MUI_DisposeObject(Object *o) { mui_disp(MUIMasterBase, o); }
Object *MUI_NewObject(char *cls, ULONG tag1, ...) { return mui_newobj(MUIMasterBase, cls, &tag1); }
Object *MUI_MakeObject(LONG type, ...) { return mui_makeobj(MUIMasterBase, type, (ULONG *)(&type + 1)); }

#endif

int main(void)
{
    Object *app, *win, *rv, *tg, *plain;
    ULONG sigs = 0;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    if (!IntuitionBase || !(MUIMasterBase = OpenLibrary("muimaster.library", 19))) { puts("no muimaster"); return 20; }

    rv = HGroup, ButtonFrame, MUIA_InputMode, MUIV_InputMode_RelVerify, MUIA_Background, MUII_ButtonBack,
        Child, ImageObject, MUIA_Image_Spec, MUII_ArrowRight, MUIA_FixWidth, 8, MUIA_FixHeight, 8, End,
        Child, TextObject, MUIA_Text_Contents, "Group Button", End,
    End;
    tg = VGroup, ButtonFrame, MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Background, MUII_ButtonBack,
        Child, TextObject, MUIA_Text_Contents, "Group Toggle", End,
    End;
    plain = SimpleButton("Plain Button");

    app = ApplicationObject, MUIA_Application_Title, "MuiGroupBtn", MUIA_Application_Base, "MUIGROUPBTN",
        MUIA_Application_Version, "$VER: muigroupbtn 1.0", MUIA_Application_Description, "Group button fixture",
        SubWindow, win = WindowObject, MUIA_Window_Title, "MUI Group Button Test", MUIA_Window_ID, 1,
            WindowContents, VGroup, Child, rv, Child, tg, Child, plain, End,
        End,
    End;
    if (!rv || !tg || !plain || !app) printf("rv %lx tg %lx plain %lx app %lx\n", (ULONG)rv, (ULONG)tg, (ULONG)plain, (ULONG)app);
    if (!app) { puts("no app"); CloseLibrary(MUIMasterBase); return 20; }

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(rv, MUIM_Notify, MUIA_Pressed, FALSE, app, 2, MUIM_Application_ReturnID, 100);
    set(win, MUIA_Window_Open, TRUE);
    puts("READY");
    for (;;) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (id == MUIV_Application_ReturnID_Quit) break;
        if (id == 100) { puts("GROUP PRESSED"); fflush(stdout); }
        if (sigs && Wait(sigs | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
