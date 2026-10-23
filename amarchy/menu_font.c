/* See menu_font.h. */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/text.h>
#include <graphics/displayinfo.h>
#include <diskfont/diskfont.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include "menu_font.h"
#include "../render/fontpair.h"

struct Library *DiskfontBase;

int menu_screen_aspect(struct Screen *scr, int *rx, int *ry)
{
    struct DisplayInfo di;
    ULONG mode = GetVPModeID(&scr->ViewPort);
    *rx = *ry = 0;
    if (mode == (ULONG)INVALID_ID || !GetDisplayInfoData(0, (UBYTE *)&di, sizeof(di), DTAG_DISP, mode))
        return 1;
    *rx = di.Resolution.x;
    *ry = di.Resolution.y;
    return di.Resolution.x == di.Resolution.y;
}

static struct TextFont *open_one(const fontpair_choice *ch)
{
    struct TextAttr ta;
    struct TextFont *f;
    ta.ta_Name = (STRPTR)ch->name;
    ta.ta_YSize = (UWORD)ch->size;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;
    f = OpenFont(&ta);
    if (!f || f->tf_YSize != ch->size) {
        struct Process *me = (struct Process *)FindTask(0);
        APTR old = me->pr_WindowPtr;
        if (f)
            CloseFont(f);
        f = 0;
        me->pr_WindowPtr = (APTR)-1;      /* a missing font is an answer, not a requester */
        DiskfontBase = OpenLibrary((STRPTR) "diskfont.library", 36);
        if (DiskfontBase) {
            f = OpenDiskFont(&ta);
            CloseLibrary(DiskfontBase);
            DiskfontBase = 0;
        }
        me->pr_WindowPtr = old;
    }
    if (f && f->tf_YSize == ch->size)
        return f;
    if (f)
        CloseFont(f);
    return 0;
}

struct TextFont *menu_font_open(struct Screen *scr, int square, int *opened)
{
    fontpair_choice ch[FONTPAIR_MENU_MAX];
    struct TextFont *f;
    int i, n = fontpair_menu_choices(square, fontpair_square_fits(scr->Height, scr->BarHeight), ch);
    *opened = 0;
    for (i = 0; i < n; i++) {
        f = open_one(&ch[i]);
        if (f) {
            *opened = 1;
            return f;
        }
    }
    return scr->RastPort.Font;
}
