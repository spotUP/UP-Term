/* winbox TITLE WIDTH HEIGHT -- resize the first window whose title is
 * TITLE, as a program (not the user) does: for the rig's SIGWINCH test,
 * where no input.device size event comes with it. */
#include <stdlib.h>
#include <string.h>
#include <exec/execbase.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;

int main(int argc, char **argv)
{
    struct Screen *s;
    struct Window *w = 0;
    ULONG lock;
    if (argc < 4)
        return 20;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !w; s = s->NextScreen)
        for (w = s->FirstWindow; w; w = w->NextWindow)
            if (w->Title && !strcmp((char *)w->Title, argv[1]))
                break;
    UnlockIBase(lock);
    if (w)
        ChangeWindowBox(w, w->LeftEdge, w->TopEdge, (WORD)atol(argv[2]), (WORD)atol(argv[3]));
    CloseLibrary((struct Library *)IntuitionBase);
    return w ? 0 : 10;
}
