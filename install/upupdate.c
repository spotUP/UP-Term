/* upupdate -- the Installer's update: what changed between the installed kit
 * and this one (installer update plan, U4).
 *
 *   upupdate OLD NEW KIT
 *
 * OLD: the installed kit's manifest (UP-Term:MANIFEST), NEW: this kit's
 * (<kit>/Files/MANIFEST), KIT: this kit's drawer, the copies' source. The
 * comparison is updiff.c's; this writes what the Installer reads:
 *   ENV:UPTUPD<part>    the KB the part copies, set for every part that runs
 *   ENV:UPTUPDKB        all of them
 *   ENV:UPTVEROLD / ENV:UPTVERNEW   the first lines of the two VERSIONS
 *                       (the files beside the two manifests)
 *   T:UPTUPD-<part>     a copy part's script: its new and changed files
 *                       copied, the ones gone deleted
 * One line of summary on the output. Return code 0, or 20 (nothing written:
 * a manifest missing or not one). */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdlib.h>
#include "updiff.h"

static BPTR script[UD_MAXPARTS];
static char scriptname[UD_MAXPARTS][UD_NAME];
static int nscripts, failed;

/* the whole file, 0-terminated, or 0 */
static char *slurp(const char *name)
{
    BPTR f = Open((STRPTR)name, MODE_OLDFILE);
    LONG n;
    char *buf;
    if (!f) return 0;
    Seek(f, 0, OFFSET_END);
    n = Seek(f, 0, OFFSET_BEGINNING);
    buf = (n >= 0) ? (char *)malloc((size_t)n + 1) : 0;
    if (buf && Read(f, buf, n) != n) {
        free(buf);
        buf = 0;
    }
    if (buf) buf[n] = 0;
    Close(f);
    return buf;
}

static void emit(void *ctx, const char *part, const char *line)
{
    int i;
    char name[64];
    (void)ctx;
    for (i = 0; i < nscripts && strcmp(scriptname[i], part); i++)
        ;
    if (i == nscripts) {
        if (nscripts >= UD_MAXPARTS) {
            failed = 1;
            return;
        }
        strcpy(name, "T:UPTUPD-");
        strcat(name, part);
        strcpy(scriptname[i], part);
        script[i] = Open((STRPTR)name, MODE_NEWFILE);
        nscripts++;
        if (!script[i]) {
            failed = 1;
            return;
        }
        FPuts(script[i], (STRPTR)"FailAt 21\n");
    }
    if (script[i]) {
        FPuts(script[i], (STRPTR)line);
        FPutC(script[i], '\n');
    }
}

static void setvar(const char *name, const char *value)
{
    if (!SetVar((STRPTR)name, (STRPTR)value, -1, GVF_GLOBAL_ONLY)) failed = 1;
}

static void number(char *buf, unsigned long n)
{
    char t[12];
    int k = 0;
    do t[k++] = (char)('0' + n % 10); while ((n /= 10) != 0);
    while (k) *buf++ = t[--k];
    *buf = 0;
}

/* ENV:<var> = the first line of the VERSIONS beside the manifest */
static void version(const char *manifest, const char *var)
{
    char path[256], *text, *e;
    const char *part = (const char *)PathPart((STRPTR)manifest);
    size_t n = (size_t)(part - manifest);
    if (n + 10 >= sizeof(path)) return;
    memcpy(path, manifest, n);
    path[n] = 0;
    AddPart((STRPTR)path, (STRPTR)"VERSIONS", sizeof(path));
    if (!(text = slurp(path))) {
        setvar(var, "unknown");
        return;
    }
    if ((e = strchr(text, '\n')) != 0) *e = 0;
    setvar(var, text);
    free(text);
}

int main(void)
{
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)"OLD/A,NEW/A,KIT/A", args, 0);
    char *a = 0, *b = 0, name[64], kb[16];
    ud_result *r = 0;
    int i, parts = 0, rc = 20;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"upupdate");
        return 20;
    }
    a = slurp((const char *)args[0]);
    b = slurp((const char *)args[1]);
    r = (ud_result *)malloc(sizeof(ud_result));
    if (!a || !b || !r) {
        Printf("upupdate: cannot read %s\n", (LONG)(a ? args[1] : args[0]));
        goto out;
    }
    if (ud_compare(a, b, (const char *)args[2], r, emit, 0) != 0) {
        Printf("upupdate: %s or %s is not a kit manifest\n", args[0], args[1]);
        goto out;
    }
    for (i = 0; i < r->nparts; i++)
        if (r->part[i].run) {
            strcpy(name, "UPTUPD");
            strcat(name, r->part[i].name);
            number(kb, ud_kb(&r->part[i]));
            setvar(name, kb);
            parts++;
        }
    number(kb, (r->bytes + 1023) / 1024);
    setvar("UPTUPDKB", kb);
    version((const char *)args[0], "UPTVEROLD");
    version((const char *)args[1], "UPTVERNEW");
    Printf("upupdate: %ld files new or changed (%s KB), %ld gone, %ld parts to run\n",
           (LONG)r->files, (LONG)kb, (LONG)r->gone, (LONG)parts);
    rc = failed ? 20 : 0;
out:
    for (i = 0; i < nscripts; i++)
        if (script[i]) Close(script[i]);
    free(a);
    free(b);
    free(r);
    FreeArgs(rd);
    return rc;
}
