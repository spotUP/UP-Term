/* resetprobe FILE [EDITOR]: run SYS:Prefs/EDITOR FROM FILE USE (default Font), then sample every 20 ms for 4 s what the
 * Workbench reset changes: screen address, title, screen Font + YSize, RastPort font, BarHeight, IPrefs task state.
 * Prints a line only when a sample differs from the previous one (DESK-31: the completion signal of a 3.1 reset). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <dos/dostags.h>

struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;
long __stack = 262144;
const char __stackcookie[] = "$STACK:262144";

int main(int argc, char **argv)
{
    int i;
    char cmd[120], line[200], prev[200];
    BPTR in, out;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!IntuitionBase || argc < 2)
        return 20;
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    strcpy(cmd, "SYS:Prefs/");
    strcat(cmd, argc > 2 ? argv[2] : "Font");
    strcat(cmd, " FROM ");
    strcat(cmd, argv[1]);
    strcat(cmd, " USE");
    prev[0] = 0;
    SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, TAG_DONE);
    if (in) Close(in);
    if (out) Close(out);
    for (i = 0; i < 200; i++) {
        ULONG lock = LockIBase(0);
        struct Screen *s = IntuitionBase->FirstScreen;
        struct Task *ip;
        int st;
        LONG ysize = -1, bar = -1;
        char *name = "-";
        APTR rpf = 0, sf = 0;
        if (s) {
            sf = s->Font;
            ysize = s->Font ? s->Font->ta_YSize : -1;
            rpf = s->RastPort.Font;
            bar = s->BarHeight;
            name = s->Title ? (char *)s->Title : "-";
        }
        Forbid();
        ip = FindTask((STRPTR)"IPrefs");
        st = ip ? ip->tc_State : -1;
        Permit();
        {
            char t[16];
            strncpy(t, name, 15); t[15] = 0;
            /* title of the screen is copied under the lock; the line is built after */
            UnlockIBase(lock);
            {
                extern int sprintf(char *, const char *, ...);
                sprintf(line, "S=%lx sf=%lx ys=%ld rpf=%lx bar=%ld ip=%d '%s'", (long)s, (long)sf, (long)ysize, (long)rpf, (long)bar, st, t);
            }
        }
        if (strcmp(line, prev)) {
            Printf((STRPTR)"%3ld0ms %s\n", (LONG)(i * 2), (STRPTR)line);
            strcpy(prev, line);
        }
        Delay(1);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
