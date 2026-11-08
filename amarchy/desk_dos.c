/* desk_dos: see desk_dos.h. */
#include <string.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "desk_dos.h"
#include "gthemes_dos.h"
#include "rexx_host.h"
#include "desk_core.h"
#include "bar_core.h"
#include "menu_theme.h"
#include "dopustb_dos.h"
#include "amconf_dos.h"
#include "flat_shared.h"
#include "win_refresh.h"
#include "proclaunch.h"
#include "sentinel_dos.h"
#include "iconpen_dos.h"

#define DK_BUF 1024
#define DK_REVERT "T:amarchy-revert"
#define DK_WORK "T:amarchy-desk"

unsigned long desk_dos_applies, desk_dos_last_mask;

static long dk_read(const char *path, unsigned char *buf, long cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    LONG got;
    if (!f)
        return -1;
    got = Read(f, buf, cap);
    Close(f);
    return got < 0 ? 0 : got;
}

static int dk_write(const char *path, const unsigned char *buf, long len)
{
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    LONG put;
    if (!f)
        return 0;
    put = Write(f, (APTR)buf, len);
    Close(f);
    return put == len;
}

static int dk_font_exists(void *ctx, const char *name)
{
    char path[160];
    BPTR l;
    (void)ctx;
    strcpy(path, "FONTS:");
    strncat(path, name, 100);
    l = Lock((STRPTR)path, ACCESS_READ);
    if (l)
        UnLock(l);
    return l != 0;
}

/* the revert copy name of a snapshot file: ':' and '/' become '-' */
static void dk_copy_name(const char *file, char *out)
{
    desk_copy_name(DK_REVERT, file, out, 96);
}

/* FP1-REVERT-THEME-LINE: the theme in force before the pick (gt_dos_select hands it over); the first snapshot of an unconfirmed apply records it */
#define DK_PREVTHEME DK_REVERT "/prevtheme"
static char dk_prev_theme[GT_NAME];
void desk_dos_prev_theme(const char *name)
{
    strncpy(dk_prev_theme, name ? name : "", GT_NAME - 1);
    dk_prev_theme[GT_NAME - 1] = 0;
}

static void dk_snapshot_theme(void);
static unsigned dk_pending_mask(void);
static void dk_snapshot(unsigned mask)
{
    static unsigned char buf[8192];
    const char *list[16];
    char name[96];
    int i, n = desk_snapshot(mask, list, 16);
    UnLock(CreateDir((STRPTR)DK_REVERT));
    for (i = 0; i < n; i++) {
        long len = dk_read(list[i], buf, sizeof buf);
        dk_copy_name(list[i], name);
        if (len < 0)
            DeleteFile((STRPTR)name); /* no original: a stale copy must not stand for one */
        else
            dk_write(name, buf, len);
    }
    dk_snapshot_theme();
}

static void dk_snapshot_theme(void)
{
    char text[GT_NAME + 24];
    long n;
    /* FP-1: an unconfirmed apply keeps its first snapshot, name included; the held name is consumed either way */
    n = desk_revert_theme_take(dk_prev_theme, dk_pending_mask() != 0, text, (int)sizeof text);
    if (n < 0)
        return;
    if (n > 0)
        dk_write(DK_PREVTHEME, (const unsigned char *)text, n);
    else
        DeleteFile((STRPTR)DK_PREVTHEME);   /* no name known: a stale one must not stand for it */
}

static unsigned long dk_iprefs_idle_mask(void);
static void dk_wait_iprefs(unsigned long idle_mask);
unsigned long desk_dos_editor_waits;

/* run one stock editor on a prefs file, USE: ENV: only, no window left behind. RL15 AMARCHY-WBPREFS-INUSE: a Workbench or WBPattern
 * USE is followed by the IPrefs idle wait here, the one place every editor run goes through: IPrefs opens ENV:Sys/Workbench.prefs
 * about 6.7 s later in its Workbench reset, and a second Workbench write in that window failed "object is in use" (Error 202). */
static void dk_editor(const char *editor, const char *file)
{
    char cmd[160], prog[80];
    unsigned long idle = 0;
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE), out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    strcpy(prog, "SYS:Prefs/");
    strcat(prog, editor);
    strcpy(cmd, prog);
    strcat(cmd, " FROM ");
    strcat(cmd, file);
    strcat(cmd, " USE");
    /* its $STACK cookie, floor 32768 (rule 6): under WBStartup the child gets 4096 otherwise */
    if (desk_editor_waits_iprefs(editor))
        idle = dk_iprefs_idle_mask(); /* before the editor: the Wait() IPrefs returns to when done */
    SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, NP_StackSize, pl_stack(prog, 32768), TAG_DONE);
    if (in)
        Close(in);
    if (out)
        Close(out);
    if (desk_editor_waits_iprefs(editor)) {
        desk_dos_editor_waits++;
        dk_wait_iprefs(idle);
    }
}

static int dk_screen_depth(void)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct Screen *scr;
    int d = 2;
    if (IntuitionBase && (scr = ipd_lock_wb_screen()) != 0) {
        d = scr->RastPort.BitMap->Depth;
        UnlockPubScreen(0, scr);
    }
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return d;
}

/* publish one part: a work copy for the editor, then the editor (its USE writes the ENV: file). Writing the ENV: file
 * here as well made IPrefs open it on the change notice while the editor wrote it: "Error accessing file
 * ENV:Sys/Pointer.prefs, object is in use" (Program Error of SYS:Prefs/Pointer). */
static int dk_put(const char *editor, const char *work, const unsigned char *buf, long len)
{
    if (len <= 0 || !dk_write(work, buf, len))
        return 0;
    dk_editor(editor, work);
    return 1;
}

/* may SYS:Prefs/Pointer be fed the 3.2 pointer prefs: version 47+ (desk_pointer_editor_ok) */
/* FLAT-P1-2: icontrol.prefs with the geometry of the border size, both copies; a file that does not fit the buffer is left alone */
#define DK_IC_BUF 8192
static int dk_icontrol(int thin, const gtheme *th)
{
    static unsigned char cur[DK_IC_BUF], out[DK_IC_BUF];
    unsigned char geom[4];
    long len = dk_read("ENV:Sys/icontrol.prefs", cur, sizeof cur), n;
    if (len < 0)
        len = dk_read("ENVARC:Sys/icontrol.prefs", cur, sizeof cur);
    if (len >= DK_IC_BUF - 64)
        return 0;
    desk_geometry_bar(thin, gt_look_bar_height(th), geom);
    n = desk_icontrol(len > 0 ? cur : 0, len > 0 ? len : 0, geom, out, sizeof out);
    return n > 0 && dk_write("ENVARC:Sys/icontrol.prefs", out, n) && dk_write("ENV:Sys/icontrol.prefs", out, n);
}

/* FLAT-P1-2 off: the snapshot dk_snapshot took before the thin write goes back over both copies (a copy that did not exist then is deleted) */
static int dk_icontrol_restore(void)
{
    static unsigned char buf[DK_IC_BUF];
    static const char *const files[2] = { "ENVARC:Sys/icontrol.prefs", "ENV:Sys/icontrol.prefs" };
    char name[96];
    int i, ok = 1;
    for (i = 0; i < 2; i++) {
        long len;
        dk_copy_name(files[i], name);
        len = dk_read(name, buf, sizeof buf);
        if (len < 0)
            DeleteFile((STRPTR)files[i]);
        else
            ok &= dk_write(files[i], buf, len);
    }
    return ok;
}

/* the saved pre-thin icontrol.prefs (T:amarchy-revert copy of ENVARC:Sys/icontrol.prefs): its length, -1 when there is none */
static long dk_read_icontrol_snapshot(unsigned char *buf, long size)
{
    char name[96];
    dk_copy_name("ENVARC:Sys/icontrol.prefs", name);
    return dk_read(name, buf, size);
}

static int dk_pointer_editor_ok(void)
{
    static unsigned char img[65536];
    long len = dk_read("SYS:Prefs/Pointer", img, sizeof img);
    return len > 0 && desk_pointer_editor_ok(img, len);
}

/* is SYS:Prefs/Palette 47+ (3.2, desk_palette_editor_ok). Below 47 (3.1: Palette 40.2, IPrefs 40.7) a palette.prefs with a 13th pen word
 * overwrites ExecBase (DESK-31-CRASH, S56-PALETTE-31-SAFE): every palette.prefs written there is the 3.1-safe form. */
static int dk_palette_editor_ok(void)
{
    static unsigned char img[65536];
    long len = dk_read("SYS:Prefs/Palette", img, sizeof img);
    return len > 0 && desk_palette_editor_ok(img, len);
}

/* S56-PALETTE-31-SAFE: the one way a palette.prefs reaches SYS:Prefs/Palette. Below 47 the bytes go through desk_palette_31safe first; a file
 * it refuses (no PALT, unterminated pen list) is not written at all. */
unsigned long desk_dos_pal31_puts;
static int dk_put_palette(const unsigned char *buf, long len)
{
    static unsigned char safe[DK_BUF];
    if (!dk_palette_editor_ok()) {
        len = desk_palette_31safe(buf, len, safe, sizeof safe);
        buf = safe;
        desk_dos_pal31_puts++;
    }
    return dk_put("Palette", DK_WORK "/palette.prefs", buf, len);
}

int (*desk_dos_wb_close)(void);
void (*desk_dos_wb_open)(int was);
unsigned long desk_dos_wb_closes;
int (*desk_dos_own_close)(void);
void (*desk_dos_own_open)(void);

/* the Workbench screen's address (0 = none): a new one means IPrefs finished the reset. pens (when not 0) gets the live
 * DrawInfo pen array, at most DK_SLOTS entries, and *npens dri_NumPens. */
static struct Screen *dk_wb_screen(unsigned short *pens, int *npens)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct Screen *scr = 0, *s;
    if (npens)
        *npens = 0;
    if (IntuitionBase && (s = ipd_lock_wb_screen()) != 0) {
        struct DrawInfo *dri = pens ? GetScreenDrawInfo(s) : 0;
        scr = s;
        if (dri) {
            int i;
            *npens = dri->dri_NumPens;
            for (i = 0; i < DK_SLOTS && i < dri->dri_NumPens; i++)
                pens[i] = dri->dri_Pens[i];
            FreeScreenDrawInfo(s, dri);
        }
        UnlockPubScreen(0, s);
    }
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return scr;
}

/* Close our Workbench windows; returns 1 when the bar was open (the state to restore: the want flag is never touched).
 * Another process sends BAR STATUS / BAR OFF; the main process installs direct hooks. */
static int dk_windows_close(void)
{
    char res[32];
    if (desk_dos_own_close)
        desk_dos_own_close();
    if (desk_dos_wb_close)
        return desk_dos_wb_close();
    if (rexx_send("AMARCHY", "BAR STATUS", res, sizeof res) != 0 || strncmp(res, "bar=1", 5) != 0)
        return 0;
    rexx_send("AMARCHY", "BAR OFF", res, sizeof res);
    return 1;
}

static void dk_windows_open(int was)
{
    char res[32];
    if (desk_dos_own_open)
        desk_dos_own_open();
    if (!was)
        return;
    if (desk_dos_wb_open)
        desk_dos_wb_open(was);
    else
        rexx_send("AMARCHY", "BAR ON", res, sizeof res);
}

/* Windows of ours back once IPrefs applied the palette: the live pens equal the desk pens (3.2 resets in place, the screen
 * address stays) or the screen is a new one (at most DK_RESET_SECS: a part that changes nothing resets nothing). */
#define DK_RESET_SECS 6
static void dk_wait_reset(struct Screen *before, int flat, int depth)
{
    int i;
    for (i = 0; i < DK_RESET_SECS * 4; i++) {
        unsigned short live[DK_SLOTS];
        int n;
        struct Screen *now = dk_wb_screen(live, &n);
        if (desk_reset_done(before, now, live, n, flat, depth))
            break;
        Delay(12);
    }
}


/* IPrefs' scheduling state (DESK-31): *have 0 when no task named "IPrefs" runs, else *waiting (TS_WAIT), its wait mask and
 * the signals it has received but not yet taken. Read under Disable(): a notification sets tc_SigRecvd from another task. */
static void dk_iprefs(int *have, int *waiting, unsigned long *sigwait, unsigned long *recvd)
{
    extern struct ExecBase *SysBase;
    struct List *lists[2];
    struct Node *n;
    int i;
    *have = *waiting = 0;
    *sigwait = *recvd = 0;
    Disable();
    lists[0] = &SysBase->TaskWait;
    lists[1] = &SysBase->TaskReady;
    for (i = 0; i < 2 && !*have; i++)
        for (n = lists[i]->lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name && strstr(n->ln_Name, "IPrefs")) {
                struct Task *t = (struct Task *)n;
                *have = 1;
                *waiting = t->tc_State == TS_WAIT;
                *sigwait = t->tc_SigWait;
                *recvd = t->tc_SigRecvd;
                break;
            }
    Enable();
}

/* The wait mask of IPrefs while it waits for a notification, taken before any editor runs (0: no IPrefs or never seen waiting) */
static unsigned long dk_iprefs_idle_mask(void)
{
    int i, have, waiting;
    unsigned long w, r;
    for (i = 0; i < DK_RESET_SECS * 50 / DESK_IPREFS_POLL_TICKS; i++) {
        dk_iprefs(&have, &waiting, &w, &r);
        if (!have || waiting)
            return have ? w : 0;
        Delay(DESK_IPREFS_POLL_TICKS);
    }
    return 0;
}

/* Windows of ours back only once IPrefs has processed every file the editors wrote: a window opened inside its Workbench reset
 * gets the "close all windows" System Request (DESK-31, rig 7 Run 10: 3.1 Font USE, two resets 0.3-0.8 s after the editor).
 * The positive event is IPrefs back in its idle Wait() with nothing pending; DK_RESET_SECS only bounds a wedged IPrefs. */
static void dk_wait_iprefs(unsigned long idle_mask)
{
    int i, have, waiting;
    unsigned long w, r;
    for (i = 0; i < DK_RESET_SECS * 50 / DESK_IPREFS_POLL_TICKS; i++) {
        dk_iprefs(&have, &waiting, &w, &r);
        if (desk_iprefs_idle(have, waiting, w, r, idle_mask))
            break;
        Delay(DESK_IPREFS_POLL_TICKS);
    }
}

/* FLAT-D2: Directory Opus's lister pens are palette pens, so they follow the Palette write, through the one mapping dopus_theme_send
 * (menu_theme.c). No DOpus port: rexx_send gives -1, which stops the sending and is no error. */
unsigned long desk_dos_dopus_sends;

static long dk_dopus_send(const char *port, const char *line, char *result, int cap, void *ud)
{
    (void)ud;
    return rexx_send(port, line, result, cap);
}

static void dk_live_pens(const gtheme *th, int flat);
static void dk_confirm_ask(unsigned mask);
/* the live role pens (FLAT-PEN5-MAGENTA) are set last, in desk_dos_apply: every Workbench reset of this apply (palette, wbpattern,
 * wbborder USE) reopens the screen with the graphics defaults on pens 4-7. 1 = theme colours, -1 = restore the kept ones, 0 = leave. */
static int dk_live_want, dk_live_flat;
static const gtheme *dk_rv_theme;   /* FP1-REVERT-THEME-LINE: the previous theme while desk_dos_revert restores; 0 = the stock baseline */
static long dk_palette_base(int restore, const unsigned char pens[DK_SLOTS], int depth, unsigned char *buf, long cap);

/* FP-1 (R1.8): the unconfirmed-apply marker T:amarchy-revert/pending holds the mask of targets whose snapshot is waiting for Keep or
 * Revert; a file, so the menu process (which applies) and the commodity (which asks and reverts) share it. */
#define DK_PENDING DK_REVERT "/pending"
static unsigned dk_pending_mask(void)
{
    unsigned char b[16];
    long n = dk_read(DK_PENDING, b, sizeof b - 1), i;
    unsigned m = 0;
    for (i = 0; i < n && b[i] >= '0' && b[i] <= '9'; i++)
        m = m * 10 + (unsigned)(b[i] - '0');
    return m;
}

int desk_dos_confirm_pending(void)
{
    return dk_pending_mask() != 0;
}

static void dk_orig_off(unsigned desk_mask, int flat);
static void dk_orig_save(unsigned mask, int wbprefs);
static int dk_apply(const gtheme *th, char *why, int cap)
{
    static unsigned char buf[DK_BUF], prev[DK_BUF], big[8192];
    am_prefs *ap;
    unsigned mask, dmask;
    int depth, plan, ok = 1, old_ws = 0, flat_pens, icon_text;
    long len, plen;
    struct Screen *wb_before = 0;
    unsigned long idle = 0;
    unsigned short live[DK_SLOTS];
    int nlive = 0, was = 0, reset = 0, ic_act, ic_snap = 0;
    static unsigned char ic_cur[DK_BUF];
    static unsigned char ic_snap_buf[DK_IC_BUF];
    long ic_len, ic_snap_len;
    ap = amconf_borrow();
    flat_pens = amconf_desk_flat_pens(ap);
    dmask = amconf_desk_mask(ap);
    icon_text = amconf_icon_text(ap);
    amconf_return(ap);                  /* the three values are all dk_apply reads: no exit below holds the borrow */
    /* FLAT-P1-2: the border size acts on its own (thin once, stock restores the snapshot), whatever else the switches enable */
    ic_len = dk_read("ENV:Sys/icontrol.prefs", ic_cur, sizeof ic_cur);
    if (ic_len < 0)
        ic_len = dk_read("ENVARC:Sys/icontrol.prefs", ic_cur, sizeof ic_cur);
    ic_snap_len = dk_read_icontrol_snapshot(ic_snap_buf, sizeof ic_snap_buf);
    ic_act = desk_icontrol_decide((dmask >> DT_ICONTROL) & 1, gt_look_bar_height(th), ic_cur, ic_len, ic_snap_buf, ic_snap_len, &ic_snap);
    plan = desk_plan(dmask & ~(1u << DT_ICONTROL), th, why, cap);
    if (plan < 0 || (plan == 0 && ic_act == DK_IC_NONE))
        return plan;
    mask = (unsigned)plan | (ic_act != DK_IC_NONE ? 1u << DT_ICONTROL : 0u);
    mask = desk_gate_mask(mask, dk_palette_editor_ok(), &old_ws); /* the palette bit stays on 3.1: dk_put_palette writes the safe form */
    if (!mask)
        return 0;
    UnLock(CreateDir((STRPTR)DK_WORK));
    if (old_ws || (mask & ((1u << DT_FONT) | (1u << DT_ICONTROL))))
        idle = dk_iprefs_idle_mask(); /* before any editor: the Wait() IPrefs returns to when done */
    dk_orig_save(mask, 0);   /* FP-2: the user's own version of every file about to be rewritten, once */
    dk_snapshot((ic_snap ? mask : (mask & ~(1u << DT_ICONTROL))) & ~dk_pending_mask()); /* FP-1: targets of an unconfirmed apply keep their first snapshot */
    depth = dk_screen_depth();
    if (mask & (1u << DT_PALETTE)) {
        wb_before = dk_wb_screen(live, &nlive);
        reset = desk_pens_reset(live, nlive, flat_pens, depth);
    }
    if (reset || old_ws) {
        desk_dos_wb_closes++;
        was = dk_windows_close();
    }
    desk_dos_applies++;
    desk_dos_last_mask = mask;
    if (mask & (1u << DT_PALETTE)) {
        unsigned char pens[DK_SLOTS];
        desk_pens_for(flat_pens, pens);
        /* FLAT-PEN5-MAGENTA: the theme colours on the entries that reach the role pens at this depth; on a deep screen entries 4-7
         * land on the last four pens, so those keep the user's colours and pens 4-7 are set live below */
        len = dk_palette_base(0, pens, depth, prev, sizeof prev);
        len = len > 0 ? desk_palette_theme(prev, len, th, depth, flat_pens, buf, sizeof buf) : 0;
        if (len <= 0)
            len = desk_palette(th, depth >= 3 ? 8 : 4, pens, buf, sizeof buf);
        ok &= dk_put_palette(buf, len);
        /* IPrefs resets Workbench for the palette: the other editors run only after it finished (SYS:Prefs/Pointer
         * raised a Program Error requester when started under the running reset) */
        if (reset)
            dk_wait_reset(wb_before, flat_pens, depth);
        dk_live_want = 1;
        dk_live_flat = flat_pens;
        desk_dos_dopus_sends++;
        dopus_theme_send(th, dk_dopus_send, 0, &dopustb_dos_ops);
    }
    if (mask & (1u << DT_WBPATTERN)) {
        plen = dk_read("ENV:Sys/wbpattern.prefs", prev, sizeof prev);
        len = desk_wbpattern(th, depth > 3 ? 3 : depth, plen > 0 ? prev : 0, plen > 0 ? plen : 0, buf, sizeof buf);
        ok &= dk_put("WBPattern", DK_WORK "/wbpattern.prefs", buf, len); /* dk_editor waits for IPrefs */
    }
    if ((mask & (1u << DT_POINTER)) && !dk_pointer_editor_ok()) {
        /* R1.17: the 3.1 editor reset-loops on the 3.2 pointer prefs; leave the pointer alone there */
    } else if (mask & (1u << DT_POINTER)) {
        len = dk_read("ENV:Sys/pointer.prefs", big, sizeof big);
        if (len < 0)
            len = dk_read("ENVARC:Sys/Pointer.prefs", big, sizeof big);
        if (len > 0 && desk_pointer(th, big, len) > 0)
            ok &= dk_put("Pointer", DK_WORK "/pointer.prefs", big, len);
        else
            ok = 0;
    }
    if (mask & (1u << DT_FONT)) {
        len = dk_read("ENV:Sys/font.prefs", buf, sizeof buf);
        if (len < 0)
            len = dk_read("ENVARC:Sys/font.prefs", buf, sizeof buf);
        if (len <= 0)
            len = desk_font_default(buf, sizeof buf);
        if (len > 0 && desk_font_pens(buf, len, icon_text) >= 0 && desk_font_name(th, buf, len, dk_font_exists, 0) >= 0) {
            ok &= dk_put("Font", DK_WORK "/font.prefs", buf, len);
            if (reset || old_ws)
                dk_wait_iprefs(idle); /* the Font editor resets the Workbench screen too (3.1: twice): the windows return after it */
        } else
            ok = 0;
    }
    if (mask & (1u << DT_ICONTROL)) {
        /* FLAT-P1-2: the file IPrefs watches is written in place, both copies, other fields and IEXC chunks kept (no editor is fed it:
         * SYS:Prefs/IControl would rewrite the exception list); then the same idle wait as the other prefs */
        ok &= ic_act == DK_IC_RESTORE ? dk_icontrol_restore() : dk_icontrol(1, th);
        dk_wait_iprefs(idle);
    }
    if (reset || old_ws)
        dk_windows_open(was);
    return ok ? (int)mask : -1;
}

/* FLAT-C9: write the flat_state fields of the running amarchy-flat from the prefs (the one mapping desk_flat_state), then redraw every
 * window frame so the switch shows at once. The layout is checked (flat_check) before the write; the other fields are the program's.
 * No record: the program is not running, so start it (BOOT: resident only when a flat switch is on; it writes the same fields itself). */
unsigned long desk_dos_flat_writes;
/* L9: flat on puts the filled icons on the Workbench default icons (amarchy-icons APPLY), flat off puts the originals back (RESTORE).
 * One process per change of the on/off state, so two quick prefs writes never run two at once on the manifest; RESTORE with no
 * manifest is a no-op, so the first sync after a start with flat off costs nothing. Counters are the sentinel. */
unsigned long desk_dos_icons_apply, desk_dos_icons_restore;

static void dk_icons_sync(int on, int outline)
{
    static int last = -1;
    int state = desk_icons_state(on, outline);
    if (state == last) return;                 /* a change of the icon style while on runs APPLY again: it reads the pref itself */
    last = state;
    if (on) {
        desk_dos_icons_apply++;
        pl_start(PL_BIN_DIR "/amarchy-icons APPLY", PL_BIN_DIR "/amarchy-icons", 16384, "amarchy-icons");     /* its $STACK cookie (rule 6) */
    } else {
        desk_dos_icons_restore++;
        pl_start(PL_BIN_DIR "/amarchy-icons RESTORE", PL_BIN_DIR "/amarchy-icons", 16384, "amarchy-icons");
    }
}

/* changed_only: the prefs-file notify path (desk_dos_flat_sync): write and redraw only when the block's fields differ, so a keymap edit
 * or the commodity's own save after a desk apply redraws nothing. */
/* FLAT-ABSENT-AT-BOOT: no Amarchy.Flat record and a flat switch on: start amarchy-flat (BOOT form: resident only when a switch is on;
 * it writes the theme fields itself). The one place Amarchy starts it, for a desk apply and for the start gate alike. */
unsigned long desk_dos_flat_starts;

static void dk_flat_start(int flat, int gad)
{
    if (!flat && !gad)
        return;
    desk_dos_flat_starts++;
    am_sentinel("AMARCHY_FLAT_START", (long)desk_dos_flat_starts);
    pl_start(PL_BIN_DIR "/amarchy-flat BOOT", PL_BIN_DIR "/amarchy-flat", 32768, "amarchy-flat");   /* its $STACK cookie (rule 6) */
}

static void dk_flat_sync_theme(const gtheme *thp, int changed_only)
{
    am_prefs *ap;
    flatshared *r;
    const gtheme *th = thp;
    int flat, gad, aa, wrote = 0;
    ap = amconf_borrow();
    flat = amconf_flat_index(ap, AM_FLAT);
    gad = amconf_flat_index(ap, AM_FLATGAD);
    aa = amconf_flat_index(ap, AM_UIAA);
    dk_icons_sync(flat || gad, amconf_icon_outline(ap));
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r && flat_check(&r->st, sizeof r->st) == FLAT_OK) {
        flat_state next = r->st;
        desk_flat_state(flat, gad, aa, &next);
        desk_flat_inbar(th, &next);                 /* S32-INACTIVE-BAR: the theme `inactive` colour */
        desk_flat_look(th, &next);                  /* TE-6.1: the version-5 look fields and knob colour */
        if (!changed_only || memcmp(&next, &r->st, sizeof next) != 0) {
            desk_flat_state(flat, gad, aa, &r->st);
            desk_flat_inbar(th, &r->st);
            desk_flat_look(th, &r->st);
            if (r->task)
                Signal(r->task, SIGBREAKF_CTRL_D);  /* LUCIDE L7: amarchy-flat takes or drops the icon pens */
            wrote = 1;
        }
    }
    Permit();
    if (wrote) {
        desk_dos_flat_writes++;
        win_refresh_frames();
    } else if (!r) {
        dk_flat_start(flat, gad);
    }
    amconf_return(ap);
}

static void dk_flat_sync_with(int changed_only)
{
    gtheme th;
    gt_dos_load_current(&th);
    dk_flat_sync_theme(&th, changed_only);
}

static void dk_flat_sync(void)
{
    dk_flat_sync_with(0);
}

/* TE-6.2: the editor's in-memory theme reaches the running amarchy-flat (changed fields only); desk_dos_flat_sync puts the saved theme back */
void desk_dos_flat_preview(const gtheme *th)
{
    dk_flat_sync_theme(th, 1);
}

/* FLAT-PREFS-NOT-NOTIFIED: ENV:Amarchy/prefs changed under the commodity (its notify, RELOAD): a flat switch flipped by any writer
 * reaches the running amarchy-flat, or starts it, as a desk apply does. */
void desk_dos_flat_sync(void)
{
    dk_flat_sync_with(1);
}

/* FLAT-ABSENT-AT-BOOT (owner 2026-11-07, rig 93/94): the flat pens (SHINE = SHADOW = BACKGROUND) reach the screen from Palette.prefs
 * and the start steps whether amarchy-flat runs or not, and without it every bevel it does not redraw vanishes (Installer buttons,
 * the window bottom ring). S:User-Startup's BOOT line was the only start; a system without it (a master image installed before the
 * Amarchy-Flat block, a failed Run) booted flat pens with no flat program. Invariant: a flat switch on means amarchy-flat is resident
 * once the start gate has run. */
int desk_dos_flat_ensure(void)
{
    am_prefs *ap;
    int flat, gad, absent;
    ap = amconf_borrow();
    flat = amconf_flat_index(ap, AM_FLAT);
    gad = amconf_flat_index(ap, AM_FLATGAD);
    amconf_return(ap);
    Forbid();
    absent = FindSemaphore((STRPTR)FLAT_SEM_NAME) == 0;
    Permit();
    if (!absent || (!flat && !gad))
        return 0;
    dk_flat_start(flat, gad);
    return 1;
}

/* FLAT-PENS-LIVE: flat-system-look writes the flat pen roles itself (dk_apply carries them only on a desktop palette write). The user's
 * colours stay: ENV:Sys/Palette.prefs (else ENVARC:) with only the pen arrays replaced, else the colours on the screen now. The write
 * goes through the palette editor like every other part (dk_put); the reset it makes is dk_apply's business (closing windows around it
 * is not done here), this waits for it to finish. */
unsigned long desk_dos_flat_pens_writes;

#define DK_FLAT_PAL DK_REVERT "/flat-palette"
#define DK_FLAT_PAT DK_REVERT "/flat-wbpattern"
#define DK_FLAT_TAKEN DK_REVERT "/flat-taken"

static int dk_exists(const char *file)
{
    BPTR l = Lock((STRPTR)file, ACCESS_READ);
    if (l)
        UnLock(l);
    return l != 0;
}

/* FLAT-BACKDROP-SAME-AS-WINDOW: flat-system-look puts BACKGROUNDPEN on pen 0, the window colour, so a Workbench with no wbpattern.prefs
 * fills its backdrop with the window colour. Flat on (desktop-theme not writing the pattern itself): the ROOT tile is the solid
 * backdrop pen (desk_backdrop_pen, not BACKGROUNDPEN) and that pen's palette colour is the theme dark (gt_darker). First turn-on
 * snapshots the previous palette and pattern (or their absence); flat off puts them back. */
static void dk_flat_backdrop_take(const unsigned char *pal, long pallen)
{
    static unsigned char prev[DK_BUF];
    long plen;
    if (dk_exists(DK_FLAT_TAKEN))
        return;
    UnLock(CreateDir((STRPTR)DK_REVERT));
    dk_write(DK_FLAT_PAL, pal, pallen);
    plen = dk_read("ENV:Sys/wbpattern.prefs", prev, sizeof prev);
    if (plen < 0)
        DeleteFile((STRPTR)DK_FLAT_PAT);
    else
        dk_write(DK_FLAT_PAT, prev, plen);
    dk_write(DK_FLAT_TAKEN, (const unsigned char *)"1", 1);
}

/* FLAT-PEN5-MAGENTA: the role pens no palette.prefs entry reaches at the Workbench depth (desk_live_role_pens: 4-7 on a screen deeper
 * than 3, where IPrefs puts entries 4-7 on the last four pens) get their colour live, after IPrefs applied the file. th = the theme;
 * th = 0 puts the user's colours back: FLAT-LIVEBASE-SAVED, from the saved palette (the kept DK_FLAT_PAL, else palette.prefs in ENV or
 * ENVARC) through desk_live_baseline, never from the live screen (an open Amarchy menu holds pens 4-7 with its own colours). The Workbench
 * reopen that follows makes a new screen with graphics-default 4-7: FLAT-PENS47-RESET, iconpen_dos pens_sync sets them on every new
 * screen through desk_live_follow. */
unsigned long desk_dos_live_pens;
static void dk_live_pens(const gtheme *th, int flat)
{
    static unsigned char saved[DK_BUF];
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    struct Screen *scr = IntuitionBase && GfxBase ? ipd_lock_wb_screen() : 0;
    int pens[4], n = 0, i, depth = 0;
    long slen = -1;
    if (scr) {
        depth = scr->RastPort.BitMap->Depth;
        n = desk_live_role_pens(depth, pens);
        if (!th) {
            slen = dk_read(DK_FLAT_PAL, saved, sizeof saved);
            if (slen < 0)
                slen = dk_read("ENV:Sys/Palette.prefs", saved, sizeof saved);
            if (slen < 0)
                slen = dk_read("ENVARC:Sys/Palette.prefs", saved, sizeof saved);
        }
        for (i = 0; i < n; i++) {
            uc_u32 c = th ? desk_pen_colour(th, pens[i], flat) : desk_live_baseline(slen > 0 ? saved : 0, slen, pens[i], depth);
            if (ipd_role_write(pens[i], c) < 0)      /* S57-PENS47-UNOWNED: the holder owns 4-7; no holder running = the old direct write */
                SetRGB32(&scr->ViewPort, (ULONG)pens[i], ((c >> 16) & 255) * 0x01010101UL, ((c >> 8) & 255) * 0x01010101UL,
                         (c & 255) * 0x01010101UL);
        }
        if (n > 0)
            desk_dos_live_pens++;
        UnlockPubScreen(0, scr);
    }
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
}

/* the palette.prefs to colour, with the pen map pens: the kept user palette (restore), else the file in force, else a snapshot of the
 * live screen (each entry from the pen IPrefs puts it on, desk_palette_live). Returns its length, 0 when none. */
static long dk_palette_base(int restore, const unsigned char pens[DK_SLOTS], int depth, unsigned char *buf, long cap)
{
    static unsigned char cur[DK_BUF];
    static unsigned short rgb4[256];
    long len = restore ? dk_read(DK_FLAT_PAL, cur, sizeof cur) : -1;
    int i, n = 0;
    if (len < 0)
        len = dk_read("ENV:Sys/Palette.prefs", cur, sizeof cur);
    if (len < 0)
        len = dk_read("ENVARC:Sys/Palette.prefs", cur, sizeof cur);
    len = len > 0 ? desk_palette_pens(cur, len, pens, buf, cap) : 0;
    if (len > 0)
        return len;
    {
        struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
        struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
        struct Screen *scr = IntuitionBase && GfxBase ? ipd_lock_wb_screen() : 0;
        if (scr) {
            n = 1 << scr->RastPort.BitMap->Depth;
            if (n > 256)
                n = 256;
            for (i = 0; i < n; i++)
                rgb4[i] = (unsigned short)GetRGB4(scr->ViewPort.ColorMap, i);
            UnlockPubScreen(0, scr);
        }
        if (GfxBase)
            CloseLibrary((struct Library *)GfxBase);
        if (IntuitionBase)
            CloseLibrary((struct Library *)IntuitionBase);
    }
    return desk_palette_live(rgb4, n, depth, pens, buf, cap);
}

/* FLAT-PENS-STALE-FILE: 1 when the palette.prefs a reset would read (ENV:, else ENVARC:) does not hold pens exactly */
static int dk_palette_file_stale(int flat, const unsigned char pens[DK_SLOTS])
{
    static unsigned char cur[DK_BUF];
    long len = dk_read("ENV:Sys/Palette.prefs", cur, sizeof cur);
    if (len < 0)
        len = dk_read("ENVARC:Sys/Palette.prefs", cur, sizeof cur);
    return desk_flat_start_refresh(flat, cur, len, pens);
}

/* FLAT-PENS-LIVE + FLAT-PEN5-MAGENTA. Owner 2026-11-01: with flat on, a theme pick writes the theme colours into the Workbench palette
 * (palette.prefs, every entry that reaches a role pen, plus the role pens no entry reaches, live), unless the desktop theme's palette
 * part writes its own; flat off puts the palette in force before flat went on back. Returns 1 when it wrote the theme colours (the
 * caller then skips wb_pens_apply, whose bg/fg/dim/accent order does not match the flat pen map). */
static int dk_flat_pens(const gtheme *th)
{
    static unsigned char buf[DK_BUF], out[DK_BUF], prev[DK_BUF];
    am_prefs *ap;
    unsigned char pens[DK_SLOTS];
    unsigned short live[DK_SLOTS];
    struct Screen *before;
    int nlive = 0, flat, own, theme, restore, sync, depth;
    unsigned long idle = 0;
    long len, wlen, plen;
    ap = amconf_borrow();
    flat = amconf_desk_flat_pens(ap);
    own = flat && !((amconf_desk_mask(ap) >> DT_WBPATTERN) & 1);
    theme = flat && !((amconf_desk_mask(ap) >> DT_PALETTE) & 1);
    amconf_return(ap);                  /* flat, own and theme are all it reads */
    restore = !flat && dk_exists(DK_FLAT_TAKEN);
    before = dk_wb_screen(live, &nlive);
    if (!before || !dk_palette_editor_ok())
        return 0;
    depth = dk_screen_depth();
    sync = desk_pens_sync(flat, live, nlive, pens, depth);
    if (!sync && !own && !theme && !restore)
        return 0;
    if (!sync)
        desk_pens_for(flat, pens);
    if (flat && !sync && dk_palette_file_stale(flat, pens))
        sync = 1; /* FLAT-PENS-STALE-FILE: the live pens match but the file on disk (an older build's map) does not */
    len = dk_palette_base(restore, pens, depth, buf, sizeof buf);
    UnLock(CreateDir((STRPTR)DK_WORK));
    if ((own || theme) && len > 0) {
        dk_flat_backdrop_take(buf, len);
        wlen = theme ? desk_palette_theme(buf, len, th, depth, flat, out, sizeof out)
                     : desk_palette_set_rgb(buf, len, desk_backdrop_screen_pen(depth), depth, gt_darker(th), out, sizeof out);
        if (wlen > 0) {
            if (wlen != len || memcmp(buf, out, wlen))
                sync = 1;
            memcpy(buf, out, wlen);
            len = wlen;
        }
    }
    if (restore)
        sync = 1;
    if (sync && len > 0) {
        /* RL15-IPREFS-SYSREQ: a pen map change resets the Workbench screen; with our windows open IPrefs' CloseWorkBench fails, its
         * "System Request" (Retry/Cancel) stays up and every wait below runs to its timeout. Same rule as dk_apply: close first. */
        int reset = desk_pens_reset(live, nlive, flat, depth), was = 0;
        idle = dk_iprefs_idle_mask();
        desk_dos_flat_pens_writes++;
        if (reset)
            was = dk_windows_close();
        if (dk_put_palette(buf, len)) {
            dk_wait_reset(before, flat, depth);
            dk_wait_iprefs(idle);
        }
        if (reset)
            dk_windows_open(was);
    }
    if (own || theme) {
        dk_live_want = 1;
        dk_live_flat = flat;
    } else if (restore)
        dk_live_want = -1;
    if (own) {
        plen = dk_read("ENV:Sys/wbpattern.prefs", prev, sizeof prev);
        wlen = desk_wbpattern(th, depth > 3 ? 3 : depth, plen > 0 ? prev : 0, plen > 0 ? plen : 0, out, sizeof out);
        if (wlen > 0 && (wlen != plen || memcmp(out, prev, wlen)))
            dk_put("WBPattern", DK_WORK "/wbpattern.prefs", out, wlen);
    } else if (restore) {
        plen = dk_read(DK_FLAT_PAT, prev, sizeof prev);
        if (plen < 0) {
            DeleteFile((STRPTR)"ENV:Sys/wbpattern.prefs");
            DeleteFile((STRPTR)"ENVARC:Sys/wbpattern.prefs");
        } else {
            dk_put("WBPattern", DK_WORK "/wbpattern.prefs", prev, plen);
        }
        DeleteFile((STRPTR)DK_FLAT_TAKEN);
    }
    if (theme && len > 0) {
        desk_dos_dopus_sends++;
        dopus_theme_send(th, dk_dopus_send, 0, &dopustb_dos_ops);
        return 1;
    }
    return 0;
}

/* AM-ICON-NOBORDER: flat-system-look sets the Workbench icon border to "No Border" (WBNC wbp_Borderless in ENV:Sys/Workbench.prefs) through the
 * 3.2 SYS:Prefs/Workbench editor (USE, like every other part: it tells workbench.library); off puts the value back that was there before. The value
 * kept lives in T:amarchy-revert/wbborder.prev (one byte, '0' or '1'). 3.1 has no Workbench editor with a WBNC chunk: skipped, no error. */
unsigned long desk_dos_wbborder_writes;
#define DK_WBB_PREV DK_REVERT "/wbborder.prev"

static int dk_wbborder_editor_ok(void)
{
    static unsigned char img[65536];
    long len = dk_read("SYS:Prefs/Workbench", img, sizeof img), i;
    for (i = 0; i + 4 <= len; i++)
        if (!memcmp(img + i, "WBNC", 4))
            return 1;
    return 0;
}

static void dk_keep_mark(const char *file, int keep)
{
    unsigned char sv[2];
    if (keep >= 0) {
        UnLock(CreateDir((STRPTR)DK_REVERT));
        sv[0] = (unsigned char)('0' + keep);
        dk_write(file, sv, 1);
    } else if (keep == -2)
        DeleteFile((STRPTR)file);
}

/* WB-VOLUME-GAUGE-OFF: the volume gauge (wbe_DisableVolumeGauge, same WBNC chunk) goes through the same one write and one editor USE as the border;
 * its saved value lives in T:amarchy-revert/wbgauge.prev. */
unsigned long desk_dos_wbgauge_writes;
#define DK_WBG_PREV DK_REVERT "/wbgauge.prev"

/* FP-1: the Workbench.prefs write of dk_wbborder (border + gauge) is outside the desk target mask; its pre-apply bytes and the two .prev marks
 * are kept in T:amarchy-revert until the apply is confirmed or reverted. The first unconfirmed apply wins (DK_WBPEND exists). */
#define DK_WBPEND DK_REVERT "/wbpending"
#define DK_WBB_SNAP DK_REVERT "/wbborder.snap"
#define DK_WBG_SNAP DK_REVERT "/wbgauge.snap"
#define DK_WB_ENV "ENV:Sys/Workbench.prefs"

static void dk_copy_file(const char *src, const char *dst)
{
    static unsigned char cb[DK_BUF];
    long len = dk_read(src, cb, sizeof cb);
    if (len < 0)
        DeleteFile((STRPTR)dst);
    else
        dk_write(dst, cb, len);
}

/* AM-ICON-NOBORDER-OFF: Workbench.prefs goes back to "absent" (the user had none): the editor's stock file is USEd, so the live Workbench gets the
 * stock border and gauge, and the ENV: file it writes STAYS. workbench.library learns a change only from IPrefs reading that file, which IPrefs does
 * on its change notice 1.5-7 s after the editor's USE (rig 56, OS 3.2, wasabi snoop); deleting it right after the USE made IPrefs' open fail
 * (Error 205) and the icons stayed frameless. ENVARC: is the persistent "absent" (dk_orig_restore_arc deletes it); a stock ENV: file reads the same
 * as none and goes at the next reboot. */
static void dk_wb_absent(void)
{
    static unsigned char st[128];
    long n = desk_wb_stock(st, sizeof st);
    UnLock(CreateDir((STRPTR)DK_WORK));
    dk_put("Workbench", DK_WORK "/workbench.prefs", st, n);
}

static void dk_wb_snapshot(void)
{
    char name[96];
    unsigned char one[2];
    if (dk_read(DK_WBPEND, one, 2) >= 0)
        return;
    UnLock(CreateDir((STRPTR)DK_REVERT));
    dk_copy_name(DK_WB_ENV, name);
    dk_copy_file(DK_WB_ENV, name);
    dk_copy_file(DK_WBB_PREV, DK_WBB_SNAP);
    dk_copy_file(DK_WBG_PREV, DK_WBG_SNAP);
    one[0] = '1';
    dk_write(DK_WBPEND, one, 1);
}

/* revert: the saved Workbench.prefs goes back through the stock editor (the stock file when it did not exist: dk_wb_absent), the .prev marks come back too */
static int dk_wb_restore(void)
{
    char name[96];
    unsigned char one[2];
    if (dk_read(DK_WBPEND, one, 2) < 0)
        return 0;
    dk_copy_name(DK_WB_ENV, name);
    if (dk_read(name, one, 0) < 0)
        dk_wb_absent();
    else
        dk_editor("Workbench", name);
    dk_copy_file(DK_WBB_SNAP, DK_WBB_PREV);
    dk_copy_file(DK_WBG_SNAP, DK_WBG_PREV);
    DeleteFile((STRPTR)DK_WBPEND);
    return 1;
}

static void dk_wbborder(void)
{
    static unsigned char cur[DK_BUF], mid[DK_BUF], out[DK_BUF];
    am_prefs *ap;
    unsigned char sv[2];
    int flat, saved = -1, keep, gsaved = -1, gkeep;
    long len, n, g;
    ap = amconf_borrow();
    flat = amconf_desk_flat_pens(ap);
    amconf_return(ap);                  /* flat is all it reads */
    if (!dk_wbborder_editor_ok())
        return;
    if (dk_read(DK_WBB_PREV, sv, 2) == 1)
        saved = sv[0] - '0';
    if (dk_read(DK_WBG_PREV, sv, 2) == 1)
        gsaved = sv[0] - '0';
    len = dk_read("ENV:Sys/Workbench.prefs", cur, sizeof cur);
    if (len < 0)
        len = dk_read("ENVARC:Sys/Workbench.prefs", cur, sizeof cur);
    if (len >= DK_BUF)
        return;
    if (len < 0)
        len = 0;
    n = desk_wbborder(flat, cur, len, saved, &keep, mid, sizeof mid);
    g = desk_wbgauge(flat, n > 0 ? mid : cur, n > 0 ? n : len, gsaved, &gkeep, out, sizeof out);
    if (n > 0 || g > 0) {
        dk_orig_save(0, 1);
        dk_wb_snapshot();
        UnLock(CreateDir((STRPTR)DK_WORK));
        if (n > 0)
            desk_dos_wbborder_writes++;
        if (g > 0)
            desk_dos_wbgauge_writes++;
        if (!dk_put("Workbench", DK_WORK "/workbench.prefs", g > 0 ? out : mid, g > 0 ? g : n))
            return;
    }
    dk_keep_mark(DK_WBB_PREV, keep);
    dk_keep_mark(DK_WBG_PREV, gkeep);
}

/* FP-2 (R1.10, OD14): ENVARC:Amarchy/desk-original/ holds, per file Amarchy may rewrite, the user's version from before the first apply
 * (ENVARC: copy, else the ENV: copy) under desk_copy_name(ENV: path); a file that did not exist is recorded by "<name>.absent". Kept until
 * "off" restores that file, so it never saves a theme as the original. */
#define DK_ORIG "ENVARC:Amarchy/desk-original"

/* S56-PALETTE-31-SAFE: below 47 a palette.prefs on disk that is not the 3.1-safe form (a 3.2 Palette save on a shared disk) is rewritten in
 * that form, after a copy to <file>.32bak (only when no such backup exists yet: the first 3.2 file stays). desk_palette_31_fix_plan decides. */
unsigned long desk_dos_pal31_checks, desk_dos_pal31_fixes;
static void dk_palette_31_fix(const char *file)
{
    static unsigned char cur[DK_BUF], out[DK_BUF];
    char bak[112];
    long len, n;
    int plan;
    desk_dos_pal31_checks++;
    len = dk_read(file, cur, sizeof cur);
    strcpy(bak, file);
    strcat(bak, ".32bak");
    plan = desk_palette_31_fix_plan(dk_palette_editor_ok(), len > 0 && len < (long)sizeof cur ? cur : 0, len, dk_exists(bak), out, sizeof out, &n);
    if ((plan & DK_P31_BACKUP) && !dk_write(bak, cur, len))
        return; /* no backup, no rewrite */
    if ((plan & DK_P31_WRITE) && dk_write(file, out, n))
        desk_dos_pal31_fixes++;
}

/* the saved original of the palette (desk-original/), the copy dk_orig_restore_arc and dk_restore_targets feed back */
static void dk_palette_31_fix_orig(void)
{
    char name[96];
    desk_copy_name(DK_ORIG, desk_target_get(DT_PALETTE)->files[1], name, sizeof name);
    dk_palette_31_fix(name);
}

/* Amarchy start: ENVARC: first (the copy a reboot reads), then ENV:, then the saved original */
static void dk_palette_31_start(void)
{
    const desk_target *t = desk_target_get(DT_PALETTE);
    dk_palette_31_fix(t->files[0]);
    dk_palette_31_fix(t->files[1]);
    dk_palette_31_fix_orig();
    am_sentinel("AMARCHY_DESK_PAL31_CHECKS", (long)desk_dos_pal31_checks);
    am_sentinel("AMARCHY_DESK_PAL31_FIXES", (long)desk_dos_pal31_fixes);
}
unsigned long desk_dos_persists, desk_dos_originals_saved, desk_dos_originals_restored, desk_dos_originals_dropped;
unsigned long desk_dos_env_fix_retries, desk_dos_env_fix_fails, desk_dos_env_fix_err;

/* streamed (a prefs file may be larger than any fixed buffer, and the copy must be byte-identical): 1 copied, 0 write failed, -1 no source */
static int dk_stream_copy(const char *src, const char *dst)
{
    static unsigned char cb[2048];
    BPTR in = Open((STRPTR)src, MODE_OLDFILE), out;
    LONG got;
    int ok = 1;
    if (!in)
        return -1;
    out = Open((STRPTR)dst, MODE_NEWFILE);
    if (!out) {
        desk_dos_env_fix_err = (unsigned long)IoErr();   /* 202: object in use (IPrefs reading the ENV: file) */
        Close(in);
        return 0;
    }
    while ((got = Read(in, cb, sizeof cb)) > 0)
        if (Write(out, cb, got) != got) {
            ok = 0;
            break;
        }
    Close(in);
    Close(out);
    return ok && got >= 0;
}

/* 1 when both files are readable and hold the same bytes */
static int dk_files_equal(const char *a, const char *b)
{
    static unsigned char ba[1024], bb[1024];
    BPTR fa = Open((STRPTR)a, MODE_OLDFILE), fb = fa ? Open((STRPTR)b, MODE_OLDFILE) : 0;
    LONG ga = 0, gb = 0;
    int eq = fa && fb;
    while (eq) {
        ga = Read(fa, ba, sizeof ba);
        gb = Read(fb, bb, sizeof bb);
        if (ga < 0 || ga != gb || (ga > 0 && memcmp(ba, bb, (size_t)ga)))
            eq = 0;
        else if (ga == 0)
            break;
    }
    if (fa)
        Close(fa);
    if (fb)
        Close(fb);
    return eq;
}

static void dk_orig_dirs(void)
{
    UnLock(CreateDir((STRPTR) "ENVARC:Amarchy"));
    UnLock(CreateDir((STRPTR)DK_ORIG));
}

static int dk_orig_have(const char *env)
{
    char name[96], ab[104];
    desk_copy_name(DK_ORIG, env, name, sizeof name);
    strcpy(ab, name);
    strcat(ab, ".absent");
    return dk_exists(name) || dk_exists(ab);
}

/* the targets (bit t) with a saved original; bit DT_ICONTROL is never set */
static unsigned dk_orig_mask(void)
{
    const char *env[1], *arc[1];
    unsigned m = 0;
    int t;
    for (t = 0; t < DT_COUNT; t++)
        if (t != DT_ICONTROL && desk_persist_pairs(1u << t, 0, env, arc, 1) == 1 && dk_orig_have(env[0]))
            m |= 1u << t;
    return m;
}

/* before the first write of each file: save the user's version once. The caller passes what it is about to write. */
static void dk_orig_save(unsigned mask, int wbprefs)
{
    const char *env[8], *arc[8];
    char name[96], ab[104];
    int i, n = desk_persist_pairs(mask, wbprefs, env, arc, 8);
    for (i = 0; i < n; i++) {
        if (dk_orig_have(env[i]))
            continue;
        dk_orig_dirs();
        desk_copy_name(DK_ORIG, env[i], name, sizeof name);
        strcpy(ab, name);
        strcat(ab, ".absent");
        if (dk_stream_copy(arc[i], name) < 0 && dk_stream_copy(env[i], name) < 0)
            dk_write(ab, (const unsigned char *)"1", 1);
        desk_dos_originals_saved++;
    }
}

/* the original of one file pair goes back over its ENVARC: copy (copied or deleted); the copy in desk-original/ stays until dk_orig_forget */
static void dk_orig_restore_arc(const char *env, const char *arc)
{
    char name[96];
    desk_copy_name(DK_ORIG, env, name, sizeof name);
    if (!strcmp(env, desk_target_get(DT_PALETTE)->files[1]))
        dk_palette_31_fix(name); /* S56-PALETTE-31-SAFE: a 3.2-saved original goes back to ENVARC: in the safe form */
    if (dk_stream_copy(name, arc) < 0)
        DeleteFile((STRPTR)arc);
    desk_dos_originals_restored++;
}

/* the restored original is forgotten: a later apply saves the file anew */
static void dk_orig_forget(const char *env)
{
    char name[96], ab[104];
    desk_copy_name(DK_ORIG, env, name, sizeof name);
    strcpy(ab, name);
    strcat(ab, ".absent");
    DeleteFile((STRPTR)name);
    DeleteFile((STRPTR)ab);
}

/* DESK-STALE-ORIG: an original lives only while its target's ENV: is themed. A Revert (user, timeout, boot marker) puts ENV: back to what it was before
 * the apply; a target whose ENVARC: or ENV: file equals its saved original (ENV: is the stock editor's rewrite, so ENVARC: is the stable witness; or ENV: is absent as the original was) was not themed, so the original goes. A revert to
 * an earlier theme leaves ENV: different from the original, and the original stays. */
static void dk_orig_drop_reverted(unsigned mask, int wbprefs)
{
    const char *env[8], *arc[8];
    char name[96];
    int i, n = desk_persist_pairs(mask, wbprefs, env, arc, 8);   /* icontrol is never a pair; Workbench.prefs when dk_wb_restore put it back */
    for (i = 0; i < n; i++) {
        if (!dk_orig_have(env[i]))
            continue;
        desk_copy_name(DK_ORIG, env[i], name, sizeof name);
        if (dk_exists(name) ? ((dk_exists(arc[i]) && dk_files_equal(name, arc[i])) || (dk_exists(env[i]) && dk_files_equal(name, env[i]))) : !dk_exists(env[i])) {
            dk_orig_forget(env[i]);
            desk_dos_originals_dropped++;
        }
    }
}

/* FLAT-PENS-STALE-FILE part 2: Amarchy's start. Called from main after the hooks desk_dos_wb_close/own_close are installed, so the write
 * path (dk_flat_pens: dk_wait_reset, the bar close/reopen around a Workbench reset) is the one a theme pick uses. */
int desk_dos_flat_pens_refresh(void)
{
    am_prefs *ap;
    unsigned char pens[DK_SLOTS];
    gtheme th;
    int flat, done;
    ap = amconf_borrow();
    flat = amconf_desk_flat_pens(ap);
    amconf_return(ap);                  /* flat is all it reads */
    if (!flat || !dk_palette_editor_ok())
        return 0;
    desk_pens_for(flat, pens);
    if (!dk_palette_file_stale(flat, pens))
        return 0;
    gt_dos_load_current(&th);
    dk_live_want = 0;
    done = dk_flat_pens(&th);
    if (dk_live_want)
        dk_live_pens(dk_live_want > 0 ? &th : 0, dk_live_flat);
    return done;
}

int desk_dos_apply(const gtheme *th, char *why, int cap)
{
    int r;
    dk_live_want = 0;
    r = dk_apply(th, why, cap);
    if (!(r > 0 && (r & (1 << DT_PALETTE))) && dk_flat_pens(th) && r >= 0)
        r |= 1 << DT_PALETTE;   /* the theme colours are in the palette and DOpus has them: no wb_pens_apply */
    if (r >= 0) {
        /* after dk_flat_pens: its flat-off palette restore writes palette.prefs from the live screen when the original is gone, and the
         * originals must have the last word on the ENV: files (FP-2) */
        am_prefs *ap;
        ap = amconf_borrow();
        dk_orig_off(amconf_desk_mask(ap), amconf_desk_flat_pens(ap));
        amconf_return(ap);
    }
    dk_wbborder();
    if (r <= 0 && !dk_pending_mask())
        DeleteFile((STRPTR)DK_WBPEND);   /* nothing to confirm: the Workbench.prefs snapshot has no owner */
    if (dk_live_want)
        dk_live_pens(dk_live_want > 0 ? th : 0, dk_live_flat);
    dk_flat_sync();          /* also when no desk target changed: flipping a flat switch alone must reach the block */
    am_sentinel("AMARCHY_DESK_APPLY_R", (long)r);
    am_sentinel("AMARCHY_DESK_ENV_FIX_RETRIES", (long)desk_dos_env_fix_retries);
    am_sentinel("AMARCHY_DESK_ENV_FIX_FAILS", (long)desk_dos_env_fix_fails);
    am_sentinel("AMARCHY_DESK_ENV_FIX_ERR", (long)desk_dos_env_fix_err);
    am_sentinel("AMARCHY_DESK_CONFIRM_ASKS", (long)desk_dos_confirm_asks);
    if (r > 0)
        dk_confirm_ask((unsigned)r);
    return r;
}

/* FP-1 (R1.8): the 15 s confirm after an apply. The commodity (desk_confirm_dos.c) owns the requester and the timer: it installs
 * desk_dos_confirm_begin; every other process sends DESK CONFIRM <mask> to it. Keep calls desk_dos_confirmed, Revert and the timeout
 * call desk_dos_revert. */
void (*desk_dos_confirm_begin)(unsigned mask);
unsigned long desk_dos_confirm_asks, desk_dos_reverts, desk_dos_keeps;

/* decimal text of v into out (no terminator); returns the length */
static long dk_num(unsigned v, unsigned char *out)
{
    long n = 0, d;
    for (d = 1000000000L; d >= 1; d /= 10)
        if (n || v / (unsigned)d)
            out[n++] = (unsigned char)('0' + (v / (unsigned)d) % 10);
    if (!n)
        out[n++] = '0';
    return n;
}

/* FP-3 (R1.9): T: is RAM, so an unconfirmed apply must outlive a reboot. When the confirm starts, the snapshot copies of the pending targets (and
 * of the Workbench.prefs write) are copied to ENVARC:Amarchy/pending-revert.d/ under their T: names, then ENVARC:Amarchy/pending-revert is written
 * (desk_marker_make: the target names) last, so a half-copied set has no marker. Keep and a finished Revert delete both; Amarchy start finds a
 * leftover (desk_dos_marker_recover) and reverts from it. ENV: is rebuilt from ENVARC: at boot and the apply writes ENVARC: only for icontrol, so
 * the copy of ENVARC:Sys/icontrol.prefs is the one that changes the outcome; the rest is restored from the same set for fidelity. */
#define DK_MARK "ENVARC:Amarchy/pending-revert"
#define DK_MARK_DIR "ENVARC:Amarchy/pending-revert.d"
unsigned long desk_dos_marker_reverts;
static const char *const dk_mark_extra[4] = { "wbpending", "wbborder.snap", "wbgauge.snap", "prevtheme" };

/* copy every member of the set between the T: snapshot dir and the marker dir; absent source = absent copy (a missing snapshot means "did not exist") */
static void dk_marker_copy(unsigned mask, int to_marker)
{
    const char *list[16];
    char a[96], b[96];
    int i, n = desk_pending_files(mask, 1, list, 16);
    for (i = 0; i < n + 4; i++) {
        if (i < n) {
            desk_copy_name(DK_REVERT, list[i], a, sizeof a);
            desk_copy_name(DK_MARK_DIR, list[i], b, sizeof b);
        } else {
            desk_copy_name(DK_REVERT, dk_mark_extra[i - n], a, sizeof a);
            desk_copy_name(DK_MARK_DIR, dk_mark_extra[i - n], b, sizeof b);
        }
        if (to_marker ? dk_stream_copy(a, b) < 0 : dk_stream_copy(b, a) < 0)
            DeleteFile((STRPTR)(to_marker ? b : a));
    }
}

static void dk_marker_write(unsigned mask)
{
    char text[128];
    long len;
    UnLock(CreateDir((STRPTR) "ENVARC:Amarchy"));
    UnLock(CreateDir((STRPTR)DK_MARK_DIR));
    dk_marker_copy(mask, 1);
    len = desk_marker_make(mask, text, sizeof text);
    if (len > 0)
        dk_write(DK_MARK, (const unsigned char *)text, len);
}

static unsigned dk_marker_mask(void)
{
    static unsigned char b[128];
    long n = dk_read(DK_MARK, b, sizeof b);
    return n > 0 ? desk_marker_parse((const char *)b, n) : 0;
}

static void dk_marker_clear(void)
{
    const char *list[16];
    char b[96];
    unsigned mask = dk_marker_mask();
    int i, n;
    DeleteFile((STRPTR)DK_MARK);   /* first: a crash while deleting leaves no marker that points at half a set */
    n = desk_pending_files(mask, 1, list, 16);
    for (i = 0; i < n + 4; i++) {
        desk_copy_name(DK_MARK_DIR, i < n ? list[i] : dk_mark_extra[i - n], b, sizeof b);
        DeleteFile((STRPTR)b);
    }
    DeleteFile((STRPTR)DK_MARK_DIR);
}

static void dk_confirm_ask(unsigned mask)
{
    char num[16], res[32];
    unsigned char out[16];
    unsigned all = mask | dk_pending_mask();
    long n = dk_num(all, out);
    out[n] = 0;
    UnLock(CreateDir((STRPTR)DK_REVERT));
    dk_write(DK_PENDING, out, n);
    dk_marker_write(all);
    desk_dos_confirm_asks++;
    if (desk_dos_confirm_begin) {
        desk_dos_confirm_begin(all);
        return;
    }
    strcpy(num, "DESK CONFIRM ");
    strcat(num, (const char *)out);
    rexx_send("AMARCHY", num, res, sizeof res);
}

/* Keep: the prefs the apply wrote to ENV: are persisted to ENVARC: now, not before (R1.10). Workbench.prefs is outside the target mask (WBPENDING). */
void desk_dos_confirmed(void)
{
    const char *env[8], *arc[8];
    int i, n = desk_persist_pairs(dk_pending_mask(), dk_exists(DK_WBPEND), env, arc, 8);
    for (i = 0; i < n; i++)
        if (dk_stream_copy(env[i], arc[i]) > 0)
            desk_dos_persists++;
    dk_marker_clear();
    DeleteFile((STRPTR)DK_PENDING);
    DeleteFile((STRPTR)DK_WBPEND);
    DeleteFile((STRPTR)DK_PREVTHEME);
    DeleteFile((STRPTR)DK_WBB_SNAP);
    DeleteFile((STRPTR)DK_WBG_SNAP);
    desk_dos_keeps++;
}

/* The stock editors rewrite what they are fed (Pointer) and IPrefs/the palette editor recreate files that were deleted: whatever ran, the ENV: file
 * ends byte-equal to its copy in dir, or absent when there was none. Shared by Revert (desk_dos_revert) and "off" (dk_orig_off). */
struct dk_fix { char name[96]; const char *env; };
static int dk_fix_want(void *ud) { return dk_exists(((struct dk_fix *)ud)->name); }
static int dk_fix_got(void *ud) { return dk_exists(((struct dk_fix *)ud)->env); }
static int dk_fix_equal(void *ud) { return dk_files_equal(((struct dk_fix *)ud)->name, ((struct dk_fix *)ud)->env); }
static void dk_fix_copy(void *ud) { dk_stream_copy(((struct dk_fix *)ud)->name, ((struct dk_fix *)ud)->env); }
static void dk_fix_del(void *ud) { DeleteFile((STRPTR)((struct dk_fix *)ud)->env); }
static void dk_fix_pause(void *ud) { (void)ud; Delay(DESK_IPREFS_POLL_TICKS); }

/* checked and retried (desk_env_fix_run): IPrefs reading the file after the editor's USE refuses the open, which left the editor's rewrite */
static void dk_env_fix_file(const char *dir, const char *env)
{
    static const desk_fix_ops ops = { dk_fix_want, dk_fix_got, dk_fix_equal, dk_fix_copy, dk_fix_del, dk_fix_pause };
    struct dk_fix f;
    int r;
    f.env = env;
    desk_copy_name(dir, env, f.name, sizeof f.name);
    r = desk_env_fix_run(&ops, &f, DK_RESET_SECS * 50 / DESK_IPREFS_POLL_TICKS);
    if (r < 0)
        desk_dos_env_fix_fails++;
    else if (r > 1)
        desk_dos_env_fix_retries += (unsigned long)(r - 1);
}

/* put the files of mask back from the copies in dir (T:amarchy-revert for Revert, the originals for "off"): each target's ENV: file is fed from
 * its copy through the stock editor (as the apply did), a file that did not exist then is deleted; icontrol goes through dk_icontrol_restore. */
static void dk_restore_targets(unsigned mask, const char *dir)
{
    static const struct { int t; const char *editor; const char *file; } ed[] = {
        { DT_FONT, "Font", "ENV:Sys/font.prefs" }, { DT_POINTER, "Pointer", "ENV:Sys/pointer.prefs" },
        { DT_WBPATTERN, "WBPattern", "ENV:Sys/wbpattern.prefs" }, { DT_PALETTE, "Palette", "ENV:Sys/palette.prefs" } };
    static unsigned char buf[8192];
    am_prefs *ap;
    unsigned long idle = 0;
    struct Screen *before = 0;
    int i, was = 0, pal = (mask >> DT_PALETTE) & 1, flat_pens;
    unsigned dmask;
    char name[96];
    ap = amconf_borrow();
    flat_pens = amconf_desk_flat_pens(ap);
    dmask = amconf_desk_mask(ap);
    amconf_return(ap);                  /* both values read once: the restore below is long DOS work */
    if (mask & ((1u << DT_FONT) | (1u << DT_POINTER) | (1u << DT_ICONTROL)))
        idle = dk_iprefs_idle_mask();
    if (pal) {
        before = dk_wb_screen(0, 0);
        was = dk_windows_close();
    }
    for (i = 0; i < 4; i++) {
        long len;
        if (!((mask >> ed[i].t) & 1) || (ed[i].t == DT_POINTER && !dk_pointer_editor_ok()))
            continue;
        desk_copy_name(dir, ed[i].file, name, sizeof name);
        if (ed[i].t == DT_PALETTE)
            dk_palette_31_fix(name); /* S56-PALETTE-31-SAFE: the copy the editor and dk_env_fix_file put into ENV: */
        len = dk_read(name, buf, 1);
        if (len < 0)
            DeleteFile((STRPTR)ed[i].file);
        else
            dk_editor(ed[i].editor, name);
        if (ed[i].t == DT_PALETTE)
            dk_wait_reset(before, flat_pens, dk_screen_depth());
        else if (ed[i].t == DT_FONT || ed[i].t == DT_POINTER)
            dk_wait_iprefs(idle);   /* DESK-PTR-REVERT: IPrefs reads the editor's ENV: write on its notify; a fix copy opened meanwhile fails (object in use) */
        dk_env_fix_file(dir, ed[i].file);
    }
    if ((mask >> DT_ICONTROL) & 1) {
        dk_icontrol_restore();
        dk_wait_iprefs(idle);
    }
    if (pal) {
        int rp[4], use = -1;
        uc_u32 rc[4];
        if (dk_rv_theme)   /* a Revert: pens 4-7 come from the previous theme (desk_revert_role_pens), the baseline only when it says so */
            use = desk_revert_role_pens(dk_rv_theme, flat_pens, (dmask >> DT_PALETTE) & 1, dk_screen_depth(), rp, rc);
        dk_live_pens(use >= 0 ? dk_rv_theme : 0, use >= 0 ? flat_pens : 0);
        dk_windows_open(was);
    }
}

/* the recorded previous theme name of the snapshot (T:amarchy-revert/prevtheme); 0 when there is none (an older build's snapshot) */
static int dk_revert_theme_name(char *name, int cap)
{
    unsigned char b[GT_NAME + 24];
    long n = dk_read(DK_PREVTHEME, b, sizeof b);
    return n > 0 && desk_revert_theme_parse((const char *)b, n, name, cap);
}

void desk_dos_revert(void)
{
    unsigned mask = dk_pending_mask();
    int wb;
    gtheme prev;
    char name[GT_NAME];
    desk_dos_reverts++;
    DeleteFile((STRPTR)DK_PENDING);
    wb = dk_wb_restore();
    if (dk_revert_theme_name(name, sizeof name)) {   /* the prefs theme line goes back first: the pens below follow the previous theme */
        am_prefs *ap;
        int failed = 0;
        ap = amconf_borrow();
        if (strcmp(ap->val[AM_GTHEME], name)) {
            strcpy(ap->val[AM_GTHEME], name);
            amconf_save(ap, 1, &failed);     /* the writer the pick uses: ENV: and ENVARC: */
        }
        gt_dos_load(name, &prev);
        dk_rv_theme = &prev;
        amconf_return(ap);
    }
    if (mask)
        dk_restore_targets(mask, DK_REVERT);
    dk_rv_theme = 0;
    if (mask || wb)
        dk_orig_drop_reverted(mask, wb);
    dk_marker_clear();   /* last: a crash during the revert leaves the marker for the next start */
    dk_flat_sync();
}

/* Amarchy start (R1.9): a marker left behind means an apply never reached Keep or Revert. T:amarchy-revert/pending still there: Amarchy died, the
 * snapshot is intact. Gone: the machine rebooted, so the snapshot is rebuilt in T: from the marker dir first. Then the ordinary revert runs. */
int desk_dos_marker_recover(void)
{
    unsigned char num[16];
    unsigned mask = dk_marker_mask();
    long n;
    if (!dk_exists(DK_MARK))
        return 0;
    if (mask && !dk_exists(DK_PENDING)) {
        UnLock(CreateDir((STRPTR)DK_REVERT));
        dk_marker_copy(mask, 0);
        n = dk_num(mask, num);
        dk_write(DK_PENDING, num, n);
    }
    desk_dos_marker_reverts++;
    desk_dos_revert();
    /* published by the process that recovers (Amarchy, at its start): the menu's copy of the counter stays 0 */
    am_sentinel("AMARCHY_DESK_MARKER_REVERTS", (long)desk_dos_marker_reverts);
    am_sentinel("AMARCHY_DESK_ORIG_DROPPED", (long)desk_dos_originals_dropped);
    return 1;
}

/* OD14: a desktop part the settings no longer apply (or the flat Workbench.prefs when flat is off) goes back to the user's original, in ENV: through
 * its editor and in ENVARC: by copy; the original is forgotten, a later apply saves the file anew. */
static void dk_orig_off(unsigned desk_mask, int flat)
{
    const char *env[8], *arc[8];
    char name[96];
    unsigned off = desk_off_mask(dk_orig_mask(), desk_mask);
    int wb = !flat && dk_orig_have(DESK_WBPREFS_ENV), wbfix = 0, i, n;
    if (!off && !wb)
        return;
    /* ENVARC: first: a reader that finds ENV: missing falls back to ENVARC:, which must already hold the original */
    n = desk_persist_pairs(off, wb, env, arc, 8);
    for (i = 0; i < n; i++)
        dk_orig_restore_arc(env[i], arc[i]);
    if (off)
        dk_restore_targets(off, DK_ORIG);
    if (wb) {
        desk_copy_name(DK_ORIG, DESK_WBPREFS_ENV, name, sizeof name);
        wbfix = dk_exists(name);
        if (wbfix)
            dk_editor("Workbench", name);
        else
            dk_wb_absent();
        DeleteFile((STRPTR)DK_WBB_PREV);
        DeleteFile((STRPTR)DK_WBG_PREV);
    }
    /* dk_restore_targets already fixed the editor targets; Workbench.prefs (wb) is the one it does not cover. Only a saved file is fixed: for an
     * absent original the fix would delete the stock ENV: file before IPrefs read it (dk_wb_absent) */
    if (wbfix)
        dk_env_fix_file(DK_ORIG, DESK_WBPREFS_ENV);
    for (i = 0; i < n; i++)
        dk_orig_forget(env[i]);
    DeleteFile((STRPTR)DK_ORIG);   /* the directory, once empty */
}

/* S31-START-APPLY (rig 55, 2026-11-02): Amarchy's start makes ENV: match the prefs it starts with, the same writes a theme pick makes
 * (dk_flat_pens, the theme backdrop of dk_apply, dk_wbborder) but only the parts desk_start_plan finds stale, so a start where ENV: already
 * matches runs no editor and no IPrefs reset. No confirm requester: nothing changed by hand here, and a timeout must not revert prefs the user
 * set. The originals are saved first (dk_orig_save), so turning the look off still restores the user's own files. */
unsigned long desk_dos_start_plans, desk_dos_start_writes;

/* S32-WB-BACKDROP: the root window's flags (the stock Workbench window spanning the screen, bar_is_desktop), DESK_ROOT_NONE / DESK_ROOT_WB (flags set) / DESK_ROOT_FOREIGN (a desktop window that is not a Workbench window, e.g. Directory Opus). */
static int dk_wb_root_flags(unsigned long *flags)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct Screen *s;
    int found = DESK_ROOT_NONE;
    if (!IntuitionBase)
        return DESK_ROOT_NONE;
    if ((s = ipd_lock_wb_screen()) != 0) {
        ULONG il = LockIBase(0);
        struct Window *w;
        for (w = s->FirstWindow; w; w = w->NextWindow)
            if (bar_is_desktop((unsigned long)w->Flags, w->LeftEdge, w->LeftEdge + w->Width - 1, s->Width)) {
                *flags = (unsigned long)w->Flags;
                found = (w->Flags & BAR_WFLG_WBENCHWINDOW) ? DESK_ROOT_WB : DESK_ROOT_FOREIGN;
                if (found == DESK_ROOT_WB)
                    break;
            }
        UnlockIBase(il);
        UnlockPubScreen(0, s);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    return found;
}

/* S57-START-POINTER-HANG: Workbench serves its ARexx port from its main loop, so an answer to a round-trip means LoadWB's
 * startup pass (icons read under icon.library's lock, windows drawn under the layer locks) is over. No Workbench task:
 * nothing to collide with (another program opened the screen). No rexxsyslib.library: no way to ask, so do not wait for ever.
 * The round-trip waits for the reply (REXX_SEND_SECS at most); a busy Workbench answers -1 and the next tick asks again. */
unsigned long desk_dos_wb_ready_asks;

static int g_wb_idle_ticks;                     /* OS 3.1 path: 1 s ticks in a row the Workbench sat idle */

int desk_dos_wb_ready(long now)
{
    struct Library *lib;
    struct Task *wb;
    unsigned long fl = 0;
    int ver = 0, answered = 0, idle, r;
    Forbid();
    wb = FindTask((STRPTR)"Workbench");
    idle = wb && wb->tc_State == TS_WAIT && !(wb->tc_SigWait & SIGF_SINGLE);
    Permit();
    if (!wb)
        return DESK_WBR_READY;
    if ((lib = OpenLibrary((STRPTR)"workbench.library", 0))) {
        ver = lib->lib_Version;
        CloseLibrary(lib);
    }
    if (ver >= DESK_WB_AREXX_VERSION) {
        if ((lib = OpenLibrary((STRPTR)"rexxsyslib.library", 0))) {
            CloseLibrary(lib);
            desk_dos_wb_ready_asks++;
            am_sentinel("AMARCHY_WB_READY_ASK", (long)desk_dos_wb_ready_asks);
            answered = rexx_port_exists("WORKBENCH") && rexx_send("WORKBENCH", "GETATTR application.version", 0, 0) >= 0;
        } else
            ver = 0;                            /* no rexxsyslib: no way to ask, use the idle signal */
    }
    g_wb_idle_ticks = idle && dk_wb_root_flags(&fl) == DESK_ROOT_WB ? g_wb_idle_ticks + 1 : 0;
    r = desk_wb_ready_state(ver, answered, g_wb_idle_ticks, now, DESK_WB_READY_TICKS);
    if (r == DESK_WBR_TIMEOUT)
        PutStr((STRPTR)"Amarchy: the Workbench did not finish its start in 30 s; applying the start prefs anyway\n");
    return r;
}

/* The root window opens a moment after the screen (LoadWB), so look for it for up to 5 s. */
unsigned long desk_dos_wb_backdrop_invokes;

int desk_dos_wb_backdrop(void)
{
    am_prefs *ap;
    unsigned long fl = 0;
    int i, state = DESK_ROOT_NONE, port, master;
    ap = amconf_borrow();
    master = amconf_desk_master(ap);
    amconf_return(ap);
    if (!master)
        return 0;
    port = rexx_port_exists("WORKBENCH");
    for (i = 0; port && i < 10 && desk_wb_root_wait(state = dk_wb_root_flags(&fl)); i++)
        Delay(25);
    if (!desk_wb_backdrop_invoke(1, port, state == DESK_ROOT_WB, fl))
        return 0;
    rexx_send("WORKBENCH", "MENU INVOKE WORKBENCH.BACKDROP", 0, 0);
    desk_dos_wb_backdrop_invokes++;
    am_sentinel("AMARCHY_WB_BACKDROP_INVOKE", (long)desk_dos_wb_backdrop_invokes);
    return 1;
}

int desk_dos_start_apply(void)
{
    static unsigned char wb[DK_BUF], pat[DK_BUF], out[DK_BUF], pal[DK_BUF], pcur[8192], pnew[8192], fcur[DK_BUF], fnew[DK_BUF];
    am_prefs *ap;
    gtheme th;
    unsigned mask, plan;
    unsigned char sv[2], pens[DK_SLOTS];
    unsigned short live[DK_SLOTS];
    int flat, depth, saved = -1, gsaved = -1, was, nlive = 0, icon_text;
    unsigned long idle = 0;
    long wblen, patlen, len, pallen, curlen, ptrlen = 0, fontlen = 0, flen;
    int ic_act = DK_IC_NONE, ic_snap = 0;
    dk_palette_31_start(); /* S56-PALETTE-31-SAFE: whatever the switches, a 3.2 palette.prefs on 3.1 is made safe first */
    ap = amconf_borrow();
    mask = amconf_desk_mask(ap);
    flat = amconf_desk_flat_pens(ap);
    icon_text = amconf_icon_text(ap);
    amconf_return(ap);                  /* mask, flat and icon_text are all it reads */
    if (!mask && !flat)
        return 0;
    gt_dos_load_current(&th);
    depth = dk_screen_depth();
    if (depth > 3)
        depth = 3;
    if (dk_read(DK_WBB_PREV, sv, 2) == 1)
        saved = sv[0] - '0';
    if (dk_read(DK_WBG_PREV, sv, 2) == 1)
        gsaved = sv[0] - '0';
    wblen = dk_read("ENV:Sys/Workbench.prefs", wb, sizeof wb);
    if (wblen < 0)
        wblen = dk_read("ENVARC:Sys/Workbench.prefs", wb, sizeof wb);
    patlen = dk_read("ENV:Sys/wbpattern.prefs", pat, sizeof pat);
    plan = desk_start_plan(mask, flat, dk_wbborder_editor_ok(), wb, wblen, saved, gsaved, &th, depth, pat, patlen);
    /* S31-START-PALETTE: the theme colours, as dk_apply's palette part writes them (dk_flat_pens leaves them to it) */
    pallen = 0;
    if ((mask >> DT_PALETTE) & 1) { /* S56-PALETTE-31-SAFE: 3.1 too, dk_put_palette writes the safe form */
        desk_pens_for(flat, pens);
        len = dk_palette_base(0, pens, dk_screen_depth(), out, sizeof out);
        curlen = dk_read("ENV:Sys/Palette.prefs", wb, sizeof wb);   /* wb is free: desk_start_plan has read it */
        pallen = desk_start_palette(mask, flat, &th, dk_screen_depth(), out, len, curlen > 0 ? wb : 0, curlen, pal, sizeof pal);
        if (pallen > 0)
            plan |= DESK_START_PALETTE;
    }
    /* S31-START-PARTS: the pointer, font and icontrol parts of a theme pick, planned like the palette (dk_apply wrote them only on a pick) */
    if (((mask >> DT_POINTER) & 1) && dk_pointer_editor_ok()) {
        curlen = dk_read("ENV:Sys/pointer.prefs", pcur, sizeof pcur);
        if (curlen < 0)
            curlen = dk_read("ENVARC:Sys/Pointer.prefs", pcur, sizeof pcur);
        ptrlen = desk_start_prefs(DT_POINTER, mask, &th, 0, 0, 0, curlen > 0 ? pcur : 0, curlen, pnew, sizeof pnew);
        if (ptrlen > 0)
            plan |= DESK_START_POINTER;
    }
    if ((mask >> DT_FONT) & 1) {
        flen = dk_read("ENV:Sys/font.prefs", fcur, sizeof fcur);
        if (flen < 0)
            flen = dk_read("ENVARC:Sys/font.prefs", fcur, sizeof fcur);
        fontlen = desk_start_prefs(DT_FONT, mask, &th, icon_text, dk_font_exists, 0, flen > 0 ? fcur : 0, flen, fnew, sizeof fnew);
        if (fontlen > 0)
            plan |= DESK_START_FONT;
    }
    if (dk_palette_editor_ok()) {
        static unsigned char ic_cur[DK_BUF * 8], ic_snap_buf[DK_IC_BUF];
        long ic_len = dk_read("ENV:Sys/icontrol.prefs", ic_cur, sizeof ic_cur), ic_snap_len;
        if (ic_len < 0)
            ic_len = dk_read("ENVARC:Sys/icontrol.prefs", ic_cur, sizeof ic_cur);
        ic_snap_len = dk_read_icontrol_snapshot(ic_snap_buf, sizeof ic_snap_buf);
        ic_act = desk_start_icontrol(mask, &th, ic_cur, ic_len, ic_snap_buf, ic_snap_len, &ic_snap);
        if (ic_act != DK_IC_NONE)
            plan |= DESK_START_ICONTROL;
    }
    desk_dos_start_plans++;
    am_sentinel("AMARCHY_DESK_START_PLAN", (long)plan);
    dk_live_want = 0;
    /* rig 56: the start writes made IPrefs reset the Workbench screen with the bar open ("Intuition is attempting to reset the
     * Workbench screen. Please close the..."), so our windows close around them, as dk_apply closes them around a reset. With
     * plan 0 the palette already matches (a fresh ENV: always plans the WB bit when flat), so dk_flat_pens writes nothing. */
    was = -1;
    if (plan) {
        idle = dk_iprefs_idle_mask();
        desk_dos_wb_closes++;
        was = dk_windows_close();
    }
    if (plan & DESK_START_PALETTE) {
        struct Screen *before = dk_wb_screen(live, &nlive);
        dk_orig_save(1u << DT_PALETTE, 0);
        UnLock(CreateDir((STRPTR)DK_WORK));
        if (dk_put_palette(pal, pallen)) {
            desk_dos_start_writes++;
            if (desk_pens_reset(live, nlive, flat, dk_screen_depth()))
                dk_wait_reset(before, flat, dk_screen_depth());
        }
        dk_live_want = 1;
        dk_live_flat = flat;
        desk_dos_dopus_sends++;
        dopus_theme_send(&th, dk_dopus_send, 0, &dopustb_dos_ops);
    }
    if (plan & DESK_START_POINTER) {
        dk_orig_save(1u << DT_POINTER, 0);
        UnLock(CreateDir((STRPTR)DK_WORK));
        if (dk_put("Pointer", DK_WORK "/pointer.prefs", pnew, ptrlen))
            desk_dos_start_writes++;
    }
    if (plan & DESK_START_FONT) {
        dk_orig_save(1u << DT_FONT, 0);
        UnLock(CreateDir((STRPTR)DK_WORK));
        if (dk_put("Font", DK_WORK "/font.prefs", fnew, fontlen))
            desk_dos_start_writes++;
        dk_wait_iprefs(idle);   /* the Font editor resets the Workbench screen too */
    }
    if (plan & DESK_START_ICONTROL) {
        dk_orig_save(1u << DT_ICONTROL, 0);
        if (ic_snap)
            dk_snapshot(1u << DT_ICONTROL);
        UnLock(CreateDir((STRPTR)DK_WORK));
        if (ic_act == DK_IC_RESTORE ? dk_icontrol_restore() : dk_icontrol(1, &th))
            desk_dos_start_writes++;
        dk_wait_iprefs(idle);
    }
    if (flat)
        dk_flat_pens(&th);   /* idempotent: writes palette / own backdrop only when they differ */
    if (plan & DESK_START_PATTERN) {
        len = desk_wbpattern(&th, depth, patlen > 0 ? pat : 0, patlen > 0 ? patlen : 0, out, sizeof out);
        dk_orig_save(1u << DT_WBPATTERN, 0);
        UnLock(CreateDir((STRPTR)DK_WORK));
        if (dk_put("WBPattern", DK_WORK "/wbpattern.prefs", out, len))
            desk_dos_start_writes++;
    }
    if (plan & DESK_START_WB) {
        dk_wbborder();
        desk_dos_start_writes++;
        if (!dk_pending_mask())
            DeleteFile((STRPTR)DK_WBPEND);   /* no confirm owns the Workbench.prefs snapshot */
    }
    if (plan) {
        dk_wait_iprefs(idle);
        dk_windows_open(was);
    }
    if (dk_live_want)
        dk_live_pens(dk_live_want > 0 ? &th : 0, dk_live_flat);
    am_sentinel("AMARCHY_DESK_START_WRITES", (long)desk_dos_start_writes);
    return (int)plan;
}
