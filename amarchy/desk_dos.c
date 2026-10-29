/* desk_dos: see desk_dos.h. */
#include <string.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "desk_dos.h"
#include "rexx_host.h"
#include "desk_core.h"
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
static int dk_pointer_editor_ok(void)
{
    static unsigned char img[65536];
    long len = dk_read("SYS:Prefs/Pointer", img, sizeof img);
    return len > 0 && desk_pointer_editor_ok(img, len);
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
        if (now != before || !desk_pens_reset(live, n))
            break;
        Delay(12);
    }
}


int desk_dos_apply(const gtheme *th, char *why, int cap)
{
    static unsigned char buf[DK_BUF], prev[DK_BUF], big[8192];
    am_prefs ap;
    unsigned mask;
    int depth, plan, ok = 1;
    long len, plen;
    struct Screen *wb_before = 0;
    unsigned short live[DK_SLOTS];
    int nlive = 0, was = 0, reset = 0;
    amconf_load(&ap);
    plan = desk_plan(amconf_desk_mask(&ap), th, why, cap);
    if (plan <= 0)
        return plan;
    mask = (unsigned)plan;
    UnLock(CreateDir((STRPTR)DK_WORK));
    dk_snapshot(mask);
    depth = dk_screen_depth();
    if (mask & (1u << DT_PALETTE)) {
        wb_before = dk_wb_screen(live, &nlive);
        reset = desk_pens_reset(live, nlive);
    }
    if (reset) {
        desk_dos_wb_closes++;
        was = dk_windows_close();
    }
    desk_dos_applies++;
    desk_dos_last_mask = mask;
    if (mask & (1u << DT_PALETTE)) {
        len = desk_palette(th, depth >= 3 ? 8 : 4, buf, sizeof buf);
        ok &= dk_put("Palette", DK_WORK "/palette.prefs", buf, len);
        /* IPrefs resets Workbench for the palette: the other editors run only after it finished (SYS:Prefs/Pointer
         * raised a Program Error requester when started under the running reset) */
        if (reset)
            dk_wait_reset(wb_before);
    }
    if (mask & (1u << DT_WBPATTERN)) {
        plen = dk_read("ENV:Sys/wbpattern.prefs", prev, sizeof prev);
        len = desk_wbpattern(th, depth > 3 ? 3 : depth, 1, plen > 0 ? prev : 0, plen > 0 ? plen : 0, buf, sizeof buf);
        ok &= dk_put("WBPattern", DK_WORK "/wbpattern.prefs", buf, len);
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
        if (len > 0 && desk_font_pens(buf, len) >= 0 && desk_font_name(th, buf, len, dk_font_exists, 0) >= 0)
            ok &= dk_put("Font", DK_WORK "/font.prefs", buf, len);
        else
            ok = 0;
    }
    if (reset)
        dk_windows_open(was);
    return ok ? (int)mask : -1;
}
