/* gthemes_dos: see gthemes_dos.h. */
#include <string.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/libraries.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "gthemes_dos.h"
#include "amconf_dos.h"
#include "../prefs/prefs_dos.h"
#include "menu_theme.h"
#include "rexx_host.h"

unsigned long gt_dos_dopus_sends;
unsigned long gt_dos_wb_pens, gt_dos_wb_refused;

static long gt_dopus_send(const char *port, const char *line, char *result, int cap, void *ud)
{
    (void)ud;
    return rexx_send(port, line, result, cap);
}

/* Workbench pens 0-3 become the theme's bg/fg/dim/accent on the Workbench screen's ViewPort, live (the pattern of
 * render/amiga_render.c: SetRGB32 with the 8-bit channel replicated to 32 bits). Refused, and counted, when pen 0
 * against pen 1 is weak (wb_pens_plan). The library bases are local: this file is linked into four programs that
 * open them (or not) under different names. */
static void wb_pens_apply(const gtheme *th)
{
    uc_u32 rgb[WB_PENS];
    struct IntuitionBase *IntuitionBase;
    struct GfxBase *GfxBase;
    struct Screen *scr;
    int i;
    if (!wb_pens_plan(th, rgb)) {
        gt_dos_wb_refused++;
        return;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (IntuitionBase && GfxBase && (scr = LockPubScreen((STRPTR)"Workbench")) != 0) {
        for (i = 0; i < WB_PENS; i++)
            SetRGB32(&scr->ViewPort, (ULONG)i, ((rgb[i] >> 16) & 255) * 0x01010101UL,
                     ((rgb[i] >> 8) & 255) * 0x01010101UL, (rgb[i] & 255) * 0x01010101UL);
        UnlockPubScreen(0, scr);
        gt_dos_wb_pens++;
    }
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
}

static long gt_read(void *ctx, const char *path, char *buf, long cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    LONG got;
    (void)ctx;
    if (!f)
        return -1;
    got = Read(f, buf, cap);
    Close(f);
    return got < 0 ? 0 : got;
}

void gt_dos_load(const char *name, gtheme *out)
{
    gt_load(gt_read, 0, name, out);
}

void gt_dos_current(char *out, int cap)
{
    am_prefs ap;
    const char *v;
    amconf_load(&ap);
    v = ap.val[AM_GTHEME];
    if (!*v || (int)strlen(v) >= cap)
        v = GT_DEFAULT;
    strcpy(out, v);
}

void gt_dos_load_current(gtheme *out)
{
    char name[GT_NAME];
    gt_dos_current(name, sizeof name);
    gt_dos_load(name, out);
}

int gt_dos_select(const char *name, char *why, int cap)
{
    am_prefs ap;
    int failed = 0, r;
    if (!gt_name_ok(name)) {
        if (why)
            gt_save_text(GT_SAVE_NAME, 0, 0, why, cap);
        return 0;
    }
    amconf_load(&ap);
    strcpy(ap.val[AM_GTHEME], name);
    r = amconf_save(&ap, 1, &failed);
    if (r == PREFS_INSTALL_OK) {
        gtheme th;
        gt_dos_load(name, &th);
        gt_dos_dopus_sends++;
        dopus_theme_send(&th, gt_dopus_send, 0);
        wb_pens_apply(&th);
        return 1;
    }
    if (why)
        gt_save_text(r, failed, prefs_dos_last_error(), why, cap);
    return 0;
}

void gt_dos_load_rows(ix_index *x)
{
    static gt_list l;
    static char lab[GT_MAX][GT_NAME + 16];
    const char *labels[GT_MAX], *names[GT_MAX];
    char cur[GT_NAME];
    int i;
    gt_dos_scan(&l);
    gt_dos_current(cur, sizeof cur);
    for (i = 0; i < l.n; i++) {
        gt_label(l.name[i], cur, lab[i], (int)sizeof lab[i]);
        labels[i] = lab[i];
        names[i] = l.name[i];
    }
    ix_add_gthemes(x, labels, names, l.n);
}
