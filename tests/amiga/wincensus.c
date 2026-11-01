/* wincensus: list every window on the Workbench screen under LockIBase: address, title, Flags, WFLG_WBENCHWINDOW,
 * owner task of the UserPort. Writes T:wincensus.txt and stdout. Census for the IPrefs 40.7 reset check
 * (it counts windows lacking WFLG_WBENCHWINDOW, 0x02000000). Also prints the public screen list (name, screen,
 * psn_VisitorCount, psn_Flags), each screen's Flags, and the jump target of intuition CloseWorkBench (LVO -78), which
 * IPrefs 40.7 SetFunctions to its window check (hunk 3 offset 0x2082). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdio.h>
#include "../../amarchy/wbreset.h"

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
    {
        wbr_pub *p;
        int i;
        Forbid();
        p = (wbr_pub *)FindSemaphore((STRPTR)WBR_SEM_NAME);
        if (p) {
            printf("WBR phase=%u busy=%u closes=%lu opens=%lu failed=%lu timeouts=%lu diag_vis=%d diag_wins=%d diag_ok=%ld\n",
                (unsigned)p->rec.phase, (unsigned)p->rec.busy, (unsigned long)p->rec.closes, (unsigned long)p->rec.opens,
                (unsigned long)p->rec.failed, (unsigned long)p->rec.timeouts, p->rec.diag_vis, p->rec.diag_wins, (long)p->rec.diag_ok);
            for (i = 0; i < WBR_MAX; i++)
                if (p->rec.c[i].task)
                    printf("WBR client %s mode=%u has_win=%u hidden=%u\n", ((struct Node *)p->rec.c[i].task)->ln_Name,
                        (unsigned)p->rec.c[i].mode, (unsigned)p->rec.c[i].has_win, (unsigned)p->rec.c[i].hidden);
        } else
            printf("WBR none\n");
        Permit();
    }
    {
        struct List *pl = LockPubScreenList();
        struct PubScreenNode *n;
        unsigned short *v = (unsigned short *)((char *)IntuitionBase - 78);
        unsigned long tgt = v[0] == 0x4ef9 ? ((unsigned long)v[1] << 16 | v[2]) : 0;
        printf("CWB vector %08lx op=%04x target=%08lx\n", (unsigned long)v, v[0], tgt);
        if (f)
            fprintf(f, "CWB vector %08lx op=%04x target=%08lx\n", (unsigned long)v, v[0], tgt);
        if (tgt) { /* first 12 words of the patch: IPrefs 40.7's stub starts 6100 e406 (bsr to its window check) */
            unsigned short *c = (unsigned short *)tgt;
            int i;
            printf("CWB code");
            for (i = 0; i < 12; i++)
                printf(" %04x", c[i]);
            printf("\n");
            if (f) {
                fprintf(f, "CWB code");
                for (i = 0; i < 12; i++)
                    fprintf(f, " %04x", c[i]);
                fprintf(f, "\n");
            }
        }
        for (n = (struct PubScreenNode *)pl->lh_Head; n->psn_Node.ln_Succ; n = (struct PubScreenNode *)n->psn_Node.ln_Succ) {
            printf("PUB name=%s screen=%08lx visitors=%d psnflags=%04x\n", n->psn_Node.ln_Name ? n->psn_Node.ln_Name : "-",
                   (unsigned long)n->psn_Screen, (int)n->psn_VisitorCount, (unsigned)n->psn_Flags);
            if (f)
                fprintf(f, "PUB name=%s screen=%08lx visitors=%d psnflags=%04x\n", n->psn_Node.ln_Name ? n->psn_Node.ln_Name : "-",
                        (unsigned long)n->psn_Screen, (int)n->psn_VisitorCount, (unsigned)n->psn_Flags);
        }
        UnlockPubScreenList();
    }
    lk = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen) {
        int wb = (s->Flags & SCREENTYPE) == WBENCHSCREEN;
        printf("SCREEN %08lx %s flags=%04x title=%s\n", (unsigned long)s, wb ? "WB" : "other", (unsigned)s->Flags, s->Title ? (char *)s->Title : "-");
        if (f)
            fprintf(f, "SCREEN %08lx %s flags=%04x title=%s\n", (unsigned long)s, wb ? "WB" : "other", (unsigned)s->Flags, s->Title ? (char *)s->Title : "-");
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
