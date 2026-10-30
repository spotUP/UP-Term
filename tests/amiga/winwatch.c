/* winwatch SECS: every tick for SECS seconds, sample the Workbench screen (address, title) and every window on it (title,
 * owner task of its UserPort). On each change, Lock() a name that encodes the sample ("T:WW|<scr>|<title>|<win>@<task>...")
 * so a wasabi snoop of the same moment puts the change in one timeline with the DOS calls of IPrefs and the editors
 * (DESK-31: the event order of a 3.1 desk apply). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <stdlib.h>

struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;
long __stack = 65536;
const char __stackcookie[] = "$STACK:65536";

static void add(char *out, int cap, const char *s)
{
    int n = strlen(out);
    while (*s && n < cap - 1) {
        char c = *s++;
        out[n++] = (c == ':' || c == '/' || c < 32) ? '_' : c;
    }
    out[n] = 0;
}

static void sample(char *out, int cap)
{
    struct Screen *s;
    struct Window *w;
    ULONG lk = LockIBase(0);
    out[0] = 0;
    strcpy(out, "T:WW");
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        if ((s->Flags & SCREENTYPE) == WBENCHSCREEN)
            break;
    if (!s) {
        add(out, cap, "|noscr");
    } else {
        char a[12];
        int i;
        unsigned long v = (unsigned long)s;
        for (i = 7; i >= 0; i--, v >>= 4)
            a[i] = "0123456789abcdef"[v & 15];
        a[8] = 0;
        add(out, cap, "|");
        add(out, cap, a);
        add(out, cap, "|");
        add(out, cap, s->Title ? (char *)s->Title : "-");
        for (w = s->FirstWindow; w; w = w->NextWindow) {
            struct Task *t = w->UserPort ? (struct Task *)w->UserPort->mp_SigTask : 0;
            add(out, cap, "|");
            add(out, cap, w->Title ? (char *)w->Title : "''");
            add(out, cap, "@");
            add(out, cap, t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?");
        }
    }
    UnlockIBase(lk);
}

int main(int argc, char **argv)
{
    static char now[400], prev[400];
    int i, n;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!IntuitionBase || argc < 2)
        return 20;
    n = atoi(argv[1]) * 50;
    prev[0] = 0;
    for (i = 0; i < n; i++) {
        sample(now, 240);
        if (strcmp(now, prev) != 0) {
            BPTR l = Lock((STRPTR)now, SHARED_LOCK);
            if (l)
                UnLock(l);
            strcpy(prev, now);
        }
        Delay(1);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
