/* frontscr -- see frontscr.h. Moved out of vtcon_handler.c unchanged so amarchy-gui locks the screen the
 * same way. */
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include "frontscr.h"

struct Screen *lock_front_screen(void)
{
    struct Screen *scr = 0, *front = IntuitionBase->FirstScreen;
    struct List *list = LockPubScreenList();
    struct Node *n;
    char name[MAXPUBSCREENNAME + 1];
    name[0] = 0;
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct PubScreenNode *ps = (struct PubScreenNode *)n;
        if (ps->psn_Screen == front && n->ln_Name) {
            const char *s = n->ln_Name;
            int i;
            for (i = 0; i < MAXPUBSCREENNAME && s[i]; i++)
                name[i] = s[i];
            name[i] = 0;
            break;
        }
    }
    UnlockPubScreenList();
    if (name[0])
        scr = LockPubScreen((UBYTE *)name);
    return scr;
}
