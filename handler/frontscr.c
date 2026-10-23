/* frontscr -- see frontscr.h. Moved out of vtcon_handler.c unchanged so amarchy-gui locks the screen the
 * same way. */
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include "frontscr.h"

int front_screen_name(char *out, int cap)
{
    struct Screen *front;
    struct List *list;
    struct Node *n;
    int found = 0;
    out[0] = 0;
    if (!IntuitionBase)
        return 0;
    front = IntuitionBase->FirstScreen;
    list = LockPubScreenList();
    for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct PubScreenNode *ps = (struct PubScreenNode *)n;
        if (ps->psn_Screen == front && n->ln_Name) {
            const char *s = n->ln_Name;
            int i;
            for (i = 0; i < cap - 1 && s[i]; i++)
                out[i] = s[i];
            out[i] = 0;
            found = out[0] != 0;
            break;
        }
    }
    UnlockPubScreenList();
    return found;
}

struct Screen *lock_front_screen(void)
{
    char name[MAXPUBSCREENNAME + 1];
    if (!front_screen_name(name, sizeof name))
        return 0;
    return LockPubScreen((UBYTE *)name);
}
