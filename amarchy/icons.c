/* amarchy-icons: put the filled icons on the Workbench default icons, and the originals back (plan
 * 2026-10-31-amarchy-lucide-icons.md rows L8, L9).
 *
 *   amarchy-icons APPLY     icon images over ENVARC:Sys/def_*.info and ENV:Sys/def_*.info (disk, drawer, tool, project,
 *                           trashcan, RAM); the originals go to SYS:Storage/Icons/Amarchy/<volume>/<path>.info
 *   amarchy-icons RESTORE   the originals back byte for byte, backups and manifest entries removed
 *   amarchy-icons STATUS    the counters of the last run in this process (written, skipped-*, restored)
 *
 * Every decision is amarchy/iconapply_core.c (host-tested, tests/test_iconapply.c). This file only does the DOS calls:
 * whole-file read, directory creation, the atomic write (<name>.amt, Rename name to <name>.amo, Rename .amt to name,
 * Delete .amo; a leftover is resolved by ia_recover before the next read) and the system default object for a file that
 * does not exist (GetDefDiskObject + PutDiskObject to T:, read back). Return code 0 done, 5 usage, 10 no memory / no dat. */
#include <string.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/intuition.h>

#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include "iconapply_core.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "iconpen_dos.h"
#include "../tty/qmark.h"

long __stack = 16384;
const char amarchy_icons_stack_cookie[] = "$STACK: 16384";
static const char ver[] = "$VER: amarchy-icons 1.0 (1.11.2026)";

struct Library *IconBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#define TEMPLATE_TEXT "amarchy-icons APPLY | RESTORE | STATUS: filled icons on the Workbench default icons, originals in SYS:Storage/Icons/Amarchy\n"
#define DFLT_TMP "T:amarchy-icons-tmp"

static uint8_t *g_chk;      /* scratch for ia_recover's parse checks and the default object */

static int exists(const char *p)
{
    BPTR l = Lock((STRPTR)p, SHARED_LOCK);
    if (l) UnLock(l);
    return l != 0;
}

/* whole file; length, -1 absent, -2 error / too long */
static long read_all(const char *p, uint8_t *buf, size_t cap)
{
    BPTR f;
    long n;
    if (!exists(p)) return IoErr() == ERROR_OBJECT_NOT_FOUND || IoErr() == ERROR_DIR_NOT_FOUND ? -1 : -2;
    f = Open((STRPTR)p, MODE_OLDFILE);
    if (!f) return -2;
    n = Read(f, buf, (long)cap + 1);
    Close(f);
    return n < 0 || (size_t)n > cap ? -2 : n;
}

static int parses(const char *p)
{
    long n = read_all(p, g_chk, IA_FILECAP);
    li_file f;
    return n > 0 && li_parse(g_chk, (size_t)n, &f) == 0;
}

static int ends_info(const char *p)
{
    size_t n = strlen(p);
    return n > 5 && strcmp(p + n - 5, ".info") == 0;
}

/* resolve a leftover of an interrupted atomic write of p (see ia_recover) */
static void recover(const char *p)
{
    char tmp[IA_MAXPATH + 8], old[IA_MAXPATH + 8];
    int has_name, has_tmp, has_old, ok_name, ok_tmp, ok_old;
    if (ia_atomic_names(p, tmp, old, sizeof tmp) != 0) return;
    has_tmp = exists(tmp);
    has_old = exists(old);
    if (!has_tmp && !has_old) return;
    has_name = exists(p);
    ok_name = has_name && parses(p);
    ok_tmp = has_tmp && (!ends_info(p) || parses(tmp));
    ok_old = has_old && (!ends_info(p) || parses(old));
    switch (ia_recover(has_name, ok_name, has_tmp, ok_tmp, has_old, ok_old)) {
    case IA_REC_OLD_TO_NAME:
        if (has_name) DeleteFile((STRPTR)p);
        Rename((STRPTR)old, (STRPTR)p);
        if (has_tmp) DeleteFile((STRPTR)tmp);
        break;
    case IA_REC_TMP_TO_NAME:
        if (has_name) DeleteFile((STRPTR)p);
        Rename((STRPTR)tmp, (STRPTR)p);
        if (has_old) DeleteFile((STRPTR)old);
        break;
    case IA_REC_DEL_TMP:
        DeleteFile((STRPTR)tmp);
        if (has_old) DeleteFile((STRPTR)old);
        break;
    default:
        if (has_old && ok_name) DeleteFile((STRPTR)old);
        break;
    }
}

static long io_read(void *ctx, const char *p, uint8_t *buf, size_t cap)
{
    (void)ctx;
    recover(p);
    return read_all(p, buf, cap);
}

static void make_dirs(const char *p)
{
    char d[IA_MAXPATH + 40];
    size_t i, n = strlen(p);
    if (n >= sizeof d) return;
    memcpy(d, p, n + 1);
    for (i = 0; i < n; i++) {
        if (d[i] == '/') {
            d[i] = 0;
            UnLock(CreateDir((STRPTR)d));       /* fails when the drawer exists, which is fine */
            d[i] = '/';
        }
    }
}

static int io_write(void *ctx, const char *p, const uint8_t *buf, size_t len)
{
    char tmp[IA_MAXPATH + 48], old[IA_MAXPATH + 48];
    BPTR f;
    long w;
    int had;
    (void)ctx;
    if (ia_atomic_names(p, tmp, old, sizeof tmp) != 0) return -1;
    make_dirs(p);
    recover(p);
    f = Open((STRPTR)tmp, MODE_NEWFILE);
    if (!f) return -1;
    w = Write(f, (APTR)buf, (long)len);
    if (!Close(f) || w != (long)len) {
        DeleteFile((STRPTR)tmp);
        return -1;
    }
    had = exists(p);
    if (had) {
        DeleteFile((STRPTR)old);
        if (!Rename((STRPTR)p, (STRPTR)old)) {
            DeleteFile((STRPTR)tmp);
            return -1;
        }
    }
    if (!Rename((STRPTR)tmp, (STRPTR)p)) {
        if (had) Rename((STRPTR)old, (STRPTR)p);
        DeleteFile((STRPTR)tmp);
        return -1;
    }
    if (had) DeleteFile((STRPTR)old);
    return 0;
}

static int io_remove(void *ctx, const char *p)
{
    (void)ctx;
    recover(p);
    return !exists(p) || DeleteFile((STRPTR)p) ? 0 : -1;
}

/* one report line per skipped file and per walked volume (iconapply_core.h ia_io.note) */
static void io_note(void *ctx, const char *line)
{
    (void)ctx;
    PutStr((STRPTR)line);
    PutStr((STRPTR)"\n");
}

/* the system default object of a defaults match, as the bytes of its .info file */
static long io_dflt(void *ctx, const char *match, uint8_t *buf, size_t cap)
{
    static const struct { const char *m; long type; } t[] = {
        { "def_disk", WBDISK }, { "def_drawer", WBDRAWER }, { "def_tool", WBTOOL }, { "def_project", WBPROJECT },
        { "def_trashcan", WBGARBAGE }, { "def_RAM", WBDISK }
    };
    struct DiskObject *o = NULL;
    long n = -1;
    size_t i;
    (void)ctx;
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (strcmp(t[i].m, match) == 0) o = GetDefDiskObject(t[i].type);
    if (!o) return -1;
    if (PutDiskObject((STRPTR)DFLT_TMP, o)) {
        n = read_all(DFLT_TMP ".info", buf, cap);
        DeleteFile((STRPTR)(DFLT_TMP ".info"));
    }
    FreeDiskObject(o);
    return n > 0 ? n : -1;
}

/* the icon set the icon-style pref names (Filled: lucide.dat, Outline: outline.dat); the pref is read from the Amarchy prefs files */
static const char *dat_path(void)
{
    static am_prefs ap;
    amconf_load(&ap);
    return ia_dat_for_style(ap.val[AM_ICONSTYLE]);
}

/* the dat whole into a fresh buffer (the images point into it); its length, 0 when missing or unreadable */
static long load_dat(uint8_t **out, const char *path)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    long n = 0;
    uint8_t *b;
    if (!f) return 0;
    Seek(f, 0, OFFSET_END);
    n = Seek(f, 0, OFFSET_BEGINNING);
    if (n <= 0 || !(b = (uint8_t *)AllocVec((ULONG)n, MEMF_PUBLIC))) {
        Close(f);
        return 0;
    }
    if (Read(f, b, n) != n) {
        FreeVec(b);
        n = 0;
    } else {
        *out = b;
    }
    Close(f);
    return n;
}

/* ---- all-volume walk (L12) ----
 * Every mounted, online volume: Info() id_DiskState says read-only; each directory is listed completely (Examine / ExNext) into
 * a heap list BEFORE any file is written, because the atomic write renames files in the directory being listed. The recursion
 * is bounded by WALK_MAXDEPTH; a frame holds two pointers and a length, the path buffer is shared, the FileInfoBlock is on the heap. */
#define WALK_MAXDEPTH 24
#define WALK_MAXVOL   32
static char g_wpath[IA_MAXPATH];

/* list entry: u32 protection, 1 byte kind ('D' directory, 'F' .info, 'U' .info.uaem), name, NUL */
static int list_dir(BPTR lock, uint8_t **out, size_t *outn)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    uint8_t *b = NULL;
    size_t n = 0, cap = 0;
    int ok = 1;
    if (!fib) return 0;
    if (Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            size_t nl = strlen(fib->fib_FileName), need;
            char kind;
            unsigned long pr = (unsigned long)fib->fib_Protection;
            if (fib->fib_DirEntryType > 0) kind = 'D';
            else if (nl > 5 && strcmp(fib->fib_FileName + nl - 5, ".info") == 0) kind = 'F';
            else if (nl > 10 && strcmp(fib->fib_FileName + nl - 10, ".info.uaem") == 0) kind = 'U';
            else continue;
            need = 5 + nl + 1;
            if (n + need > cap) {
                size_t nc = cap ? cap * 2 : 2048;
                uint8_t *nb;
                while (nc < n + need) nc *= 2;
                nb = (uint8_t *)AllocVec((ULONG)nc, MEMF_PUBLIC);
                if (!nb) { ok = 0; break; }
                if (b) { memcpy(nb, b, n); FreeVec(b); }
                b = nb;
                cap = nc;
            }
            memcpy(b + n, &pr, 4);
            b[n + 4] = (uint8_t)kind;
            memcpy(b + n + 5, fib->fib_FileName, nl + 1);
            n += need;
        }
    }
    FreeDosObject(DOS_FIB, fib);
    *out = b;
    *outn = n;
    return ok;
}

static void walk_dir(ia_ctx *c, size_t plen, int ro, unsigned depth)
{
    BPTR lock = Lock((STRPTR)g_wpath, SHARED_LOCK);
    uint8_t *list = NULL;
    size_t n = 0, i;
    int pass;
    if (!lock) { ia_skip_note(c, &c->skipped_io, g_wpath, "io (no lock)"); return; }
    if (!list_dir(lock, &list, &n)) ia_walk_count(c, IA_DO_SKIP_DEEP);
    UnLock(lock);
    for (pass = 0; pass < 2; pass++) {          /* icons of this drawer first, then the subdrawers */
        for (i = 0; i < n;) {
            unsigned long pr;
            const char *name = (const char *)list + i + 5;
            char kind = (char)list[i + 4];
            size_t nl = strlen(name);
            memcpy(&pr, list + i, 4);
            i += 5 + nl + 1;
            if (!ia_walk_fits(c, plen, nl, sizeof g_wpath, depth, WALK_MAXDEPTH, pass == 1 && kind == 'D')) continue;
            if (pass == 0 && kind != 'D') {
                memcpy(g_wpath + plen, name, nl + 1);
                ia_walk_file(c, g_wpath, ro, pr);
            } else if (pass == 1 && kind == 'D') {
                size_t np = plen;
                memcpy(g_wpath + np, name, nl);
                np += nl;
                g_wpath[np] = 0;
                if (ia_walk_skip_dir(g_wpath)) { g_wpath[plen] = 0; continue; }
                g_wpath[np++] = '/';
                g_wpath[np] = 0;
                walk_dir(c, np, ro, depth + 1);
            }
        }
    }
    g_wpath[plen] = 0;
    if (list) FreeVec(list);
}

static void walk_volumes(ia_ctx *c)
{
    char names[WALK_MAXVOL][34];
    int nv = 0, i;
    struct DosList *dl;
    dl = LockDosList(LDF_VOLUMES | LDF_READ);
    while ((dl = NextDosEntry(dl, LDF_VOLUMES)) != NULL && nv < WALK_MAXVOL) {
        const UBYTE *bs = (const UBYTE *)BADDR(dl->dol_Name);
        unsigned l;
        if (!dl->dol_Task || !bs) continue;     /* not mounted */
        l = bs[0] > 31 ? 31 : bs[0];
        memcpy(names[nv], bs + 1, l);
        names[nv][l] = ':';
        names[nv][l + 1] = 0;
        nv++;
    }
    UnLockDosList(LDF_VOLUMES | LDF_READ);
    ia_walk_begin(c);
    for (i = 0; i < nv; i++) {
        BPTR l;
        int ro = 1;
        l = Lock((STRPTR)names[i], SHARED_LOCK);
        if (!l) continue;
        {
            struct InfoData *info = (struct InfoData *)AllocVec(sizeof *info, MEMF_PUBLIC);     /* AllocVec: longword aligned */
            if (info && Info(l, info)) ro = info->id_DiskState != ID_VALIDATED;
            if (info) FreeVec(info);
        }
        UnLock(l);
        strcpy(g_wpath, names[i]);
        walk_dir(c, strlen(g_wpath), ro, 0);
        ia_walk_volume(c, names[i]);
    }
}

static void print_counters(const ia_ctx *c)
{
    unsigned long v[13];
    v[0] = c->written; v[1] = c->skipped_done; v[2] = c->skipped_bad; v[3] = c->skipped_nodat;
    v[4] = c->skipped_nodefault; v[5] = c->skipped_io; v[6] = c->restored; v[7] = c->restore_failed;
    v[8] = c->skipped_readonly; v[9] = c->skipped_locked; v[10] = c->skipped_uaem; v[11] = c->fallback; v[12] = c->skipped_deep;
    VPrintf((STRPTR)"written=%ld skipped-done=%ld skipped-bad=%ld skipped-nodat=%ld skipped-nodefault=%ld skipped-io=%ld restored=%ld restore-failed=%ld skipped-readonly=%ld skipped-locked=%ld skipped-uaem=%ld fallback=%ld skipped-deep=%ld\n",
            (LONG *)v);
}

int main(void)
{
    const char *args = (const char *)GetArgStr();
    int cmd = ia_parse_cmd(args);
    ia_ctx *c;
    uint8_t *dat = NULL;
    long dn;
    int rc = 0;
    struct Process *me = NULL;
    APTR oldwin = NULL;
    (void)ver;

    if (qmark_line(args) || cmd == IA_CMD_HELP) {
        PutStr((STRPTR)TEMPLATE_TEXT);
        return 0;
    }
    if (cmd == IA_CMD_NONE) {
        PutStr((STRPTR)TEMPLATE_TEXT);
        return 5;
    }
    c = (ia_ctx *)AllocVec(sizeof *c + 4 * IA_FILECAP, MEMF_PUBLIC | MEMF_CLEAR);
    if (!c) return 10;
    c->cur = (uint8_t *)(c + 1);
    c->out = c->cur + IA_FILECAP;
    c->bak = c->out + IA_FILECAP;
    g_chk = c->bak + IA_FILECAP;
    {
        static ia_io io;
        io.ctx = NULL; io.read = io_read; io.write = io_write; io.remove = io_remove; io.dflt = io_dflt; io.note = io_note;
        c->io = &io;
    }
    if (cmd == IA_CMD_STATUS) {       /* nothing is resident: the counters live for one run, and the manifest is the state */
        static li_dat d;
        char line[160];
        dn = load_dat(&dat, dat_path());
        if (dn > 0 && li_dat_open(dat, (size_t)dn, &d) == 0) c->dat = &d;     /* without lucide.dat the table path rows count as walked */
        if (ia_status(c, line, sizeof line)) PutStr((STRPTR)line);
        if (ipd_status_line(line, sizeof line)) { PutStr((STRPTR)line); PutStr((STRPTR)"\n"); }   /* the shared icon pen holder (F14) */
        goto out;
    }
    IconBase = OpenLibrary((STRPTR)"icon.library", 36);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    if (!IconBase || !IntuitionBase || !GfxBase) { rc = 10; goto out; }
    /* no "Please insert volume" requester for the whole APPLY / RESTORE: a table path row on an absent assign (UP-Term:), a manifest
     * path on a removed volume, or the walk's volumes; such a path is skipped and counted (restored in out:) */
    me = (struct Process *)FindTask(NULL);
    oldwin = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1L;

    if (cmd == IA_CMD_APPLY) {
        static li_dat d;
        struct Screen *s;
        dn = load_dat(&dat, dat_path());
        if (dn <= 0 || li_dat_open(dat, (size_t)dn, &d) != 0) {
            PutStr((STRPTR)"amarchy-icons: ");
            PutStr((STRPTR)dat_path());
            PutStr((STRPTR)" missing or bad\n");
            rc = 10;
            goto out;
        }
        c->dat = &d;
        ipd_owner(IP_OWNER_ICONS, 1);     /* F14: icons that name pens 32..47 are only on disk while the pens are held; first, so no .info is written without them */
        s = ipd_lock_wb_screen();
        c->variant = s ? li_pick_variant(s->Width, s->Height) : 0;
        if (IconBase->lib_Version >= 47) {     /* OS 3.2: true transparency through our FORM ICON; 3.1 keeps the classic image on pen 44 */
            static uint8_t form_rgb[16][3];
            ipd_icon_rgb(form_rgb);
            c->form_rgb = form_rgb;
        }
        if (s) UnlockPubScreen(NULL, s);
        ia_apply(c);
        walk_volumes(c);
    } else {
        ia_restore(c);
        if (c->restore_failed) rc = 5;
        else ipd_owner(IP_OWNER_ICONS, 0);   /* originals are back: give the reference up (the pens go with the last owner) */
    }
    print_counters(c);
out:
    if (me) me->pr_WindowPtr = oldwin;
    if (dat) FreeVec(dat);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IconBase) CloseLibrary(IconBase);
    FreeVec(c);
    return rc;
}
