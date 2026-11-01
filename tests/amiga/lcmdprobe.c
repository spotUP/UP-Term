/* lcmdprobe: a stand-in third-party application for the Amarchy clash rig (tools/rig/amarchy_clash_rig.py).
 * Opens a window "lcmdprobe", and for every IDCMP_RAWKEY it receives appends "raw=<hex> qual=<hex>" to
 * RAM:lcmdprobe.log. Ends when the window is closed or after 60 s. A Left Amiga chord that Amarchy does not
 * own reaches it with IEQUALIFIER_LCOMMAND (0x0040) set; one the commodity swallowed never appears. */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

struct IntuitionBase *IntuitionBase;

/* open, write, close per line: the rig reads the log while the probe runs */
static void logline(const char *mode, const char *text)
{
    FILE *f = fopen("RAM:lcmdprobe.log", mode);
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

int main(void)
{
    struct Window *w;
    struct IntuiMessage *m;
    int t = 0;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    w = OpenWindowTags(0, WA_Left, 10, WA_Top, 20, WA_Width, 300, WA_Height, 60, WA_Title, (ULONG)"lcmdprobe",
                       WA_Activate, TRUE, WA_DragBar, TRUE, WA_CloseGadget, TRUE,
                       WA_IDCMP, IDCMP_RAWKEY | IDCMP_CLOSEWINDOW, TAG_DONE);
    if (!w)
        return 20;
    logline("w", "ready\n");
    while (t < 600) {
        int closed = 0;
        while ((m = (struct IntuiMessage *)GetMsg(w->UserPort)) != 0) {
            if (m->Class == IDCMP_CLOSEWINDOW)
                closed = 1;
            else if (m->Class == IDCMP_RAWKEY) {
                char line[32];
                sprintf(line, "raw=%02x qual=%04x\n", (unsigned)m->Code, (unsigned)m->Qualifier);
                logline("a", line);
            }
            ReplyMsg((struct Message *)m);
        }
        if (closed)
            break;
        Delay(5);
        t++;
    }
    CloseWindow(w);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
