/* desk_dos: see desk_dos.h. */
#include <string.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "desk_dos.h"
#include "rexx_host.h"
#include "desk_core.h"
#include "menu_theme.h"
#include "amconf_dos.h"

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
    strcpy(out, DK_REVERT "/");
    out += strlen(out);
    for (; *file; file++)
        *out++ = (*file == ':' || *file == '/') ? '-' : *file;
    *out = 0;
}

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
}

/* run one stock editor on a prefs file, USE: ENV: only, no window left behind */
static void dk_editor(const char *editor, const char *file)
{
    char cmd[160];
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE), out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    strcpy(cmd, "SYS:Prefs/");
    strcat(cmd, editor);
    strcat(cmd, " FROM ");
    strcat(cmd, file);
    strcat(cmd, " USE");
    SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, TAG_DONE);
    if (in)
        Close(in);
    if (out)
        Close(out);
}

static int dk_screen_depth(void)
{
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    struct Screen *scr;
    int d = 2;
    if (IntuitionBase && (scr = LockPubScreen((STRPTR)"Workbench")) != 0) {
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
static int dk_icontrol(int thin)
{
    static unsigned char cur[DK_IC_BUF], out[DK_IC_BUF];
    unsigned char geom[4];
    long len = dk_read("ENV:Sys/icontrol.prefs", cur, sizeof cur), n;
    if (len < 0)
        len = dk_read("ENVARC:Sys/icontrol.prefs", cur, sizeof cur);
    if (len >= DK_IC_BUF - 64)
        return 0;
    desk_geometry(thin, geom);
    n = desk_icontrol(len > 0 ? cur : 0, len > 0 ? len : 0, geom, out, sizeof out);
    return n > 0 && dk_write("ENVARC:Sys/icontrol.prefs", out, n) && dk_write("ENV:Sys/icontrol.prefs", out, n);
}

static int dk_pointer_editor_ok(void)
{
    static unsigned char img[65536];
    long len = dk_read("SYS:Prefs/Pointer", img, sizeof img);
    return len > 0 && desk_pointer_editor_ok(img, len);
}

/* may SYS:Prefs/Palette write ENV:Sys/palette.prefs: version 47+ (desk_palette_editor_ok). On 3.1 (Palette 40.2, IPrefs 40.7)
 * the write freezes the guest (DESK-31-CRASH) */
static int dk_palette_editor_ok(void)
{
    static unsigned char img[65536];
    long len = dk_read("SYS:Prefs/Palette", img, sizeof img);
    return len > 0 && desk_palette_editor_ok(img, len);
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
    if (IntuitionBase && (s = LockPubScreen((STRPTR)"Workbench")) != 0) {
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
static void dk_wait_reset(struct Screen *before)
{
    int i;
    for (i = 0; i < DK_RESET_SECS * 4; i++) {
        unsigned short live[DK_SLOTS];
        int n;
        struct Screen *now = dk_wb_screen(live, &n);
        if (desk_reset_done(before, now, live, n))
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

int desk_dos_apply(const gtheme *th, char *why, int cap)
{
    static unsigned char buf[DK_BUF], prev[DK_BUF], big[8192];
    am_prefs ap;
    unsigned mask;
    int depth, plan, ok = 1, old_ws = 0;
    long len, plen;
    struct Screen *wb_before = 0;
    unsigned long idle = 0;
    unsigned short live[DK_SLOTS];
    int nlive = 0, was = 0, reset = 0;
    amconf_load(&ap);
    plan = desk_plan(amconf_desk_mask(&ap), th, why, cap);
    if (plan <= 0)
        return plan;
    mask = (unsigned)plan;
    mask = desk_gate_mask(mask, dk_palette_editor_ok(), &old_ws); /* the caller sets the colours live when the palette bit goes */
    if (!mask)
        return 0;
    UnLock(CreateDir((STRPTR)DK_WORK));
    if (old_ws || (mask & ((1u << DT_FONT) | (1u << DT_ICONTROL))))
        idle = dk_iprefs_idle_mask(); /* before any editor: the Wait() IPrefs returns to when done */
    dk_snapshot(mask);
    depth = dk_screen_depth();
    if (mask & (1u << DT_PALETTE)) {
        wb_before = dk_wb_screen(live, &nlive);
        reset = desk_pens_reset(live, nlive);
    }
    if (reset || old_ws) {
        desk_dos_wb_closes++;
        was = dk_windows_close();
    }
    desk_dos_applies++;
    desk_dos_last_mask = mask;
    if (mask & (1u << DT_PALETTE)) {
        unsigned char stock_pens[DK_SLOTS];
        desk_pens(stock_pens); /* the flat map (desk_pens_flat) is wired in by FLAT-P1-2 */
        len = desk_palette(th, depth >= 3 ? 8 : 4, stock_pens, buf, sizeof buf);
        ok &= dk_put("Palette", DK_WORK "/palette.prefs", buf, len);
        /* IPrefs resets Workbench for the palette: the other editors run only after it finished (SYS:Prefs/Pointer
         * raised a Program Error requester when started under the running reset) */
        if (reset)
            dk_wait_reset(wb_before);
        desk_dos_dopus_sends++;
        dopus_theme_send(th, dk_dopus_send, 0);
    }
    if (mask & (1u << DT_WBPATTERN)) {
        plen = dk_read("ENV:Sys/wbpattern.prefs", prev, sizeof prev);
        len = desk_wbpattern(th, depth > 3 ? 3 : depth, 1, plen > 0 ? prev : 0, plen > 0 ? plen : 0, buf, sizeof buf);
        ok &= dk_put("WBPattern", DK_WORK "/wbpattern.prefs", buf, len);
        if (old_ws)
            dk_wait_iprefs(idle);
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
        if (len > 0 && desk_font_pens(buf, len) >= 0 && desk_font_name(th, buf, len, dk_font_exists, 0) >= 0) {
            ok &= dk_put("Font", DK_WORK "/font.prefs", buf, len);
            if (reset || old_ws)
                dk_wait_iprefs(idle); /* the Font editor resets the Workbench screen too (3.1: twice): the windows return after it */
        } else
            ok = 0;
    }
    if (mask & (1u << DT_ICONTROL)) {
        /* FLAT-P1-2: the file IPrefs watches is written in place, both copies, other fields and IEXC chunks kept (no editor is fed it:
         * SYS:Prefs/IControl would rewrite the exception list); then the same idle wait as the other prefs */
        ok &= dk_icontrol(amconf_flat_index(&ap, AM_FLATSIZE));
        dk_wait_iprefs(idle);
    }
    if (reset || old_ws)
        dk_windows_open(was);
    return ok ? (int)mask : -1;
}
