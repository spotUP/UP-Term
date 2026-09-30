/* UPConsole -- switch the system's CON: and RAW: to UP-Term (the vtcon
 * handler) and back: H5.4 of thoughts/shared/plans/2026-09-30-console-device.md
 * (design DD2, DD19, DD20, DD21).
 *
 *   UPConsole CON ON      new CON:/RAW: windows run L:vtcon-handler
 *   UPConsole CON OFF     back to the ROM con-handler (open windows keep theirs)
 *   UPConsole STATUS      which handler serves CON:/RAW:, and console.device
 *   HANDLER <file>        another handler file (default L:vtcon-handler; the rig)
 *
 * CON ON loads the handler once and keeps it loaded for good (open windows
 * may run it), then, under the DosList write lock, points the CON and RAW
 * entries at it. What it replaces is kept in a public SignalSemaphore named
 * "UP-Term console", found again by later runs, and put back by CON OFF.
 * It refuses, and changes nothing, when:
 *   - the entries are not the ones the ROM leaves (DP3, measured on 39.106,
 *     40.63, 40.71 and 47.115): another console replacement (KingCON,
 *     ViNCEd, ...) is installed -- conflicts refuse, never stack (DD21);
 *   - dos.library is V47 (3.2): its Shell needs the con-handler's medium mode
 *     (SetMode 2), which UP-Term does not have yet (DD20, plan H5.3).
 * DEVICE and EXCLUDE (the console.device replacement, phase D4) are not
 * built yet and say so. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>

extern struct DosLibrary *DOSBase;

static const char vers[] = "$VER: UPConsole 0.1 (30.9.2026)";

#define SEM_NAME "UP-Term console"

/* what CON ON replaced in one DosList entry */
typedef struct {
    BPTR seglist;
    BSTR handler;
    LONG stack, pri, globvec;
    BPTR startup;
} saved_entry;

typedef struct {
    struct SignalSemaphore ss;
    char name[sizeof(SEM_NAME)];
    BPTR seg;          /* the handler, loaded once, never unloaded */
    int con_on;
    saved_entry saved[2]; /* CON, RAW */
} upc_state;

static const char *const entry_name[2] = { "CON", "RAW" };

/* Under the DosList lock (either kind): the device entry `name`. */
static struct DeviceNode *find_entry(struct DosList *locked, const char *name)
{
    return (struct DeviceNode *)FindDosEntry(locked, (STRPTR)name, LDF_DEVICES);
}

static void bstr_copy(char *out, int max, BSTR b)
{
    const UBYTE *s = b ? (const UBYTE *)BADDR(b) : 0;
    int n = s ? s[0] : 0, i;
    if (n > max - 1)
        n = max - 1;
    for (i = 0; i < n; i++)
        out[i] = (char)s[i + 1];
    out[n] = 0;
}

/* Is this the entry the ROM leaves (DP3's pristine values)? If not, `why`
 * says what differs. */
static int pristine(struct DeviceNode *d, int raw, char *why, int max)
{
    char h[80];
    LONG stack = DOSBase->dl_lib.lib_Version >= 47 ? 4096 : 3200;
    bstr_copy(h, sizeof(h), d->dn_Handler);
    if (h[0]) {
        snprintf(why, (size_t)max, "served by %s", h);
        return 0;
    }
    if (d->dn_Type != 0 || d->dn_GlobalVec != -1 || d->dn_Priority != 5 || !d->dn_SegList ||
        d->dn_Startup != (BPTR)raw || d->dn_StackSize != stack) {
        snprintf(why, (size_t)max, "not the ROM's entry (type %ld stack %ld pri %ld startup %ld globvec %ld)",
                 (long)d->dn_Type, (long)d->dn_StackSize, (long)d->dn_Priority, (long)d->dn_Startup,
                 (long)d->dn_GlobalVec);
        return 0;
    }
    return 1;
}

static int word_is(const char *s, const char *upper)
{
    for (; *s && *upper; s++, upper++)
        if ((*s >= 'a' && *s <= 'z' ? *s - 32 : *s) != *upper)
            return 0;
    return !*s && !*upper;
}

static upc_state *find_state(void)
{
    upc_state *st;
    Forbid();
    st = (upc_state *)FindSemaphore((STRPTR)SEM_NAME);
    Permit();
    return st;
}

static upc_state *make_state(void)
{
    upc_state *st = find_state();
    if (st)
        return st;
    st = (upc_state *)AllocMem(sizeof(*st), MEMF_PUBLIC | MEMF_CLEAR);
    if (!st)
        return 0;
    strcpy(st->name, SEM_NAME);
    st->ss.ss_Link.ln_Name = st->name;
    st->ss.ss_Link.ln_Pri = 0;
    AddSemaphore(&st->ss); /* stays for good: it owns the loaded handler */
    return st;
}

static int con_on(const char *file)
{
    upc_state *st;
    struct DeviceNode *d[2];
    struct DosList *dl;
    char why[120];
    int i, bad = -1;
    if (DOSBase->dl_lib.lib_Version >= 47) {
        printf("UPConsole: not switched: this is AmigaOS 3.2 (dos.library %d), whose Shell needs the\n"
               "console's medium mode (SetMode 2); UP-Term does not have it yet.\n",
               (int)DOSBase->dl_lib.lib_Version);
        return RETURN_WARN;
    }
    st = make_state();
    if (!st) {
        printf("UPConsole: no memory\n");
        return RETURN_FAIL;
    }
    ObtainSemaphore(&st->ss);
    if (st->con_on) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: CON: is UP-Term already\n");
        return RETURN_OK;
    }
    if (!st->seg)
        st->seg = LoadSeg((STRPTR)file); /* before the DosList lock: it does DOS I/O */
    if (!st->seg) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: cannot load %s\n", file);
        return RETURN_FAIL;
    }
    why[0] = 0;
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    for (i = 0; i < 2 && bad < 0; i++) {
        d[i] = find_entry(dl, entry_name[i]);
        if (!d[i]) {
            strcpy(why, "missing");
            bad = i;
        } else if (!pristine(d[i], i, why, sizeof(why))) {
            bad = i;
        }
    }
    if (bad < 0)
        for (i = 0; i < 2; i++) {
            saved_entry *s = &st->saved[i];
            s->seglist = d[i]->dn_SegList;
            s->handler = d[i]->dn_Handler;
            s->stack = d[i]->dn_StackSize;
            s->pri = d[i]->dn_Priority;
            s->globvec = d[i]->dn_GlobalVec;
            s->startup = d[i]->dn_Startup;
            d[i]->dn_SegList = st->seg;
            d[i]->dn_StackSize = 16000;
            d[i]->dn_GlobalVec = -1;
        }
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    if (bad >= 0) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: not switched: %s: is %s.\n"
               "Remove that console replacement first; UP-Term does not stack on another.\n",
               entry_name[bad], why);
        return RETURN_WARN;
    }
    st->con_on = 1;
    ReleaseSemaphore(&st->ss);
    printf("UPConsole: new CON: and RAW: windows are UP-Term\n");
    return RETURN_OK;
}

static int con_off(void)
{
    upc_state *st = find_state();
    struct DosList *dl;
    int i, changed = 0;
    if (!st) {
        printf("UPConsole: CON: is not UP-Term\n");
        return RETURN_OK;
    }
    ObtainSemaphore(&st->ss);
    if (!st->con_on) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: CON: is not UP-Term\n");
        return RETURN_OK;
    }
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    for (i = 0; i < 2; i++) {
        struct DeviceNode *d = find_entry(dl, entry_name[i]);
        saved_entry *s = &st->saved[i];
        if (!d || d->dn_SegList != st->seg) {
            changed |= 1 << i; /* someone else changed it since: leave it */
            continue;
        }
        d->dn_SegList = s->seglist;
        d->dn_Handler = s->handler;
        d->dn_StackSize = s->stack;
        d->dn_Priority = s->pri;
        d->dn_GlobalVec = s->globvec;
        d->dn_Startup = s->startup;
    }
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    st->con_on = 0; /* the handler stays loaded: open windows may run it */
    ReleaseSemaphore(&st->ss);
    for (i = 0; i < 2; i++)
        if (changed & (1 << i))
            printf("UPConsole: %s: was changed by another program since CON ON; left as it is\n",
                   entry_name[i]);
    printf("UPConsole: new CON: and RAW: windows are the ROM's again\n");
    return changed ? RETURN_WARN : RETURN_OK;
}

static void status(void)
{
    upc_state *st = find_state();
    struct DosList *dl;
    char h[2][80], why[120];
    int i, ours[2], rom[2];
    dl = LockDosList(LDF_DEVICES | LDF_READ);
    for (i = 0; i < 2; i++) {
        struct DeviceNode *d = find_entry(dl, entry_name[i]);
        h[i][0] = 0;
        ours[i] = d && st && st->seg && d->dn_SegList == st->seg;
        rom[i] = d && !ours[i] && pristine(d, i, why, sizeof(why));
        if (d && !ours[i] && !rom[i])
            bstr_copy(h[i], sizeof(h[i]), d->dn_Handler);
    }
    UnLockDosList(LDF_DEVICES | LDF_READ);
    for (i = 0; i < 2; i++)
        printf("%s: %s%s\n", entry_name[i], ours[i] ? "UP-Term" : rom[i] ? "ROM" : "other",
               !ours[i] && !rom[i] && h[i][0] ? (sprintf(why, " (%s)", h[i]), why) : "");
    printf("console.device: ROM (the UP-Term device is not built yet)\n");
}

int main(void)
{
    static const char tmpl[] = "CON/K,DEVICE/K,EXCLUDE/K,STATUS/S,HANDLER/K";
    LONG args[5] = { 0, 0, 0, 0, 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)tmpl, args, 0);
    int rc = RETURN_OK;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"UPConsole");
        return RETURN_FAIL;
    }
    if (!vers[0])
        rc = RETURN_FAIL; /* keeps the version string linked in */
    if (args[1] || args[2]) {
        printf("UPConsole: DEVICE and EXCLUDE come with the console.device replacement (not built yet)\n");
        rc = RETURN_WARN;
    }
    if (args[0]) {
        const char *v = (const char *)args[0];
        if (word_is(v, "ON"))
            rc = con_on(args[4] ? (const char *)args[4] : "L:vtcon-handler");
        else if (word_is(v, "OFF"))
            rc = con_off();
        else {
            printf("UPConsole: CON takes ON or OFF\n");
            rc = RETURN_ERROR;
        }
    }
    if (args[3] || (!args[0] && !args[1] && !args[2]))
        status();
    FreeArgs(rd);
    return rc;
}
