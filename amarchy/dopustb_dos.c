/* dopustb_dos: see dopustb_dos.h. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "dopustb_dos.h"
#include "dopustb_core.h"
#include "menu_font.h"
#include "rexx_host.h"

#define DTD_DRAWER "DOpus5:Images/Amarchy/"

extern struct GfxBase *GfxBase;      /* each Amarchy program owns its own, opened or not: see dtd_screen */

/* The font height and the pixel aspect of the screen Directory Opus runs on; the aspect through menu_screen_aspect, the lookup the
 * menus use. The screen is the one `dopus screen` names (DOPUS.1 on its own screen, Workbench when DOpus opens there: chosen over
 * the lister's WScreen because that needs a lister, and over a fixed DOPUS.1 because that is absent on Workbench); an unanswered
 * command or an unlocked name falls back to the default public screen. 0 when Directory Opus has no port. */
static int dtd_screen(unsigned *font_h, int *rx, int *ry, void *ud)
{
    struct IntuitionBase *IntuitionBase;
    struct GfxBase *opened = 0, *saved = GfxBase;
    struct Screen *scr;
    char reply[128], name[64];
    int found = 0;
    (void)ud;
    *font_h = 0;
    *rx = *ry = 0;
    reply[0] = 0;
    if (rexx_send("DOPUS.1", "dopus screen", reply, sizeof reply) != 0)
        return 0;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!IntuitionBase)
        return 0;
    scr = dt_screen_name(reply, name, sizeof name) ? LockPubScreen((STRPTR)name) : 0;
    if (!scr)
        scr = LockPubScreen(0);
    if (scr) {
        if (scr->Font)
            *font_h = scr->Font->ta_YSize;
        if (!GfxBase) {
            opened = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
            GfxBase = opened;
        }
        if (GfxBase)
            menu_screen_aspect(scr, rx, ry);
        if (opened) {
            CloseLibrary((struct Library *)opened);
            GfxBase = saved;
        }
        UnlockPubScreen(0, scr);
        found = 1;
    }
    CloseLibrary((struct Library *)IntuitionBase);
    return found;
}

/* Rewrites the CMAP of one brush file when it differs. Returns 1 when the file was rewritten. */
static int dtd_patch_file(const char *path, const uint8_t rgb[DT_PENS][3])
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    struct FileInfoBlock *fib;
    uint8_t *buf = 0;
    int done = 0;
    if (!fh)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && ExamineFH(fh, fib) && fib->fib_Size > 0 && fib->fib_Size < 65536L &&
        (buf = (uint8_t *)AllocVec((ULONG)fib->fib_Size, MEMF_PUBLIC)) != 0 &&
        Read(fh, buf, fib->fib_Size) == fib->fib_Size && dt_cmap_patch(buf, (size_t)fib->fib_Size, rgb) == 1 &&
        Seek(fh, 0, OFFSET_BEGINNING) >= 0 && Write(fh, buf, fib->fib_Size) == fib->fib_Size)
        done = 1;
    if (buf)
        FreeVec(buf);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    Close(fh);
    return done;
}

/* Every brush of the set's drawer gets the theme's blend. Returns the number of files rewritten. */
static int dtd_recolour(const char *set, const uint8_t rgb[DT_PENS][3], void *ud)
{
    char dir[96], path[160];
    BPTR lock;
    struct FileInfoBlock *fib;
    int changed = 0;
    unsigned n, k;
    (void)ud;
    for (n = 0; DTD_DRAWER[n]; n++)
        dir[n] = DTD_DRAWER[n];
    for (k = 0; set[k] && n + 1 < sizeof dir; k++)
        dir[n++] = set[k];
    dir[n] = 0;
    lock = Lock((STRPTR)dir, ACCESS_READ);
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (lock && fib && Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            if (fib->fib_DirEntryType > 0 || strlen(dir) + 1 + strlen(fib->fib_FileName) >= sizeof path)
                continue;
            strcpy(path, dir);
            strcat(path, "/");
            strcat(path, fib->fib_FileName);
            changed += dtd_patch_file(path, rgb);
        }
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (lock)
        UnLock(lock);
    return changed;
}

const dopus_tb_ops dopustb_dos_ops = { dtd_screen, dtd_recolour, 0 };
