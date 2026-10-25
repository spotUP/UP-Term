/* amarchy-index: the Amarchy app index scanner (plan 2026-10-23-amarchy-app-index.md, X2).
 *
 * Walks every hard-disk volume for icons: a WBTOOL icon is an app, a WBPROJECT icon whose default
 * tool is WHDLoad is a game. The rows (`kind<TAB>label<TAB>path`, amarchy/index_core.c) go to
 * ENVARC:Amarchy/apps through a temp file and a rename, so a reader never sees half a file. Amarchy
 * itself never scans. The upconf key amarchy-index-skip (comma separated path prefixes) leaves
 * drawers out. Network volumes (ix_net_handler) are left out unless
 * the key amarchy-index-volumes lists their names. Ctrl-C stops the scan: no cache, RETURN_WARN. A plain vbcc program, no ixemul.
 *
 * Progress (AMARCHY-INDEX-WHERE): the scan copies the drawer or icon path it is on, and a stage word,
 * into a public record (a semaphore named IX_PROG_NAME, AllocVec MEMF_PUBLIC, never freed, found
 * again by the next run) that outlives a scanner that dies at a Software Failure. No disk writes.
 * `amarchy-index --where` prints it. The record's logic is index_core.c (ix_prog_*). */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <string.h>
#include "index_core.h"
#include "ixwatch.h"
#include "../install/hardvol.h"
#include "../config/upconf.h"
#include "../tty/qmark.h"

static const char ver[] = "$VER: amarchy-index 1.0 (23.10.2026) (UP-Term)";

/* vbcc's startup (vc.lib) reads ___stack and runs main on a stack of this
 * size, whatever stack the Shell, script or menu gives it; the cookie says the
 * same to AmigaOS 3.2 and to vsh (RULES.md rule 6). A full scan measured under
 * 1700 bytes on icon.library 40.1 and 47.5; the rest is room for other icon
 * libraries' GetDiskObject. */
long __stack = 16384;
const char amarchy_index_stack_cookie[] = "$STACK: 16384";

#define IX_DEPTH    6
#define IX_MAXROWS  3000
#define IX_MAXVOLS  16
#define IX_DEST     IX_CACHE_PATH
#define IX_TMP      "ENVARC:Amarchy/apps.tmp"

struct Library *IconBase;

static ixshared *sh;
static char where_buf[1024];

static upconf conf;
static char conf_buf[UC_MAX_FILE + 2];
static const char *skip_list = "";
static const char *net_volumes = "";   /* upconf amarchy-index-volumes: the network volumes to scan */
static int stopped;                    /* Ctrl-C seen: unwind, write nothing */
static ix_row *rows;
static int nrows;
static long n_dirs;

/* Drawers that hold no apps: system, fonts, env, trash. A name is compared whole, case-insensitive. */
static const char *const skip_drawers[] = {"Devs", "Libs", "L", "Fonts", "Locale", "Env", "T", "Trashcan", 0};

/* The record, made on first use and found again afterwards (never one per run). Finding is ixwatch.c. */
static ixshared *shared_make(void)
{
    ixshared *u;
    int i;
    Forbid();
    u = ixw_find();
    if (!u && (u = (ixshared *)AllocVec(sizeof(ixshared), MEMF_PUBLIC | MEMF_CLEAR))) {
        for (i = 0; IX_PROG_NAME[i]; i++)
            u->name[i] = IX_PROG_NAME[i];
        u->ss.ss_Link.ln_Name = u->name;
        AddSemaphore(&u->ss);   /* never freed: --where reads it after the scanner is gone */
    }
    Permit();
    return u;
}

/* Take the record for this scan, or refuse (0) when a live scanner owns it (ix_scan_live): one writer of
 * the apps file. Check and take under one Forbid, so two copies started together cannot both pass. */
static int claim(void)
{
    static ix_prog snap;
    int alive = 0, ok = 1, have;
    Forbid();
    if ((sh = shared_make())) {
        have = ixw_snapshot(&snap, &alive);   /* its own statement: alive is set before it is read */
        if (ix_scan_live(have, &snap, alive)) {
            sh = 0;
            ok = 0;
        } else {
            ix_prog_begin(&sh->p);
            sh->task = FindTask(0);
            strncpy(sh->tname, sh->task->tc_Node.ln_Name ? (const char *)sh->task->tc_Node.ln_Name : "", 31);
        }
    }
    Permit();
    return ok;
}

static void note(int stage, const char *path)
{
    if (sh)
        ix_prog_note(&sh->p, stage, path);
}

/* --where: print the record, say whether the scanner still exists and whether it moved in a second. */
static int where(void)
{
    static ix_prog snap;
    long seq;
    int alive, moving = 0;
    if (!ixw_snapshot(&snap, &alive)) {
        if (ixw_other_layout()) {
            if (ix_prog_other_text(where_buf, sizeof where_buf) > 0)
                PutStr((STRPTR)where_buf);
            return 0;
        }
        PutStr((STRPTR) "amarchy-index: no scan has run since the machine started\n");
        return 0;
    }
    if (snap.version != IX_PROG_VERSION) {
        ix_prog_text(&snap, alive, 0, where_buf, sizeof where_buf);
        PutStr((STRPTR)where_buf);
        return 0;
    }
    if (alive && snap.stage != IX_ST_DONE && snap.stage != IX_ST_IDLE) {
        seq = snap.seq;
        Delay(50);
        ixw_snapshot(&snap, &alive);
        moving = snap.seq != seq;
    }
    if (ix_prog_text(&snap, alive, moving, where_buf, sizeof where_buf) < 0)
        PutStr((STRPTR) "amarchy-index: record does not fit\n");
    else
        PutStr((STRPTR)where_buf);
    return 0;
}

static int ieqn(const char *a, const char *b, int n);

static int word_is(const char *line, const char *w1, const char *w2)
{
    int n = 0, i, hit = 0;
    while (*line == ' ' || *line == '\t')
        line++;
    while (line[n] && line[n] != ' ' && line[n] != '\t' && line[n] != '\n' && line[n] != '\r')
        n++;
    for (i = n; line[i] == ' ' || line[i] == '\t' || line[i] == '\n' || line[i] == '\r'; i++)
        ;
    if (line[i])
        return 0;
    if (w1 && (int)strlen(w1) == n && ieqn(line, w1, n))
        hit = 1;
    if (w2 && (int)strlen(w2) == n && ieqn(line, w2, n))
        hit = 1;
    return hit;
}

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
        net_volumes = upconf_str(&conf, "default", "amarchy-index-volumes", "");
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
    note(IX_ST_ICON, base);
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

/* Ctrl-C pending? SetSignal(0, 0) reads the signals without clearing them. Latches in `stopped`. */
static int broken(void)
{
    if (!stopped && (SetSignal(0, 0) & SIGBREAKF_CTRL_C))
        stopped = 1;
    return stopped;
}

static void walk(const char *dir, int depth)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    char sub[IX_PATH];
    if (nrows >= IX_MAXROWS || stopped)
        return;
    if (broken())
        return;
    note(IX_ST_WALK, dir);
    if (!(lock = Lock((STRPTR)dir, ACCESS_READ)))
        return;
    n_dirs++;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib)) {
        while (!broken() && ExNext(lock, fib)) {
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
            if (nrows >= IX_MAXROWS || stopped)
                break;
        }
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/* Does the volume sit on a network handler (ix_net_handler)? The device that owns the volume's
 * handler task names it; checked here, not in hv_disk(), which the installer shares. */
static int vol_network(struct DosList *vol, struct DosList *head)
{
    struct DosList *d;
    char h[64];
    for (d = NextDosEntry(head, LDF_DEVICES); d; d = NextDosEntry(d, LDF_DEVICES)) {
        const unsigned char *hn;
        int n;
        if (!ix_same_handler_task(vol->dol_Task, d->dol_Task) || !d->dol_misc.dol_handler.dol_Handler)
            continue;
        hn = (const unsigned char *)BADDR(d->dol_misc.dol_handler.dol_Handler);
        n = hn[0] < sizeof h - 1 ? hn[0] : (int)sizeof h - 1;
        memcpy(h, hn + 1, n);
        h[n] = 0;
        if (ix_net_handler(h))
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
        if (len <= 0 || len >= IX_VOL || !hv_disk(v, head, 0))
            continue;
        memcpy(names[n], nm + 1, len);
        names[n][len] = 0;
        if (!ix_volume_wanted(names[n], vol_network(v, head), net_volumes))
            continue;
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
    const char *args = (const char *)GetArgStr();
    if (qmark_line(args) || word_is(args, "help", 0)) {
        PutStr((STRPTR) "amarchy-index: scans the hard disks for apps and WHDLoad games and writes " IX_DEST "\n"
               "amarchy-index --where (-w): the stage and path of the running or last scan, and whether the scanner task still exists\n");
        return 0;
    }
    if (word_is(args, "--where", "-w"))
        return where();
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
    if (!claim()) {
        PutStr((STRPTR) "amarchy-index: a scan runs already, amarchy-index --where shows it\n");
        FreeVec(rows);
        CloseLibrary(IconBase);
        return RETURN_WARN;
    }
    nv = volumes(names);
    for (i = 0; i < nv; i++) {
        char root[IX_VOL + 2];
        strcpy(root, names[i]);
        strcat(root, ":");
        walk(root, 0);
    }
    if (broken()) {
        if (sh)
            ix_prog_stopped(&sh->p);
        PutStr((STRPTR) "amarchy-index: stopped by Ctrl-C, no cache written\n");
        FreeVec(rows);
        CloseLibrary(IconBase);
        return RETURN_WARN;
    }
    note(IX_ST_WRITE, IX_DEST);
    ix_sort_dedupe(rows, nrows);
    for (i = 0; i < nrows; i++)
        rows[i].kind == IX_KIND_GAME ? ngames++ : napps++;
    i = write_cache() ? 0 : RETURN_ERROR;
    if (!i && sh)
        ix_prog_done(&sh->p, napps, ngames, nv);
    else if (i && sh)
        ix_prog_write_failed(&sh->p);
    if (i)
        PutStr((STRPTR) "amarchy-index: cannot write " IX_DEST "\n");
    else
        Printf((STRPTR) "amarchy-index: %ld apps, %ld games on %ld volumes\n", (LONG)napps, (LONG)ngames, (LONG)nv);
    FreeVec(rows);
    CloseLibrary(IconBase);
    return i;
}
