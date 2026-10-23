/* scrprobe: the screens amarchy_gui_rig.py needs (G7, AMB3), and the font numbers it compares the menu with.
 *   scrprobe open NAME|- SECS      opens a 640x256 screen and holds it SECS seconds (Ctrl-C ends it): public under
 *                                  NAME, or private with "-" (a screen with no public name: the menu must fall back
 *                                  to the Workbench). Prints "open" once it is in front.
 *   scrprobe font NAME|- [text...] the public screen NAME (the default public screen for "-"): prints
 *                                  "screen W H ysize xsize proportional" and "len N text" for each text, the pixel
 *                                  width TextLength gives in that screen's font. */
#include <exec/types.h>
#include <dos/dos.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <stdlib.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static int do_open(const char *name, long secs)
{
    struct TagItem tags[10];
    struct Screen *s;
    int n = 0, i;
    int pub = strcmp(name, "-") != 0;
    tags[n].ti_Tag = SA_Width; tags[n++].ti_Data = 640;
    tags[n].ti_Tag = SA_Height; tags[n++].ti_Data = 256;
    tags[n].ti_Tag = SA_Depth; tags[n++].ti_Data = 3;
    tags[n].ti_Tag = SA_DisplayID; tags[n++].ti_Data = 0x00008000; /* HIRES */
    tags[n].ti_Tag = SA_Title; tags[n++].ti_Data = (ULONG)"G7 screen";
    tags[n].ti_Tag = SA_Type; tags[n++].ti_Data = pub ? PUBLICSCREEN : CUSTOMSCREEN;
    if (pub) {
        tags[n].ti_Tag = SA_PubName; tags[n++].ti_Data = (ULONG)name;
    }
    tags[n].ti_Tag = TAG_DONE;
    s = OpenScreenTagList(0, tags);
    if (!s) {
        Printf((STRPTR)"scrprobe: no screen\n");
        return 20;
    }
    if (pub)
        PubScreenStatus(s, 0);
    ScreenToFront(s);
    Printf((STRPTR)"open\n");
    Flush(Output());
    for (i = 0; i < secs * 5; i++) {
        Delay(10);
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
            break;
    }
    if (pub)
        PubScreenStatus(s, PSNF_PRIVATE);
    for (i = 0; i < 50 && !CloseScreen(s); i++)
        Delay(10);
    return 0;
}

static int do_font(const char *name, int argc, char **argv)
{
    struct Screen *s = LockPubScreen(strcmp(name, "-") ? (UBYTE *)name : 0);
    struct TextFont *f;
    int i;
    if (!s) {
        Printf((STRPTR)"scrprobe: no screen\n");
        return 20;
    }
    f = s->RastPort.Font;
    Printf((STRPTR)"screen %ld %ld %ld %ld %ld\n", (LONG)s->Width, (LONG)s->Height, (LONG)f->tf_YSize,
           (LONG)f->tf_XSize, (LONG)((f->tf_Flags & FPF_PROPORTIONAL) != 0));
    for (i = 0; i < argc; i++)
        Printf((STRPTR)"len %ld %s\n", (LONG)TextLength(&s->RastPort, (STRPTR)argv[i], (ULONG)strlen(argv[i])),
               argv[i]);
    UnlockPubScreen(0, s);
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 5;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    if (argc >= 4 && !strcmp(argv[1], "open"))
        rc = do_open(argv[2], atol(argv[3]));
    else if (argc >= 3 && !strcmp(argv[1], "font"))
        rc = do_font(argv[2], argc - 3, argv + 3);
    else
        Printf((STRPTR)"usage: scrprobe open NAME|- SECS | font NAME|- [text...]\n");
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
