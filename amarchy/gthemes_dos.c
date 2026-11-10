/* gthemes_dos: see gthemes_dos.h. */
#include "../prefs/prefs_asl.h"
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
#include "iconpen_dos.h"
#include "desk_core.h"
#include "amconf_dos.h"
#include "../prefs/prefs_dos.h"
#include "menu_theme.h"
#include "dopustb_dos.h"
#include "rexx_host.h"
#include "wbreset_core.h"

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
    if (IntuitionBase && GfxBase && (scr = ipd_lock_wb_screen()) != 0) {
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

/* The theme's terminal colours into GT_TERM_FILE: every open console tab watches it and recolours (vtcon_handler.c). The
 * drawer ENV:up-term exists on any install that has run the console; made here when it does not. */
unsigned long gt_dos_term_writes;

/* The console tabs' file: GT_TERM_FILE gets text (n bytes) */
static void gt_dos_term_put(const char *text, int n)
{
    BPTR f = Open((STRPTR)GT_TERM_FILE, MODE_NEWFILE);
    if (!f) {
        BPTR l = CreateDir((STRPTR) "ENV:up-term");
        if (l)
            UnLock(l);
        f = Open((STRPTR)GT_TERM_FILE, MODE_NEWFILE);
    }
    if (!f)
        return;
    Write(f, (APTR)text, n);
    Close(f);
    gt_dos_term_writes++;
}

void gt_dos_term_send(const char *name)
{
    static char text[1024];
    int n = gt_load_term(gt_dos_read, 0, name, text, (int)sizeof text);
    if (n > 0)
        gt_dos_term_put(text, n);
}

/* DESK-PREVIEW-PALETTE: the picker's highlight previews the Workbench colours (desk_preview, desk_core.h) straight on the Workbench
 * screen's ViewPort with SetRGB32, the pens desk_palette writes. The pens it overwrote are kept until the picker restores them. */
unsigned long gt_dos_desk_previews, gt_dos_desk_restores;
static desk_pv gt_pv;

/* The callbacks get the graphics base through ctx: the bases in this file are local (see wb_pens_apply). */
typedef struct {
    struct Screen *scr;
    struct GfxBase *gfx;
} pv_ctx;

static void pv_get(void *ctx, int pen, uc_u32 *rgb)
{
    ULONG t[3] = {0, 0, 0};
    struct Screen *scr = ((pv_ctx *)ctx)->scr;
    struct GfxBase *GfxBase = ((pv_ctx *)ctx)->gfx;
    GetRGB32(scr->ViewPort.ColorMap, (ULONG)pen, 1, t);
    *rgb = ((t[0] >> 24) << 16) | ((t[1] >> 24) << 8) | (t[2] >> 24);
}

static void pv_set(void *ctx, int pen, uc_u32 rgb)
{
    struct Screen *scr = ((pv_ctx *)ctx)->scr;
    struct GfxBase *GfxBase = ((pv_ctx *)ctx)->gfx;
    SetRGB32(&scr->ViewPort, (ULONG)pen, ((rgb >> 16) & 255) * 0x01010101UL, ((rgb >> 8) & 255) * 0x01010101UL,
             (rgb & 255) * 0x01010101UL);
}

/* th = 0 restores the saved pens */
static void desk_preview_theme(const gtheme *th)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    struct Screen *scr;
    pv_ctx c;
    if (IntuitionBase && GfxBase && (scr = ipd_lock_wb_screen()) != 0) {
        int depth = scr->RastPort.BitMap->Depth;
        c.scr = scr;
        c.gfx = GfxBase;
        gt_pv.ctx = &c;
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
    if (gt_pv.have) {
        desk_preview_theme(0);
        desk_dos_flat_sync();   /* TE-6.2: the flat look and colours of the saved theme go back too */
    }
}

/* TE-6.2: the term bridge text of an in-memory model, the Workbench pens and the flat state; the editor's edit hook */
void gt_dos_preview_model(const gt_model *m)
{
    static char buf[UC_MAX_FILE], text[1024];
    long n = gt_model_save_text(m, buf, (long)sizeof buf);
    int t;
    gtheme th;
    if (n > 0 && (t = gt_term_text(buf, n, m->name, text, (int)sizeof text)) > 0)
        gt_dos_term_put(text, t);
    gt_model_to_v1(m, &th);
    th.model = (gt_model *)m;
    desk_preview_theme(&th);
    desk_dos_flat_preview(&th);
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
    am_prefs *ap;
    ap = amconf_borrow();
    gt_boot_term(ap->val[AM_GTHEME], gt_dos_term_send);
    amconf_return(ap);
}

/* What follows the save of the chosen theme's name: the preview's colours go, then consoles, desktop pens, Directory Opus and the
 * held icon pens follow the file now on disk. Also what saving an edit of the current theme runs (gt_save_edit). */
static int gt_dos_apply_saved(const char *name)
{
    gtheme th;
    int d;
    gt_dos_desk_restore();      /* the preview's colours go before the real apply */
    gt_dos_load(name, &th);
    gt_dos_term_send(name);
    /* the native desktop theme (desk_dos.h) sets pens 0-3 through the Palette editor; the SetRGB32 mapping is the fallback when it is off or refused */
    d = desk_dos_apply(&th, 0, 0);
    if (d <= 0 || !(d & (1 << DT_PALETTE))) {
        /* desk_dos_apply hands the theme to DOpus itself after its Palette write (FLAT-D2); otherwise it goes here */
        gt_dos_dopus_sends++;
        dopus_theme_send(&th, gt_dopus_send, 0, &dopustb_dos_ops);
        wb_pens_apply(&th);
    }
    ipd_recolour();             /* ICON-AA-WRONG-BG: the held icon pens 32-47 follow the new theme live */
    return 1;
}

int gt_dos_select(const char *name, char *why, int cap)
{
    am_prefs *ap;
    int failed = 0, r;
    char prev[GT_NAME];
    if (!gt_name_ok(name)) {
        if (why)
            gt_save_text(GT_SAVE_NAME, 0, 0, why, cap);
        return 0;
    }
    ap = amconf_borrow();
    strncpy(prev, ap->val[AM_GTHEME], GT_NAME - 1);
    prev[GT_NAME - 1] = 0;
    strcpy(ap->val[AM_GTHEME], name);
    r = amconf_save(ap, 1, &failed);
    amconf_return(ap);                  /* the saved copy is not read again below */
    if (r == PREFS_INSTALL_OK) {
        desk_dos_prev_theme(prev);      /* FP1-REVERT-THEME-LINE: a Revert of this pick writes it back */
        gt_dos_apply_saved(name);
        return 1;
    }
    if (why)
        gt_save_text(r, failed, prefs_dos_last_error(), why, cap);
    return 0;
}

static void gt_dos_screens_load(void);

void gt_dos_load_rows(ix_index *x)
{
    static gt_list l;
    static char lab[GT_MAX][GT_NAME + 16];
    const char *labels[GT_MAX], *names[GT_MAX];
    char cur[GT_NAME];
    int i;
    gt_se_loader = gt_dos_screens_load;
    gt_se_stale();                      /* each menu open samples the open screens again; unsaved edits stay */
    gt_dos_scan(&l);
    gt_dos_current(cur, sizeof cur);
    for (i = 0; i < l.n; i++) {
        gt_label(l.name[i], cur, lab[i], (int)sizeof lab[i]);
        labels[i] = lab[i];
        names[i] = l.name[i];
    }
    ix_add_gthemes(x, labels, names, l.n);
}

/* ---- TE-10.8b: the Screens page ---- */
#define SCR_FILE_ENV "ENV:Amarchy/screen-themes"
#define SCR_FILE_ARC "ENVARC:Amarchy/screen-themes"
#define SCR_BUF 8192

static void gt_dos_drawers(void);

/* the page's loader (gt_se_loader): the rule file (ENV: copy first), the open public screens (ipd_pub_screens, the bar's sampler) and
 * the theme names (gt_dos_scan). Runs before the first row of the page shows, and again after each gt_se_stale. */
static void gt_dos_screens_load(void)
{
    static gt_list l;
    static char open[GT_SE_ROWS][GT_SCR_PAT];    /* a public screen name at full length (139 + terminator) */
    const char *op[GT_SE_ROWS], *tn[GT_MAX];
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    char *buf = (char *)AllocVec(SCR_BUF, MEMF_PUBLIC);
    long len = 0;
    int i, n;
    me->pr_WindowPtr = (APTR)-1;        /* a volume not there is an answer, not a requester */
    if (buf) {
        len = gt_dos_read(0, SCR_FILE_ENV, buf, SCR_BUF);
        if (len < 0)
            len = gt_dos_read(0, SCR_FILE_ARC, buf, SCR_BUF);
        if (len < 0)
            len = 0;
    }
    me->pr_WindowPtr = oldwin;
    gt_dos_scan(&l);
    for (i = 0; i < l.n; i++)
        tn[i] = l.name[i];
    gt_se_set_themes(tn, l.n);
    n = ipd_pub_screens(open[0], GT_SCR_PAT, GT_SE_ROWS, 0);
    for (i = 0; i < n; i++)
        op[i] = open[i];
    gt_se_load(buf ? buf : "", len, op, n);
    if (buf)
        FreeVec(buf);
}

/* Save Screen Themes: the rule file to ENVARC: and ENV:, the on/off key to match, then the pen holder is told to reread
 * (ipd_rules_changed sets rules_dirty; its next sync runs rules_read on ENV:Amarchy/screen-themes and AM_SCREENTHEMES). 1 saved. */
int gt_dos_screens_save(char *why, int cap)
{
    am_prefs *ap = 0;   /* borrowed only when the files were written; amconf_return takes 0 */
    char *buf = (char *)AllocVec(SCR_BUF, MEMF_PUBLIC);
    const char *want = gt_se_enabled() ? "on" : "off";
    long len;
    int ok = 0, failed = 0, r;
    if (!buf) {
        if (why)
            strncpy(why, "out of memory", (size_t)cap - 1);
        return 0;
    }
    gt_dos_drawers();
    len = gt_se_text(buf, SCR_BUF);
    if (len > 0 && prefs_dos_fs.write_file(0, SCR_FILE_ARC, buf, len) && prefs_dos_fs.write_file(0, SCR_FILE_ENV, buf, len)) {
        ok = 1;
        ap = amconf_borrow();
        if (strcmp(ap->val[AM_SCREENTHEMES], want) != 0) {
            strcpy(ap->val[AM_SCREENTHEMES], want);
            r = amconf_save(ap, 1, &failed);
            ok = r == PREFS_INSTALL_OK;
        }
    }
    if (ok)
        ipd_rules_changed();
    else if (why)
        strncpy(why, "cannot write the screen themes", (size_t)cap - 1);
    FreeVec(buf);
    amconf_return(ap);
    return ok;
}

/* TE-3 */
static void gt_dos_drawers(void)
{
    static const char *const dirs[] = { "ENVARC:Amarchy", GT_USER_DIR, "ENV:Amarchy", GT_ENV_DIR };
    int i;
    for (i = 0; i < 4; i++) {
        BPTR l = CreateDir((STRPTR)dirs[i]);   /* fails when it exists: fine */
        if (l)
            UnLock(l);
    }
}

/* TE-7.1 (D1): the reset-bound fields changed. The requester lives in the commodity (desk_confirm_dos.c): DESK CONFIRM with
 * WBR_ASK_MASK, as every other process asks for the Keep / Revert one. */
static void gt_dos_reset_ask(unsigned mask)
{
    char res[32];
    (void)mask;
    if (desk_dos_confirm_begin)
        desk_dos_confirm_begin(WBR_ASK_MASK);
    else
        rexx_send("AMARCHY", "DESK CONFIRM 1073741824", res, sizeof res);
}

int gt_dos_save(const char *name, const gt_model *m, char *why, int cap)
{
    char cur[GT_NAME];
    int r;
    gt_dos_drawers();
    gt_dos_current(cur, (int)sizeof cur);
    r = gt_save_editor(&prefs_dos_fs, name, m, cur, gt_dos_apply_saved, wbr_bound_mask, gt_dos_reset_ask, why, cap);
    gt_dos_desk_restore();      /* TE-6.2: a saved edit of another theme leaves no preview on the desktop */
    return r;
}

int gt_dos_save_as(const char *name, const gt_model *m, char *why, int cap)
{
    gt_dos_drawers();
    return gt_save_as(&prefs_dos_fs, name, m, why, cap);
}

int gt_dos_duplicate(const char *from, const char *to, char *why, int cap)
{
    gt_dos_drawers();
    return gt_duplicate(&prefs_dos_fs, gt_dos_read, 0, from, to, why, cap);
}

int gt_dos_import(const char *src_path, const char *name, char *why, int cap)
{
    gt_dos_drawers();
    return gt_import(&prefs_dos_fs, gt_dos_read, 0, src_path, name, why, cap);
}

int gt_dos_export(const char *name, const char *dst_path, char *why, int cap)
{
    return gt_export(&prefs_dos_fs, gt_dos_read, 0, name, dst_path, why, cap);
}

int gt_dos_delete_user(const char *name, char *why, int cap)
{
    return gt_delete_user(&prefs_dos_fs, name, why, cap);
}

int gt_dos_pick_file(struct Screen *scr, struct Window *win, const char *title, char *out, int cap)
{
    int named = 0;
    if (!asl_pick_file(scr, win, title, "RAM:", "#?.theme", 1, out, cap, &named))
        return 0;
    return named;
}

/* TE-8.1: Reset to Base: the edit buffer takes its base theme's model. 1 reset. */
int gt_dos_reset_base(void)
{
    return gte_reset_to_base(gt_dos_read, 0);
}

/* TE-4: load the named theme's model into the editor's edit buffer (gte_open). 1 opened; 0 when the theme has no model. */
int gt_dos_edit_open(const char *name)
{
    gte_live = gt_dos_preview_model;    /* TE-6.3: every edit of the buffer shows on the desktop */
    return gte_open_from(gt_dos_read, 0, name);
}
