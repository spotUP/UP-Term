/* amarchy-index: the Amarchy app index scanner (plan 2026-10-23-amarchy-app-index.md, X2).
 *
 * Walks every hard-disk volume for icons: a WBTOOL icon is an app, a WBPROJECT icon whose default
 * tool is WHDLoad is a game. The rows (`kind<TAB>label<TAB>path`, amarchy/index_core.c) go to
 * ENVARC:Amarchy/apps through a temp file and a rename, so a reader never sees half a file. Amarchy
 * itself never scans. The upconf key amarchy-index-skip (comma separated path prefixes) leaves
 * drawers out. A plain vbcc program, no ixemul. */
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <string.h>
#include "index_core.h"
#include "../config/upconf.h"
#include "../tty/qmark.h"

static const char ver[] = "$VER: amarchy-index 1.0 (23.10.2026) (UP-Term)";

#define IX_DEPTH    6
#define IX_MAXROWS  3000
#define IX_MAXVOLS  16
#define IX_DEST     IX_CACHE_PATH
#define IX_TMP      "ENVARC:Amarchy/apps.tmp"
#define ID_CD01 0x43443031L
#define ID_CDFS 0x43444653L

struct Library *IconBase;

static upconf conf;
static char conf_buf[UC_MAX_FILE + 2];
static const char *skip_list = "";
static ix_row *rows;
static int nrows;
static long n_dirs;

/* Drawers that hold no apps: system, fonts, env, trash. A name is compared whole, case-insensitive. */
static const char *const skip_drawers[] = {"Devs", "Libs", "L", "Fonts", "Locale", "Env", "T", "Trashcan", 0};

static int lc(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/* case-insensitive compare of at most n characters (n < 0: whole strings); no utility.library needed */
static int ieqn(const char *a, const char *b, int n)
{
    for (; n != 0; n--, a++, b++) {
        if (lc((unsigned char)*a) != lc((unsigned char)*b))
            return 0;
        if (!*a)
            break;
    }
    return 1;
}

static int ieq(const char *a, const char *b)
{
    return ieqn(a, b, -1);
}

static int skipped_drawer(const char *path, const char *name)
{
    int i, len = (int)strlen(path);
    for (i = 0; skip_drawers[i]; i++)
        if (ieq(name, skip_drawers[i]))
            return 1;
    /* Prefs/Env-Archive, wherever the Prefs drawer is */
    if (len >= 17 && ieq(path + len - 17, "Prefs/Env-Archive") && (len == 17 || path[len - 18] == '/' || path[len - 18] == ':'))
        return 1;
    return 0;
}

static void conf_read(void)
{
    BPTR f = Open((STRPTR) "ENV:up-term/up-term", MODE_OLDFILE);
    LONG got;
    if (!f)
        f = Open((STRPTR) "ENVARC:up-term/up-term", MODE_OLDFILE);
    if (!f)
        return;
    got = Read(f, conf_buf, UC_MAX_FILE);
    Close(f);
    if (got > 0) {
        conf_buf[got] = 0;
        upconf_parse(&conf, conf_buf, got);
        skip_list = upconf_str(&conf, "default", "amarchy-index-skip", "");
    }
}

static int file_exists(const char *path)
{
    BPTR l = Lock((STRPTR)path, ACCESS_READ);
    if (!l)
        return 0;
    UnLock(l);
    return 1;
}

static int is_whdload(const char *tool)
{
    int len = (int)strlen(tool), i;
    for (i = len; i > 0 && tool[i - 1] != '/' && tool[i - 1] != ':'; i--)
        ;
    return ieqn(tool + i, "WHDLoad", 7);
}

/* One icon file `dir/name.info`: add a row for it when it is an app or a WHDLoad game. */
static void add_icon(const char *dir, const char *name)
{
    char base[IX_PATH];
    struct DiskObject *o;
    int kind = -1, dl = (int)strlen(dir), nl = (int)strlen(name) - 5;
    if (nrows >= IX_MAXROWS || nl <= 0 || dl + 1 + nl >= IX_PATH || ieq(name, "Disk.info"))
        return;
    strcpy(base, dir);
    if (dl && base[dl - 1] != ':' && base[dl - 1] != '/')
        strcat(base, "/");
    strncat(base, name, nl);
    if (ix_skip_match(skip_list, base))
        return;
    o = GetDiskObject((STRPTR)base);
    if (!o)
        return;
    if (o->do_Type == WBTOOL)
        kind = IX_KIND_APP;
    else if (o->do_Type == WBPROJECT && o->do_DefaultTool && is_whdload((const char *)o->do_DefaultTool))
        kind = IX_KIND_GAME;
    FreeDiskObject(o);
    if (kind < 0)
        return;
    /* a tool icon with no program beside it is not an app (a project's file may be a slave, so only tools) */
    if (kind == IX_KIND_APP && !file_exists(base))
        return;
    memset(&rows[nrows], 0, sizeof rows[nrows]);
    rows[nrows].kind = kind;
    ix_label_from_name(name, rows[nrows].label, IX_LABEL);
    strcpy(rows[nrows].path, base);
    nrows++;
}

static void walk(const char *dir, int depth)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    char sub[IX_PATH];
    if (nrows >= IX_MAXROWS || !(lock = Lock((STRPTR)dir, ACCESS_READ)))
        return;
    n_dirs++;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            const char *nm = (const char *)fib->fib_FileName;
            int dl = (int)strlen(dir), nl = (int)strlen(nm);
            if (fib->fib_DirEntryType > 0) {
                if (depth >= IX_DEPTH || dl + 1 + nl >= IX_PATH)
                    continue;
                strcpy(sub, dir);
                if (dl && sub[dl - 1] != ':' && sub[dl - 1] != '/')
                    strcat(sub, "/");
                strcat(sub, nm);
                if (skipped_drawer(sub, nm) || ix_skip_match(skip_list, sub))
                    continue;
                walk(sub, depth + 1);
            } else if (nl > 5 && ieq(nm + nl - 5, ".info"))
                add_icon(dir, nm);
            if (nrows >= IX_MAXROWS)
                break;
        }
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/* A volume worth scanning: a disk is in, its handler is not RAM, a CD file system or trackdisk. */
static int hard_disk(struct DosList *vol, struct DosList *devs)
{
    struct DosList *d;
    if (!vol->dol_Task)
        return 0;
    if (vol->dol_misc.dol_volume.dol_DiskType == ID_CD01 || vol->dol_misc.dol_volume.dol_DiskType == ID_CDFS || vol->dol_misc.dol_volume.dol_DiskType == (ULONG)ID_NO_DISK_PRESENT ||
        vol->dol_misc.dol_volume.dol_DiskType == ID_UNREADABLE_DISK)
        return 0;
    for (d = NextDosEntry(devs, LDF_DEVICES); d; d = NextDosEntry(d, LDF_DEVICES)) {
        const unsigned char *nm = (const unsigned char *)BADDR(d->dol_Name);
        if (d->dol_Task != vol->dol_Task || !nm)
            continue;
        if (nm[0] == 3 && nm[1] == 'D' && nm[2] == 'F' && nm[3] >= '0' && nm[3] <= '9')
            return 0;
        if (nm[0] == 3 && (nm[1] == 'R' || nm[1] == 'r') && (nm[2] == 'A' || nm[2] == 'a') && (nm[3] == 'M' || nm[3] == 'm'))
            return 0;
        if (nm[0] == 3 && nm[1] == 'C' && nm[2] == 'D' && nm[3] >= '0' && nm[3] <= '9')
            return 0;
        {
            /* the startup message names the exec device; trackdisk is a floppy whatever the unit is called */
            ULONG st = (ULONG)d->dol_misc.dol_handler.dol_Startup;
            struct FileSysStartupMsg *fs = (struct FileSysStartupMsg *)BADDR(st);
            if (st > 0x400 && fs->fssm_Device) {
                const unsigned char *dn = (const unsigned char *)BADDR(fs->fssm_Device);
                if (dn[0] >= 9 && ieqn((const char *)dn + 1, "trackdisk", 9))
                    return 0;
            }
        }
        return 1;
    }
    return 0;
}

static int volumes(char names[][IX_VOL])
{
    struct DosList *head, *v = 0;
    int n = 0;
    head = LockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
    while (n < IX_MAXVOLS && (v = NextDosEntry(v ? v : head, LDF_VOLUMES))) {
        const unsigned char *nm = (const unsigned char *)BADDR(v->dol_Name);
        int len = nm ? nm[0] : 0;
        if (len <= 0 || len >= IX_VOL || !hard_disk(v, head))
            continue;
        memcpy(names[n], nm + 1, len);
        names[n][len] = 0;
        n++;
    }
    UnLockDosList(LDF_VOLUMES | LDF_DEVICES | LDF_READ);
    return n;
}

static int write_cache(void)
{
    BPTR f;
    char line[IX_LABEL + IX_PATH + 16];
    int i, len;
    UnLock(CreateDir((STRPTR) "ENVARC:Amarchy")); /* fails when the drawer exists, which is fine */
    if (!(f = Open((STRPTR)IX_TMP, MODE_NEWFILE)))
        return 0;
    for (i = 0; i < nrows; i++) {
        len = ix_format_row(&rows[i], line, sizeof line);
        if (len < 0 || Write(f, line, len) != len) {
            Close(f);
            DeleteFile((STRPTR)IX_TMP);
            return 0;
        }
    }
    if (!Close(f)) {
        DeleteFile((STRPTR)IX_TMP);
        return 0;
    }
    DeleteFile((STRPTR)IX_DEST);
    return Rename((STRPTR)IX_TMP, (STRPTR)IX_DEST) != 0;
}

int main(void)
{
    char names[IX_MAXVOLS][IX_VOL];
    int nv, i, napps = 0, ngames = 0;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR) "amarchy-index: scans the hard disks for apps and WHDLoad games and writes " IX_DEST "\n");
        return 0;
    }
    if (!(IconBase = OpenLibrary((STRPTR) "icon.library", 36))) {
        PutStr((STRPTR) "amarchy-index: needs icon.library\n");
        return 20;
    }
    rows = (ix_row *)AllocVec(sizeof(ix_row) * IX_MAXROWS, MEMF_PUBLIC | MEMF_CLEAR);
    if (!rows) {
        CloseLibrary(IconBase);
        return 20;
    }
    conf_read();
    nv = volumes(names);
    for (i = 0; i < nv; i++) {
        char root[IX_VOL + 2];
        strcpy(root, names[i]);
        strcat(root, ":");
        walk(root, 0);
    }
    ix_sort_dedupe(rows, nrows);
    for (i = 0; i < nrows; i++)
        rows[i].kind == IX_KIND_GAME ? ngames++ : napps++;
    i = write_cache() ? 0 : 10;
    if (i)
        PutStr((STRPTR) "amarchy-index: cannot write " IX_DEST "\n");
    else
        Printf((STRPTR) "amarchy-index: %ld apps, %ld games on %ld volumes\n", (LONG)napps, (LONG)ngames, (LONG)nv);
    FreeVec(rows);
    CloseLibrary(IconBase);
    return i;
}
