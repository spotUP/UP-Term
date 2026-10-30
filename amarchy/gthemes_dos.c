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
#include "desk_dos.h"
#include "desk_core.h"
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

/* The theme's terminal colours into GT_TERM_FILE: every open console tab watches it and recolours (vtcon_handler.c). The
 * drawer ENV:up-term exists on any install that has run the console; made here when it does not. */
unsigned long gt_dos_term_writes;

void gt_dos_term_send(const char *name)
{
    static char text[1024];
    BPTR f;
    int n = gt_load_term(gt_read, 0, name, text, (int)sizeof text);
    if (n <= 0)
        return;
    f = Open((STRPTR)GT_TERM_FILE, MODE_NEWFILE);
    if (!f) {
        BPTR l = CreateDir((STRPTR) "ENV:up-term");
        if (l)
            UnLock(l);
        f = Open((STRPTR)GT_TERM_FILE, MODE_NEWFILE);
    }
    if (!f)
        return;
    Write(f, text, n);
    Close(f);
    gt_dos_term_writes++;
}

/* DESK-PREVIEW-PALETTE: the picker's highlight previews the Workbench colours (desk_preview, desk_core.h) straight on the Workbench
 * screen's ViewPort with SetRGB32, the pens desk_palette writes. The pens it overwrote are kept until the picker restores them. */
unsigned long gt_dos_desk_previews, gt_dos_desk_restores;
static desk_pv gt_pv;

static void pv_get(void *ctx, int pen, uc_u32 *rgb)
{
    ULONG t[3] = {0, 0, 0};
    struct Screen *scr = (struct Screen *)ctx;
    GetRGB32(scr->ViewPort.ColorMap, (ULONG)pen, 1, t);
    *rgb = ((t[0] >> 24) << 16) | ((t[1] >> 24) << 8) | (t[2] >> 24);
}

static void pv_set(void *ctx, int pen, uc_u32 rgb)
{
    struct Screen *scr = (struct Screen *)ctx;
    SetRGB32(&scr->ViewPort, (ULONG)pen, ((rgb >> 16) & 255) * 0x01010101UL, ((rgb >> 8) & 255) * 0x01010101UL,
             (rgb & 255) * 0x01010101UL);
}

/* th = 0 restores the saved pens */
static void desk_preview_theme(const gtheme *th)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    struct Screen *scr;
    if (IntuitionBase && GfxBase && (scr = LockPubScreen((STRPTR)"Workbench")) != 0) {
        int depth = scr->RastPort.BitMap->Depth;
        gt_pv.ctx = scr;
        gt_pv.get = pv_get;
        gt_pv.set = pv_set;
        gt_pv.npens = depth >= 3 ? 8 : 1 << depth;
        if (desk_preview(&gt_pv, th)) {
            if (th)
                gt_dos_desk_previews++;
            else
                gt_dos_desk_restores++;
        }
        UnlockPubScreen(0, scr);
    }
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
}

void gt_dos_desk_restore(void)
{
    if (gt_pv.have)
        desk_preview_theme(0);
}

void gt_dos_preview_send(const char *name)
{
    char cur[GT_NAME];
    gt_dos_term_send(name);
    gt_dos_current(cur, (int)sizeof cur);
    if (!strcmp(name, cur)) {           /* the saved theme again: the Workbench keeps (gets back) its own colours */
        gt_dos_desk_restore();
    } else {
        gtheme th;
        gt_dos_load(name, &th);
        desk_preview_theme(&th);
    }
}

void gt_dos_term_boot(void)
{
    am_prefs ap;
    amconf_load(&ap);
    gt_boot_term(ap.val[AM_GTHEME], gt_dos_term_send);
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
        int d;
        gt_dos_desk_restore();      /* the preview's colours go before the real apply */
        gt_dos_load(name, &th);
        gt_dos_term_send(name);
        gt_dos_dopus_sends++;
        dopus_theme_send(&th, gt_dopus_send, 0);
        /* the native desktop theme (desk_dos.h) sets pens 0-3 through the Palette editor; the SetRGB32 mapping is the fallback when it is off or refused */
        d = desk_dos_apply(&th, 0, 0);
        if (d <= 0 || !(d & (1 << DT_PALETTE)))
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
