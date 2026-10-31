/* wincensus: list every window on the Workbench screen under LockIBase: address, title, Flags, WFLG_WBENCHWINDOW,
 * owner task of the UserPort. Writes T:wincensus.txt and stdout. Census for the IPrefs 40.7 reset check
 * (it counts windows lacking WFLG_WBENCHWINDOW, 0x02000000). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdio.h>

struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;
long __stack = 65536;
const char __stackcookie[] = "$STACK:65536";

int main(void)
{
    struct Screen *s;
    struct Window *w;
    ULONG lk;
    FILE *f = fopen("T:wincensus.txt", "w");
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    lk = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen) {
        int wb = (s->Flags & SCREENTYPE) == WBENCHSCREEN;
        printf("SCREEN %08lx %s title=%s\n", (unsigned long)s, wb ? "WB" : "other", s->Title ? (char *)s->Title : "-");
        if (f)
            fprintf(f, "SCREEN %08lx %s title=%s\n", (unsigned long)s, wb ? "WB" : "other", s->Title ? (char *)s->Title : "-");
        for (w = s->FirstWindow; w; w = w->NextWindow) {
            struct Task *t = w->UserPort ? (struct Task *)w->UserPort->mp_SigTask : 0;
            const char *tn = t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "none";
            const char *ti = w->Title ? (const char *)w->Title : "(null)";
            int yes = (w->Flags & 0x02000000) != 0;
            printf("WIN %08lx flags=%08lx WB=%s task=%s title=%s\n", (unsigned long)w, (unsigned long)w->Flags, yes ? "yes" : "NO", tn, ti);
            if (f)
                fprintf(f, "WIN %08lx flags=%08lx WB=%s task=%s title=%s\n", (unsigned long)w, (unsigned long)w->Flags, yes ? "yes" : "NO", tn, ti);
        }
    }
    UnlockIBase(lk);
    if (f)
        fclose(f);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
