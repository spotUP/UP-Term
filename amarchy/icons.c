/* amarchy-icons: put the Lucide icons on the Workbench default icons, and the originals back (plan
 * 2026-10-31-amarchy-lucide-icons.md rows L8, L9).
 *
 *   amarchy-icons APPLY     Lucide images over ENVARC:Sys/def_*.info and ENV:Sys/def_*.info (disk, drawer, tool, project,
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

#include "iconapply_core.h"
#include "../tty/qmark.h"

long __stack = 16384;
const char amarchy_icons_stack_cookie[] = "$STACK: 16384";
static const char ver[] = "$VER: amarchy-icons 1.0 (1.11.2026)";

struct Library *IconBase;
struct IntuitionBase *IntuitionBase;

#define TEMPLATE_TEXT "amarchy-icons APPLY | RESTORE | STATUS: Lucide icons on the Workbench default icons, originals in SYS:Storage/Icons/Amarchy\n"
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

/* lucide.dat whole into a fresh buffer (the images point into it); its length, 0 when missing or unreadable */
static long load_dat(uint8_t **out)
{
    BPTR f = Open((STRPTR)IA_DAT, MODE_OLDFILE);
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

static void print_counters(const ia_ctx *c)
{
    unsigned long v[12];
    v[0] = c->written; v[1] = c->skipped_done; v[2] = c->skipped_bad; v[3] = c->skipped_nodat;
    v[4] = c->skipped_nodefault; v[5] = c->skipped_io; v[6] = c->restored; v[7] = c->restore_failed;
    v[8] = c->skipped_readonly; v[9] = c->skipped_locked; v[10] = c->skipped_uaem; v[11] = c->fallback;
    VPrintf((STRPTR)"written=%ld skipped-done=%ld skipped-bad=%ld skipped-nodat=%ld skipped-nodefault=%ld skipped-io=%ld restored=%ld restore-failed=%ld skipped-readonly=%ld skipped-locked=%ld skipped-uaem=%ld fallback=%ld\n",
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
    (void)ver;

    if (qmark_line(args) || cmd == IA_CMD_HELP) {
        PutStr((STRPTR)TEMPLATE_TEXT);
        return 0;
    }
    if (cmd == IA_CMD_NONE) {
        PutStr((STRPTR)TEMPLATE_TEXT);
        return 5;
    }
    if (cmd == IA_CMD_STATUS) {       /* nothing is resident: the counters live for one run, and the manifest is the state */
        BPTR l = Lock((STRPTR)IA_MANIFEST, SHARED_LOCK);
        PutStr((STRPTR)(l ? "icons=applied\n" : "icons=original\n"));
        if (l) UnLock(l);
        return 0;
    }
    c = (ia_ctx *)AllocVec(sizeof *c + 4 * IA_FILECAP, MEMF_PUBLIC | MEMF_CLEAR);
    if (!c) return 10;
    c->cur = (uint8_t *)(c + 1);
    c->out = c->cur + IA_FILECAP;
    c->bak = c->out + IA_FILECAP;
    g_chk = c->bak + IA_FILECAP;
    {
        static ia_io io;
        io.ctx = NULL; io.read = io_read; io.write = io_write; io.remove = io_remove; io.dflt = io_dflt;
        c->io = &io;
    }
    IconBase = OpenLibrary((STRPTR)"icon.library", 36);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IconBase || !IntuitionBase) { rc = 10; goto out; }

    if (cmd == IA_CMD_APPLY) {
        static li_dat d;
        struct Screen *s;
        dn = load_dat(&dat);
        if (dn <= 0 || li_dat_open(dat, (size_t)dn, &d) != 0) {
            PutStr((STRPTR)"amarchy-icons: UP-Term:Amarchy/Icons/lucide.dat missing or bad\n");
            rc = 10;
            goto out;
        }
        c->dat = &d;
        s = LockPubScreen(NULL);
        c->variant = s ? li_pick_variant(s->Width, s->Height) : 0;
        if (s) UnlockPubScreen(NULL, s);
        ia_apply(c);
    } else {
        ia_restore(c);
        if (c->restore_failed) rc = 5;
    }
    print_counters(c);
out:
    if (dat) FreeVec(dat);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (IconBase) CloseLibrary(IconBase);
    FreeVec(c);
    return rc;
}
