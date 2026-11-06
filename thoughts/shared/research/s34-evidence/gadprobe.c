/* gadprobe [substr]: dump gadgets + PropInfo of Workbench-screen windows whose title contains substr */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <exec/resident.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
struct IntuitionBase *IntuitionBase;
long __stack = 16384;
static const char __ver[] = "$STACK:16384";
int main(int argc, char **argv)
{
    struct Screen *s; struct Window *w; ULONG lk; struct Resident *rt;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    if (!IntuitionBase) return 20;
    rt = FindResident("intuition.library");
    if (rt) printf("IROM lo=%08lx hi=%08lx ver=%ld\n", (ULONG)rt->rt_MatchTag, (ULONG)rt->rt_EndSkip, (long)rt->rt_Version);
    lk = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
    for (w = s->FirstWindow; w; w = w->NextWindow) {
        struct Gadget *g; int k = 0;
        if (argc > 1 && (!w->Title || !strstr((char *)w->Title, argv[1]))) continue;
        printf("WIN '%s' %d,%d %dx%d B l%d t%d r%d b%d flags=%08lx\n", w->Title ? (char *)w->Title : "", w->LeftEdge, w->TopEdge, w->Width, w->Height,
               w->BorderLeft, w->BorderTop, w->BorderRight, w->BorderBottom, w->Flags);
        for (g = w->FirstGadget; g; g = g->NextGadget, k++) {
            printf(" G%d @%08lx type=%04x flags=%04x act=%04x le=%d te=%d w=%d h=%d si=%08lx id=%d\n", k, (ULONG)g, g->GadgetType, g->Flags, g->Activation,
                   g->LeftEdge, g->TopEdge, g->Width, g->Height, (ULONG)g->SpecialInfo, g->GadgetID);
            if (g->SpecialInfo && ((g->GadgetType & 7) == 3 || (g->GadgetType & 7) == 5)) {
                struct PropInfo *pi = (struct PropInfo *)g->SpecialInfo;
                printf("   PI flags=%04x hp=%04x vp=%04x hb=%04x vb=%04x cw=%d ch=%d hpr=%d vpr=%d lb=%d tb=%d\n", pi->Flags, pi->HorizPot, pi->VertPot,
                       pi->HorizBody, pi->VertBody, pi->CWidth, pi->CHeight, pi->HPotRes, pi->VPotRes, pi->LeftBorder, pi->TopBorder);
            }
        }
    }
    UnlockIBase(lk);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
