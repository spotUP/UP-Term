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
#include "dopusenv_core.h"
#include "dopustb_bank_core.h"
#include "desk_dos.h"
#include "menu_font.h"
#include "rexx_host.h"
#include "config/amconf_core.h"
#include "config/amconf_dos.h"
#include "handler/pubscr.h"

#define DTD_DRAWER "DOpus5:Images/Patina/"

extern struct GfxBase *GfxBase;      /* each Patina program owns its own, opened or not: see dtd_screen */

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
    scr = dt_screen_name(reply, name, sizeof name) ? ipd_lock_pub_screen(name) : 0;   /* guarded: never reopens a closed WB */
    if (!scr)
        scr = ipd_lock_default_pub_screen();
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

#define DTD_ARROW_DIR "DOpus5:Images/"
#define DTD_ARROW_ORIG_DIR "DOpus5:Images/Patina/orig"
#define DTD_ARROW_ORIG DTD_ARROW_ORIG_DIR "/"
static const char *const dtd_arrow_names[2] = { "ToolbarArrow.image", "ToolbarArrow.image_frame" };

/* Copies src over dst (whole file, under 64 KB). A missing src copies nothing. Returns 1 when written. */
static int dtd_copy(const char *src, const char *dst)
{
    BPTR in = Open((STRPTR)src, MODE_OLDFILE), out;
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    uint8_t *buf = 0;
    int done = 0;
    if (in && fib && ExamineFH(in, fib) && fib->fib_Size > 0 && fib->fib_Size < 65536L &&
        (buf = (uint8_t *)AllocVec((ULONG)fib->fib_Size, MEMF_PUBLIC)) != 0 && Read(in, buf, fib->fib_Size) == fib->fib_Size &&
        (out = Open((STRPTR)dst, MODE_NEWFILE)) != 0) {
        done = Write(out, buf, fib->fib_Size) == fib->fib_Size;
        Close(out);
    }
    if (buf)
        FreeVec(buf);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (in)
        Close(in);
    return done;
}

static int dtd_exists(const char *path)
{
    BPTR l = Lock((STRPTR)path, ACCESS_READ);
    if (l)
        UnLock(l);
    return l != 0;
}

/* dir/name joined into out (160 bytes). */
static void dtd_join(char *out, const char *dir, const char *name)
{
    strcpy(out, dir);
    strcat(out, name);
}

/* Both arrow files: from(dir) over to(dir). Requesters off (pr_WindowPtr -1) for a missing volume. */
static int dtd_arrow_copy(const char *from, const char *to, int save_orig)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR old = me->pr_WindowPtr;
    char a[160], b[160], o[160];
    int n, w = 0;
    me->pr_WindowPtr = (APTR)-1;
    if (save_orig)
        UnLock(CreateDir((STRPTR)DTD_ARROW_ORIG_DIR));       /* fails when the drawer exists, which is fine */
    for (n = 0; n < 2; n++) {
        dtd_join(a, from, dtd_arrow_names[n]);
        dtd_join(b, to, dtd_arrow_names[n]);
        if (!dtd_exists(a))
            continue;
        if (save_orig) {       /* the stock file is saved once; an existing backup is never touched, so it never holds a flat file */
            dtd_join(o, DTD_ARROW_ORIG, dtd_arrow_names[n]);
            if (!dtd_exists(o) && dtd_exists(b) && !dtd_copy(b, o))
                continue;
        }
        w += dtd_copy(a, b);
    }
    me->pr_WindowPtr = old;
    return w;
}

int dopustb_arrow_restore(void)
{
    return dtd_arrow_copy(DTD_ARROW_ORIG, DTD_ARROW_DIR, 0);
}

/* ---- the default lister toolbar in the DOpus environment files (plan s91 toolbar) ---- */
#define DTD_ENV_MAX 65536L
static const char *const dtd_env_files[2] = { "DOpus5:Environment/default", "DOpus5:Environment/workbench" };

/* A crash between the renames leaves name missing with name.old present (or a stray name.new): put the old file back, drop the new one. */
static void dtd_env_recover(const char *name, const char *tmp, const char *old)
{
    if (!dtd_exists(name) && dtd_exists(old))
        Rename((STRPTR)old, (STRPTR)name);
    if (dtd_exists(tmp))
        DeleteFile((STRPTR)tmp);
    if (dtd_exists(old) && dtd_exists(name))
        DeleteFile((STRPTR)old);
}

/* Whole file into a new AllocVec buffer of size * (1 + mul) + add bytes (caller FreeVecs); 0 when missing, empty or over DTD_ENV_MAX. */
static uint8_t *dtd_slurp(const char *path, long mul, long add, long *size)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    uint8_t *buf = 0;
    if (f && fib && ExamineFH(f, fib) && fib->fib_Size > 0 && fib->fib_Size < DTD_ENV_MAX) {
        *size = fib->fib_Size;
        buf = (uint8_t *)AllocVec((ULONG)(*size * (1 + mul) + add), MEMF_PUBLIC);
        if (buf && Read(f, buf, *size) != *size) {
            FreeVec(buf);
            buf = 0;
        }
    }
    if (f)
        Close(f);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    return buf;
}

/* name.tmp written whole, then renamed over name (an existing name is replaced only after the new file is complete). 1 when done. */
static int dtd_put(const char *name, const void *data, long n)
{
    char tmp[DE_PATHMAX + 8];
    BPTR f;
    int ok = 0;
    if (strlen(name) + 5 > sizeof tmp)
        return 0;
    strcpy(tmp, name);
    strcat(tmp, ".tmp");
    if ((f = Open((STRPTR)tmp, MODE_NEWFILE)) != 0) {
        ok = Write(f, (APTR)data, n) == n;
        Close(f);
    }
    if (ok) {
        DeleteFile((STRPTR)name);
        ok = Rename((STRPTR)tmp, (STRPTR)name) != 0;
    }
    if (!ok)
        DeleteFile((STRPTR)tmp);
    return ok;
}

/* The saved original TBAR path of one env file: a text file name + ".orig" holding the path. out "" when none. */
static void dtd_orig_get(const char *name, char *out)
{
    char rec[80];
    BPTR f;
    long n = 0;
    out[0] = 0;
    strcpy(rec, name);
    strcat(rec, ".orig");
    if ((f = Open((STRPTR)rec, MODE_OLDFILE)) != 0) {
        n = Read(f, out, DE_PATHMAX - 1);
        Close(f);
    }
    out[n > 0 ? n : 0] = 0;
    for (n = 0; out[n]; n++)
        if (out[n] == '\n' || out[n] == '\r') {
            out[n] = 0;
            break;
        }
}

/* The user's bank at orig flattened for set into the dt_bank_name file (copy). The original is only read. 1 when the copy is complete. */
static int dtd_bank_make(const char *orig, const char *set, char *copy)
{
    long len = 0, n;
    uint8_t *in, *out;
    int ok = 0;
    if (dt_bank_name(orig, set, copy, DE_PATHMAX) != 0 || (in = dtd_slurp(orig, 2, 1024, &len)) == 0)
        return 0;
    out = in + len;                                    /* the button count is not cheap to get: output cap is len * 2 + 1024 */
    n = dt_bank_flatten(in, len, set, out, len * 2 + 1024);
    if (n > 0)
        ok = dtd_put(copy, out, n);                    /* DTB_BAD / DTB_FULL: nothing written */
    FreeVec(in);
    return ok;
}

/* Rewrites TBAR of one environment file. want_restore 1: only when the current path is ours; the saved original (name.orig) goes back with
 * its record and our copy removed, else stock. want_restore 0: a custom bank is flattened for set into our copy (original saved once, never
 * written), a stock or our bank is pointed at path when it differs. The old env file stays intact until the new one is complete; on any
 * failure TBAR is left alone. Returns 1 when changed. */
static int dtd_env_one(const char *name, const char *path, const char *set, int want_restore)
{
    char tmp[64], old[64], rec[80], cur[DE_PATHMAX], saved[DE_PATHMAX], copy[DE_PATHMAX];
    BPTR f;
    uint8_t *in = 0, *out;
    const char *orig = 0, *prev = 0, *target = path;
    long len = 0, n;
    int done = 0, made = 0;
    if (strlen(name) + 5 > sizeof tmp)
        return 0;
    strcpy(tmp, name);
    strcat(tmp, ".new");
    strcpy(old, name);
    strcat(old, ".old");
    strcpy(rec, name);
    strcat(rec, ".orig");
    dtd_env_recover(name, tmp, old);
    in = dtd_slurp(name, 1, DE_PATHMAX, &len);    /* output needs at most len + a longer path */
    if (in && de_tbar_get(in, (size_t)len, cur, sizeof cur) == DE_OK) {
        dtd_orig_get(name, saved);
        orig = de_orig_pick(cur, saved);
        if (orig && orig != cur)
            prev = cur;                               /* cur is our flattened copy of orig */
        if (want_restore) {
            if (!de_may_restore(cur))
                goto out_free;
            if (orig)
                target = orig;
        } else if (orig) {
            if (!dtd_bank_make(orig, set, copy))     /* unreadable or unparsable original: TBAR stays */
                goto out_free;
            made = 1;
            if (orig == cur && !dtd_put(rec, cur, (long)strlen(cur))) {   /* the original is recorded before TBAR moves */
                DeleteFile((STRPTR)copy);
                goto out_free;
            }
            target = copy;
        } else if (!(de_may_replace(cur) && strcmp(cur, path) != 0))
            goto out_free;
        if (!strcmp(cur, target))
            done = made;                              /* same path: the copy was rewritten in place */
        else {
            out = in + len;
            n = de_tbar_set(in, (size_t)len, target, out, (size_t)(len + DE_PATHMAX));
            if (n > 0 && (f = Open((STRPTR)tmp, MODE_NEWFILE)) != 0) {
                int ok = Write(f, out, n) == n;
                Close(f);
                if (ok && Rename((STRPTR)name, (STRPTR)old)) {
                    if (Rename((STRPTR)tmp, (STRPTR)name)) {
                        DeleteFile((STRPTR)old);
                        done = 1;
                    } else {
                        Rename((STRPTR)old, (STRPTR)name);
                    }
                }
                if (!done)
                    DeleteFile((STRPTR)tmp);
            }
            if (!done && made && strcmp(copy, cur))
                DeleteFile((STRPTR)copy);
        }
        if (done && prev && strcmp(prev, target))
            DeleteFile((STRPTR)prev);                 /* the previous flattened copy, only ever ours (DE_BANK_C) */
        if (done && want_restore && orig)
            DeleteFile((STRPTR)rec);
    }
out_free:
    if (in)
        FreeVec(in);
    return done;
}

/* palette_count of one environment file to 0 (de_userpens_zero): DOpus takes no pens of its own at its next start, so the
 * Workbench pens 4..11 stay with Patina's icon pen holder (S92e). Written only when it changes. */
static void dtd_userpens_one(const char *name)
{
    long len = 0;
    uint8_t *in = dtd_slurp(name, 0, 0, &len);
    if (!in)
        return;
    if (de_userpens_zero(in, (size_t)len) == 1)
        dtd_put(name, in, len);
    FreeVec(in);
}

static int dtd_env_all(const char *path, const char *set, int want_restore)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    int n, w = 0;
    me->pr_WindowPtr = (APTR)-1;
    for (n = 0; n < 2; n++) {
        w += dtd_env_one(dtd_env_files[n], path, set, want_restore);
        if (!want_restore)
            dtd_userpens_one(dtd_env_files[n]);
    }
    me->pr_WindowPtr = oldwin;
    return w;
}

int dopustb_env_apply(const char *bank, const char *set)
{
    return dtd_env_all(bank, set, 0);
}

int dopustb_env_restore(void)
{
    return dtd_env_all(DE_STOCK, "", 1);
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
    strcat(dir, "/");
    dtd_arrow_copy(dir, DTD_ARROW_DIR, 1);       /* after the blend, so the arrow carries it too */
    strcpy(path, DOPUS_TOOLBAR_FLAT);            /* new listers read their bank from the environment files */
    strcat(path, set);
    dopustb_env_apply(path, set);
    return changed;
}

/* DT-9: the family of the icon-style setting (filled, the default, or outline), read from the Patina settings each theme apply. */
static int dtd_outline(void *ud)
{
    am_prefs *p = amconf_borrow();     /* never NULL (amconf_dos.h) */
    int outline = amconf_icon_outline(p);
    (void)ud;
    amconf_return(p);
    return outline;
}

const dopus_tb_ops dopustb_dos_ops = { dtd_screen, dtd_recolour, dtd_outline, desk_dos_font_exists, 0 };
