/* See wbmode.h. */
#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "wbmode.h"

#define WBM_FILE "ENV:Sys/screenmode.prefs"

static wbm_prefs g_prefs;
static int g_have;

void wbm_begin(void)
{
    static unsigned char buf[128];
    BPTR fh = Open((STRPTR)WBM_FILE, MODE_OLDFILE);
    LONG n = 0;
    g_have = 0;
    if (fh) {
        n = Read(fh, buf, sizeof buf);
        Close(fh);
    }
    if (n > 0)
        g_have = wbm_parse_prefs(buf, n, &g_prefs);
}

/* the Workbench screen's mode ID, WBM_INVALID_ID when it cannot be read */
static unsigned long wb_mode_id(void)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    unsigned long id = WBM_INVALID_ID;
    struct Screen *scr;
    if (IntuitionBase && GfxBase && (scr = LockPubScreen((STRPTR)"Workbench")) != 0) {
        id = (unsigned long)GetVPModeID(&scr->ViewPort);
        UnlockPubScreen(0, scr);
    }
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return id;
}

int wbm_check(long elapsed_ticks)
{
    return wb_mode_ready(g_prefs.id, g_have, g_have ? wb_mode_id() : 0, elapsed_ticks, WBM_TIMEOUT_TICKS);
}
